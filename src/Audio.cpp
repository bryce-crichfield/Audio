#include "Audio.h"
#include "Log.h"

#include <portaudio.h>
#include <sndfile.hh>

#include <filesystem>
#include <iostream>
#include <queue>
#include <string>
#include <utility>
#include <vector>
/* -------------------------------------------------------------------------- */
/*               Forward Declarations of Internal Implementation              */
/* -------------------------------------------------------------------------- */
namespace Audio {

// Mapped by an Audio::Sample identifier, and represents the data for that id.
struct SampleData {
    float* data = nullptr;
    unsigned int length = 0;
    bool mono = false;

    // Reset this instance to defaults.  Since this data is pooled, we don't want
    // to delete the data, just reset the values.
    void Reset();
};

// The possible states of a clip.
// Playing - The clip is currently playing, and will be fed to the audio thread.
// Paused - The clip is currently paused, but can be resumed and is considered
//          allocated.  It may be resumed from the current index.
// Complete - The clip has finished playing, and is considered deallocated.  It
//           may not be resumed, will not feed to the audio thread, and will be
//           reset (deallocated) on the next flush.
enum class ClipState {
    Playing,
    Paused,
    Complete
};

// Mapped by an Audio::Clip identifier, and represents the data for that id.
struct ClipData {
    SampleData* sample = nullptr;
    ClipState state = ClipState::Paused;
    uint32_t sampleIndex = 0;
    float volume = 1.0f;
    float pan = 0.0f;
    unsigned int loopCount = 0;

    // Advances the sample index by one, and will loop if necessary.
    // If the index runs over, the clip will be marked as complete.
    void IncrementSampleIndex();

    // Returns the next sample value for this clip and advances the index.
    // If the clip is paused, complete, or has no sample, then 0.0f will be returned.
    float Next();

    // Returns the next stereo sample value for this clip and advances the index.
    // If the clip is paused, complete, or has no sample, then 0.0f will be returned.
    std::pair<float, float> NextStereo();

    // Reset this instance to defaults.  Since this data is pooled, we don't want
    // to delete the data, just reset the values.
    void Reset();
};

// Houses all the global data for the audio system, including the audio stream,
// object pools, and id pools.
struct GlobalData {
    Properties properties;

    // ID Pools - Exist for the lifetime of the program.
    // IDs are used to map to the object pools.  IDs are
    // recycled when an object is destroyed.
    std::queue<Sample> availableSampleIds;
    std::queue<Clip> availableClipIds;

    // Object Pools - Exist for the lifetime of the program.
    // Data is never deleted from these pools, rather it
    // is reset and reused when an object is destroyed
    std::vector<SampleData> sampleData;
    std::vector<ClipData> clipData;

    // PortAudio State
    PaStream* stream = nullptr;
    PaStreamParameters outputParameters;

    // Error State
    std::string errorString;
};

// The audio callback function. `userData` is a pointer to the global data.
int PortAudioCallback(const void* inputBuffer, void* outputBuffer, unsigned long framesPerBuffer,
    const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void* userData);

// The instance of the global data.
GlobalData* gData = nullptr;

} // namespace Audio

/* -------------------------------------------------------------------------- */
/*                               Implementation                               */
/* -------------------------------------------------------------------------- */
namespace Audio {
/* -------------------------------------------------------------------------- */
void SampleData::Reset()
{
    if (data != nullptr) {
        delete[] data;
    }
    data = nullptr;
    length = 0;
    mono = false;
}
/* -------------------------------------------------------------------------- */
void ClipData::IncrementSampleIndex()
{
    if (sample == nullptr) {
        return;
    }
    sampleIndex += 1;
    if (sampleIndex >= sample->length) {
        if (loopCount == 0) {
            state = ClipState::Complete;
        }
        else {
            sampleIndex = 0;
            if (loopCount != -1) {
                loopCount -= 1;
            }
        }
    }
}
/* -------------------------------------------------------------------------- */
inline float ClipData::Next()
{
    // If the sample is not bound, or the clip is paused, or the clip is complete
    // then return 0.0f

    // This check seems to be necessary.  Reset sets the sample to nullptr, but
    // the audio thread still tries to access it.  Therefore, we need to check
    // and provide a default value.
    if (sample == nullptr || state == ClipState::Paused || state == ClipState::Complete) {
        return 0.0f;
    }
    auto sampleValue = sample->data[sampleIndex];
    IncrementSampleIndex();
    return sampleValue * volume;
}
/* -------------------------------------------------------------------------- */
inline std::pair<float, float> ClipData::NextStereo()
{
    if (sample == nullptr) {
        return std::make_pair(0.0f, 0.0f);
    }

    if (sample->mono) {
        auto value = Next();
        return std::make_pair(value, value);
    }

    auto left = Next() * (1.0f - pan);
    auto right = Next() * (1.0f + pan);
    return std::make_pair(left, right);
}
/* -------------------------------------------------------------------------- */
void ClipData::Reset()
{
    sample = nullptr;
    sampleIndex = 0;
    state = ClipState::Paused;
    volume = 1.0f;
    pan = 0.0f;
    loopCount = 0;
}
/* -------------------------------------------------------------------------- */
bool Initialize(const Properties& properties)
{
    LOG_INFO("Initializing Audio System")

    if (gData != nullptr) {
        LOG_ERROR("Audio System already initialized")
        return false;
    }

    gData = new GlobalData();
    gData->properties = properties;

    // Initialize Sample ID Pool
    for (unsigned int i = 1; i < properties.maxSampleCount + 1; ++i) {
        gData->availableSampleIds.push(i);
    }
    // Initialize Clip ID Pool
    for (unsigned int i = 1; i < properties.maxClipCount + 1; ++i) {
        gData->availableClipIds.push(i);
    }

    // Initialize Sample Data Pool
    // HACK ALERT - The first sample will never be used as Id 0 is invalid.
    gData->sampleData.resize(properties.maxSampleCount + 1);
    // Initialize Clip Data Pool
    // HACK ALERT - The first clip will never be used as Id 0 is invalid.
    gData->clipData.resize(properties.maxClipCount + 1);

    // Intialize PortAudio, return false if there is an error
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        gData->errorString = Pa_GetErrorText(err);
        LOG_ERROR("PortAudio Error: " << gData->errorString)
        return false;
    }

