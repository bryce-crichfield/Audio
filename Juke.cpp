#include "Juke.h"
#include "libs/inc/portaudio.h"
#include "libs/inc/sndfile.hh"

#include <filesystem>
#include <iostream>
#include <queue>
#include <vector>
#include <set>

#define MAX_SAMPLE_COUNT 2048
#define MAX_CLIP_COUNT 2048
namespace Juke
{

struct AudioSampleData
{
    float *data = nullptr;
    uint32_t size = 0;
    bool stereo = true;

    ~AudioSampleData()
    {
        delete[] data;
    }
};

struct AudioClipData
{
    AudioSampleData *sample = nullptr;
    uint32_t position = 0;
    bool loop = false;
    float volume = 1.0f;
    float pan = 0.0f;
    bool paused = false;
    bool complete = false;

    float Next()
    {
        if (position > sample->size)
        {
            if (loop)
            {
                position = 0;
            }
            else
            {
                complete = true;
                return 0.0f;
            }
        }
        auto value = sample->data[position++];
        return value * volume;
    }
};

// AUDIO SYSTEM DATA
std::queue<AudioSample> availableSampleIds;
std::queue<AudioClip> availableClipsIds;
std::vector<AudioSampleData *> loadedSamples;
std::vector<AudioClipData *> playingClips;
// NOTE: this feels like a bit of a hack, but it seems to be the only way to
// ensure that the clips are correctly freed after they are no longer in use.
std::set<AudioClipData *> clipsToFree;
uint32_t clipsInFlight = 0;
PaStreamParameters outputParameters;
PaStream *stream;
std::string error;

static int paCallback(const void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer,
                      const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags, void *userData)
{
    // This shouldn't happen, but just in case
    if (userData == nullptr)
    {
        std::cerr << "PortAudio Error: No user data." << std::endl;
        return paAbort;
    }

    auto &clips = *(std::vector<AudioClipData *> *)userData;
    auto *out = (float *)outputBuffer;

    // Fill with silence
    float *write = out;
    for (int i = 0; i < framesPerBuffer; i++)
    {
        *write++ = 0.0f;
        *write++ = 0.0f;
    }

    for (auto *clip : clips)
    {
        if (clip == nullptr || clip->paused)
        {
            continue;
        }

        float *write = out;
        for (int i = 0; i < framesPerBuffer; i++)
        {
            // Get the left and right channels
            auto &left = *write++;
            auto &right = *write++;

            // Get the next sample from the clip
            // and enforce mono compatibility
            float l, r;
            if (clip->sample->stereo)
            {
                l = clip->Next();
                r = clip->Next();
            }
            else
            {
                auto value = clip->Next();
                l = value;
                r = value;
            }

            // Apply Constant Power Panning
            auto pan = clip->pan;
            left += (l * (1.0f - pan) * 0.707f);
            right += (r * (1.0f + pan) * 0.707f);

            // Clip the output value between -1.0f and 1.0f
            left > 1.0f ? left = 1.0f : left < -1.0f ? left = -1.0f : left;
            right > 1.0f ? right = 1.0f : right < -1.0f ? right = -1.0f : right;
        }
    }

    return paContinue;
}

bool Initialize()
{
    // Fill the available sample IDs
    for (uint32_t i = 1; i < MAX_SAMPLE_COUNT; i++)
    {
        availableSampleIds.push(i);
    }

    // Fill the available clip IDs
    for (uint32_t i = 1; i < MAX_CLIP_COUNT; i++)
    {
        availableClipsIds.push(i);
    }

    // Initialize the loaded samples array
    loadedSamples.resize(MAX_SAMPLE_COUNT, nullptr);

    // Initialize the playing clips array
    playingClips.resize(MAX_CLIP_COUNT, nullptr);

    // Initialize PortAudio
    PaError err = Pa_Initialize();
    if (err != paNoError)
    {
        error = "Error initializing PortAudio: " + std::string(Pa_GetErrorText(err));
        return false;
    }

    // Open the default output stream
    outputParameters.device = Pa_GetDefaultOutputDevice();
    if (outputParameters.device == paNoDevice)
    {
        error = "Error: No default output device.";
        return false;
    }

    // TODO: Make this configurable
    // Configure the output stream
    outputParameters.channelCount = 2;
    outputParameters.sampleFormat = paFloat32;
    outputParameters.suggestedLatency = Pa_GetDeviceInfo(outputParameters.device)->defaultLowOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = nullptr;

    // Open the stream
    err = Pa_OpenStream(&stream, NULL, &outputParameters, 44100, 256, paClipOff, paCallback, &playingClips);
    if (err != paNoError)
    {
        error = "Error opening PortAudio stream: " + std::string(Pa_GetErrorText(err));
        return false;
    }

    // Start the stream and begin the audio callback
    err = Pa_StartStream(stream);
    if (err != paNoError)
    {
        error = "Error starting PortAudio stream: " + std::string(Pa_GetErrorText(err));
        return false;
    }

    return true;
}

bool Terminate()
{
    // Stop the stream
    PaError err = Pa_StopStream(stream);
    if (err != paNoError)
    {
        error = "Error stopping PortAudio stream: " + std::string(Pa_GetErrorText(err));
        return false;
    }

    // Close the stream
    err = Pa_CloseStream(stream);
    if (err != paNoError)
    {
        error = "Error closing PortAudio stream: " + std::string(Pa_GetErrorText(err));
        return false;
    }

    // Terminate PortAudio
    err = Pa_Terminate();
    if (err != paNoError)
    {
        error = "Error terminating PortAudio: " + std::string(Pa_GetErrorText(err));
        return false;
    }

    // Delete all the playing clips
    for (auto &clip : playingClips)
    {
        delete clip;
    }

    // Delete all the waste clips
    for (auto &clip : clipsToFree)
    {
        delete clip;
    }
    clipsToFree.clear();

    // Delete all the loaded samples
    for (auto &sample : loadedSamples)
    {
        delete sample;
    }

    return true;
}

void Reset()
{
    // Stop all the playing clips
    for (int i = 1; i < MAX_CLIP_COUNT; i++)
    {
        if (playingClips[i] != nullptr)
        {
            Stop(i);
        }
    }

    // Delete all the waste clips
    for (auto &clip : clipsToFree)
    {
        delete clip;
    }
    clipsToFree.clear();
    
    // Free all the loaded samples
    for (int i = 1; i < MAX_SAMPLE_COUNT; i++)
    {
        if (loadedSamples[i] != nullptr)
        {
            Free(i);
        }
    }
}

const std::string &GetError()
{
    return error;
}

AudioSample Load(const std::string &path)
{
    // Ensure the path is valid
    if (!std::filesystem::exists(path))
    {
        error = "Error loading sample '" + path + "': File does not exist.";
        return 0;
    }

    // Ensure the path is a file
    if (!std::filesystem::is_regular_file(path))
    {
        error = "Error loading sample '" + path + "': Path is not a file.";
        return 0;
    }

    // Load the sample
    SndfileHandle file = SndfileHandle(path.c_str());

    // Ensure the sample was loaded successfully
    if (file.error())
    {
        error = "Error loading sample '" + path + "': " + file.strError();
        return 0;
    }

    // Ensure the sample is mono or stereo
    if (file.channels() != 1 && file.channels() != 2)
    {
        error = "Error loading sample '" + path + "': Sample must be mono or stereo.";
        return 0;
    }

    // Ensure the sample is 32-bit float
    // if (file.format() != SF_FORMAT_FLOAT)
    // {
    //     error = "Error loading sample '" + path + "': Sample must be 32-bit float.";
    //     return 0;
    // }

    // Ensure the sample is 44100 Hz
    if (file.samplerate() != 44100)
    {
        error = "Error loading sample '" + path + "': Sample must be 44100 Hz.";
        return 0;
    }

    // Read the sample data into an AudioSampleData struct
    AudioSampleData *sample = new AudioSampleData();
    sample->data = new float[file.frames() * file.channels()];
    sample->size = file.frames() * file.channels();
    file.read(sample->data, sample->size);

    // Allocate a new sample ID
    auto sampleId = availableSampleIds.front();
    availableSampleIds.pop();

    // Store the sample data
    loadedSamples[sampleId] = sample;

    return sampleId;
}

void Free(AudioSample sample)
{
    // Stop any associated clips
    for (int i = 0; i < playingClips.size(); i++)
    {
        if (playingClips[i] != nullptr && playingClips[i]->sample == loadedSamples[sample])
        {
            Stop(i);
        }
    }

    // Free the sample data
    delete loadedSamples[sample];
    loadedSamples[sample] = nullptr;

    // Add the sample ID to the available queue
    availableSampleIds.push(sample);
}

AudioClip Clip(AudioSample sample)
{
    // Ensure the sample is valid
    if (sample == 0)
    {
        error = "Error clipping sample: Invalid sample ID.";
        return 0;
    }

    // Ensure the sample has been loaded
    if (loadedSamples[sample] == nullptr)
    {
        error = "Error clipping sample: Sample has not been loaded.";
        return 0;
    }

    // Allocate a new clip
    auto clipId = availableClipsIds.front();
    availableClipsIds.pop();

    // Allocate a new clip data
    auto *clip = new AudioClipData();
    clip->sample = loadedSamples[sample];
    clip->loop = false;
    clip->paused = true;

    // Store the clip data
    playingClips[clipId] = clip;

    return clipId;
}

void Play(AudioClip clip)
{
    // Ensure the clip is valid
    if (playingClips[clip] == nullptr)
    {
        return;
    }

    // Reset the clip so that it will play from the beginning
    auto clipData = playingClips[clip];
    clipData->paused = false;
    clipData->complete = false;
    clipData->position = 0;

    clipsInFlight++;
}

bool IsPlaying(AudioClip clip)
{
    // Ensure the clip is valid
    if (playingClips[clip] == nullptr)
    {
        return false;
    }
    return !playingClips[clip]->complete && !playingClips[clip]->paused;
}

bool Flush()
{
    for (int i = 1; i < MAX_CLIP_COUNT; i++)
    {
        if (playingClips[i] != nullptr && playingClips[i]->complete)
        {
            Stop(i);
        }
    }

    for (auto clip : clipsToFree)
    {
        delete clip;
    }
    clipsToFree.clear();

    return clipsInFlight > 0;
}

void Stop(AudioClip clip)
{
    // Ensure the clip is valid
    if (playingClips[clip] == nullptr)
    {
        return;
    }

    // Free the clip data
    // HACK - We can't delete the clip data here because it will be used by the audio thread?
    // I'm assuming this won't always work, especially if the audio thread is running 
    // significantly slower than the main thread?
    clipsToFree.insert(playingClips[clip]);
    playingClips[clip] = nullptr;

    // Add the clip ID back to the available queue
    availableClipsIds.push(clip);

    clipsInFlight--;
}

void SetVolume(AudioClip clip, float volume)
{
    if (playingClips[clip] == nullptr)
    {
        return;
    }
    playingClips[clip]->volume = volume;
}

void SetLoop(AudioClip clip, bool loop)
{
    if (playingClips[clip] == nullptr)
    {
        return;
    }
    playingClips[clip]->loop = loop;
}

void SetPan(AudioClip clip, float pan)
{
    if (playingClips[clip] == nullptr)
    {
        return;
    }
    playingClips[clip]->pan = pan;
}
} // namespace AudioSystem