    // Define the parameters of the output stream, return false if there is an error
    gData->outputParameters.device = Pa_GetDefaultOutputDevice();
    if (gData->outputParameters.device == paNoDevice) {
        gData->errorString = "No default output device.";
        LOG_ERROR("PortAudio Error: " << gData->errorString)
        return false;
    }
    gData->outputParameters.channelCount = 2;
    gData->outputParameters.sampleFormat = paFloat32;
    gData->outputParameters.suggestedLatency =
        Pa_GetDeviceInfo(gData->outputParameters.device)->defaultLowOutputLatency;
    gData->outputParameters.hostApiSpecificStreamInfo = nullptr;

    // Open the stream, return false if there is an error
    err = Pa_OpenStream(&gData->stream, nullptr, &gData->outputParameters, properties.sampleRate,
        properties.bufferSize, paNoFlag, PortAudioCallback, gData);
    if (err != paNoError) {
        gData->errorString = Pa_GetErrorText(err);
        LOG_ERROR("PortAudio Error: " << gData->errorString)
        return false;
    }

    // Start the stream, return false if there is an error
    err = Pa_StartStream(gData->stream);
    if (err != paNoError) {
        gData->errorString = Pa_GetErrorText(err);
        LOG_ERROR("PortAudio Error: " << gData->errorString)
        return false;
    }

    LOG_SUCCESS("Audio System Initialized")
    return true;
}
/* -------------------------------------------------------------------------- */
void Terminate()
{
    LOG_INFO("Terminating Audio System")
    // Stop the stream, return false if there is an error
    PaError err = Pa_StopStream(gData->stream);
    if (err != paNoError) {
        gData->errorString = Pa_GetErrorText(err);
        LOG_ERROR("PortAudio Error: " << gData->errorString)
    }

    // Close the stream, return false if there is an error
    err = Pa_CloseStream(gData->stream);
    if (err != paNoError) {
        gData->errorString = Pa_GetErrorText(err);
        LOG_ERROR("PortAudio Error: " << gData->errorString)
    }

    // Terminate PortAudio, return false if there is an error
    err = Pa_Terminate();
    if (err != paNoError) {
        gData->errorString = Pa_GetErrorText(err);
        LOG_ERROR("PortAudio Error: " << gData->errorString)
    }

    // Destroy Sample Data Pool
    gData->sampleData.clear();
    // Destroy Clip Data Pool
    gData->clipData.clear();

    // Destroy Sample ID Pool
    while (!gData->availableSampleIds.empty()) {
        gData->availableSampleIds.pop();
    }
    // Destroy Clip ID Pool
    while (!gData->availableClipIds.empty()) {
        gData->availableClipIds.pop();
    }

    delete gData;

    LOG_SUCCESS("Audio System Terminated")
}
/* -------------------------------------------------------------------------- */
int PortAudioCallback(const void* inputBuffer, void* outputBuffer, unsigned long framesPerBuffer,
    const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void* userData)
{
    GlobalData* globalData = (GlobalData*)userData;

    (void)inputBuffer;
    (void)timeInfo;
    (void)statusFlags;

    // Clear the output buffer with silence
    for (unsigned int i = 0; i < framesPerBuffer * 2; ++i) {
        ((float*)outputBuffer)[i] = 0.0f;
    }

    for (auto& clip : globalData->clipData) {
        if (clip.state != ClipState::Playing) {
            continue;
        }

        float* out = (float*)outputBuffer;
        for (unsigned int i = 0; i < framesPerBuffer; ++i) {
            std::pair<float, float> value = clip.NextStereo();
            *out++ += value.first;
            *out++ += value.second;
        }
    }

    return paContinue;
}
/* -------------------------------------------------------------------------- */
void Flush()
{
    // HACK - We have to start at 0 because we technically have a clip at index 0
    int i = 0;
    for (auto& clip : gData->clipData) {
        if (clip.state == ClipState::Complete) {
            DestroyClip(i);
        }
        i++;
    }
}
/* -------------------------------------------------------------------------- */
int GetPlayingClipCount()
{
    int count = 0;
    for (auto& clip : gData->clipData) {
        if (clip.state == ClipState::Playing) {
            ++count;
        }
    }
    return count;
}
/* -------------------------------------------------------------------------- */
std::string GetErrorString()
{
    return gData->errorString;
}
/* -------------------------------------------------------------------------- */
Sample CreateSample(const std::string& filename)
{
    LOG_INFO("Loading Sample '" << filename << "'")
    // Ensure that there is an available sample ID
    if (gData->availableSampleIds.empty()) {
        gData->errorString = "Error creating sample '" + filename + "': No available sample IDs.";
        LOG_ERROR("Audio Error: " << gData->errorString)
        return 0;
    }

    // Ensure the path is valid
    if (!std::filesystem::exists(filename)) {
        gData->errorString = "Error loading sample '" + filename + "': File does not exist.";
        LOG_ERROR("Audio Error: " << gData->errorString)
        return 0;
    }

    // Ensure the path is a file
    if (!std::filesystem::is_regular_file(filename)) {
        gData->errorString = "Error loading sample '" + filename + "': Path is not a file.";
        LOG_ERROR("Audio Error: " << gData->errorString)
        return 0;
    }

    // Load the sample
    SndfileHandle file = SndfileHandle(filename.c_str());

    // Ensure the sample was loaded successfully
    if (file.error()) {
        gData->errorString = "Error loading sample '" + filename + "': " + file.strError();
        LOG_ERROR("Audio Error: " << gData->errorString)
        return 0;
    }

    // Ensure the sample is mono or stereo
    if (file.channels() != 1 && file.channels() != 2) {
        gData->errorString =
            "Error loading sample '" + filename + "': Sample must be mono or stereo.";
        LOG_ERROR("Audio Error: " << gData->errorString)
        return 0;
    }

    // Ensure the sample is 44100 Hz
    if (file.samplerate() != 44100) {
        gData->errorString = "Error loading sample '" + filename + "': Sample must be 44100 Hz.";
        LOG_ERROR("Audio Error: " << gData->errorString)
        return 0;
    }

    // Allocate a new sample ID
    auto sampleId = gData->availableSampleIds.front();
    gData->availableSampleIds.pop();

    // Get the sample and make sure it's reset and clean
    // The reset should happen on destruction, but just in case
    SampleData& sample = gData->sampleData[sampleId];
    sample.Reset();

    sample.data = new float[file.frames() * file.channels()];
    sample.length = file.frames() * file.channels();
    sample.mono = file.channels() == 1;
    file.read(sample.data, sample.length);

    LOG_SUCCESS("Loaded sample '" << filename << "'")

    return sampleId;
}
/* -------------------------------------------------------------------------- */
void DestroySample(Sample sample)
{
    // Ensure that the sample is valid
    if (!sample) {
        gData->errorString = "Error destroying sample: Invalid sample.";
        LOG_ERROR("Audio Error: " << gData->errorString)
        return;
    }

    // Reset the reference sample data in the pool to default values
    SampleData& sampleData = gData->sampleData[sample];
    sampleData.Reset();

    // Add the sample ID back to the pool
    gData->availableSampleIds.push(sample);
}
/* -------------------------------------------------------------------------- */
Clip PlaySample(Sample sample)
{
    // Ensure the sample is valid
    if (!sample) {
        gData->errorString = "Error playing sample: Invalid sample.";
        LOG_ERROR("Audio Error: " << gData->errorString)
        return 0;
    }

    // Create a new clip
    Clip clip = CreateClip(sample);
    if (!clip) {
        gData->errorString = "Error playing sample: " + gData->errorString;
        LOG_ERROR("Audio Error: " << gData->errorString)
        return 0;
    }

    // Play the clip
    Play(clip);

    return clip;
}

/* -------------------------------------------------------------------------- */
Clip CreateClip(Sample sample)
{
    // Ensure the sample is valid
    if (!sample) {
        gData->errorString = "Error creating clip: Invalid sample.";
        LOG_ERROR("Audio Error: " << gData->errorString)
        return 0;
    }

    // Ensure that there is an available clip ID
    if (gData->availableClipIds.empty()) {
        gData->errorString = "Error creating clip: No available clip IDs.";
        LOG_ERROR("Audio Error: " << gData->errorString)
        return 0;
    }

    // Allocate a new clip ID
    auto clipId = gData->availableClipIds.front();
    gData->availableClipIds.pop();

    // Set up a new clip data struct and make sure it's reset and clean
    ClipData& clip = gData->clipData[clipId];
    clip.Reset();

    // Set the clip's sample data
    // Even if the sample is invalid, we still want to set the clip's sample data
    // to the sample data in the pool so that we can safely call DestroyClip()
    SampleData& sampleData = gData->sampleData[sample];
    clip.sample = &sampleData;

    return clipId;
}
/* -------------------------------------------------------------------------- */
void DestroyClip(Clip clip)
{
    // Ensure that the clip is valid
    if (!clip) return;

    // Get the clip data from the pool and reset it to default values
    ClipData& clipData = gData->clipData[clip];
    clipData.Reset();

    // Add the clip ID back to the pool
    gData->availableClipIds.push(clip);
}
/* -------------------------------------------------------------------------- */
void Play(Clip clip)
{
    // Ensure that the clip is valid
    if (!clip) return;

    // Get the clip data from the pool and set it to playing
    ClipData& clipData = gData->clipData[clip];
    clipData.state = ClipState::Playing;
}
/* -------------------------------------------------------------------------- */
void Pause(Clip clip)
{
    // Ensure that the clip is valid
    if (!clip) return;

    // Get the clip data from the pool and set it to paused
    ClipData& clipData = gData->clipData[clip];
    clipData.state = ClipState::Paused;
}
/* -------------------------------------------------------------------------- */
float GetClipVolume(Clip clip)
{
    // Ensure that the clip is valid
    if (!clip) return 0.0f;

    // Get the clip data from the pool and return the volume
    ClipData& clipData = gData->clipData[clip];
    return clipData.volume;
}
/* -------------------------------------------------------------------------- */
float GetClipPan(Clip clip)
{
    // Ensure that the clip is valid
    if (!clip) return 0.0f;

    // Get the clip data from the pool and return the pan
    ClipData& clipData = gData->clipData[clip];
    return clipData.pan;
}
/* -------------------------------------------------------------------------- */
int GetClipLoop(Clip clip)
{
    // Ensure that the clip is valid
    if (!clip) return 0;

    // Get the clip data from the pool and return the loop
    ClipData& clipData = gData->clipData[clip];
    return clipData.loopCount;
}
/* -------------------------------------------------------------------------- */
float GetClipPosition(Clip clip)
{
    // Ensure that the clip is valid
    if (!clip) return 0.0f;

    // Get the clip data from the pool and return the position
    ClipData& clipData = gData->clipData[clip];

    // Ensure that the clip has an associated sample
    if (!clipData.sample) return 0.0f;

    return (float)clipData.sampleIndex / clipData.sample->length;
}
/* -------------------------------------------------------------------------- */
void SetClipVolume(Clip clip, float volume)
{
    // Ensure that the clip is valid
    if (!clip) return;

    // Get the clip data from the pool and set the volume
    ClipData& clipData = gData->clipData[clip];
    clipData.volume = volume;
}
/* -------------------------------------------------------------------------- */
void SetClipPan(Clip clip, float pan)
{
    // Ensure that the clip is valid
    if (!clip) return;

    // Get the clip data from the pool and set the pan
    ClipData& clipData = gData->clipData[clip];
    clipData.pan = pan;
}
/* -------------------------------------------------------------------------- */
void SetClipLoop(Clip clip, int count)
{
    // Ensure that the clip is valid
    if (!clip) return;

    // Get the clip data from the pool and set the loop count
    ClipData& clipData = gData->clipData[clip];
    clipData.loopCount = count;
}
/* -------------------------------------------------------------------------- */
void SetClipPosition(Clip clip, float position)
{
    // Ensure that the clip is valid
    if (!clip) return;

    // Get the clip data from the pool and set the position
    ClipData& clipData = gData->clipData[clip];

    // Ensure that the clip has an associated sample
    if (!clipData.sample) return;

    // Scale the position to the sample's length
    // TODO: Verify this doesn't cause any issues
    auto index = static_cast<int>(position * clipData.sample->length);
    clipData.sampleIndex = index;
}
/* -------------------------------------------------------------------------- */
bool IsClipPlaying(Clip clip)
{
    // Ensure that the clip is valid
    if (!clip) return false;

    // Get the clip data from the pool and return the state
    ClipData& clipData = gData->clipData[clip];
    return clipData.state == ClipState::Playing;
}
/* -------------------------------------------------------------------------- */
}; // namespace Audio