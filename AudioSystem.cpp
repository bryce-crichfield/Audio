#include "AudioSystem.h"
#include "libs/inc/portaudio.h"
#include "libs/inc/sndfile.hh"

#include <iostream>
#include <queue>
#include <vector>

#define MAX_SAMPLE_COUNT 2048
#define MAX_CLIP_COUNT 2048
namespace AudioSystem
{

struct AudioSampleData
{
    float *data;
    uint32_t size;

    AudioSampleData() : data(nullptr), size(0)
    {
    }

    ~AudioSampleData()
    {
        delete[] data;
    }
};

struct AudioClipData
{
    AudioSampleData *sample;
    uint32_t position;
    bool loop;
    float volume;
    float pan;
    bool paused = false;
    bool complete = false;

    AudioClipData() : sample(nullptr), position(0), loop(false), volume(1.0f), pan(0.0f)
    {
    }

    ~AudioClipData()
    {
    }

    // TODO: Implement
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
PaStreamParameters outputParameters;
PaStream *stream;

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

            // Apply Constant Power Panning
            auto pan = clip->pan;
            auto l = clip->Next() * (1.0f - pan) * 0.707f;
            auto r = clip->Next() * (1.0f + pan) * 0.707f;

            // Accumulate the sample into the output buffer
            left += l;
            right += r;
            // Clip the output value between -1.0f and 1.0f
            left > 1.0f ? left = 1.0f : left < -1.0f ? left = -1.0f : left;
            right > 1.0f ? right = 1.0f : right < -1.0f ? right = -1.0f : right;
        }
    }

    return paContinue;
}

void Initialize()
{
    // Fill the available sample IDs
    for (uint32_t i = 0; i < MAX_SAMPLE_COUNT; i++)
    {
        availableSampleIds.push(i);
    }

    // Fill the available clip IDs
    for (uint32_t i = 0; i < MAX_CLIP_COUNT; i++)
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
        std::cout << "Error initializing PortAudio: " << Pa_GetErrorText(err) << std::endl;
        return;
    }

    // Open the default output stream
    outputParameters.device = Pa_GetDefaultOutputDevice();
    if (outputParameters.device == paNoDevice)
    {
        std::cout << "Error: No default output device." << std::endl;
        return;
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
        std::cout << "Pa_OpenStream() PortAudio Error: " << Pa_GetErrorText(err) << std::endl;
    }

    // Start the stream and begin the audio callback
    err = Pa_StartStream(stream);
    if (err != paNoError)
    {
        std::cout << "Pa_StartStream() PortAudio Error: " << Pa_GetErrorText(err) << std::endl;
    }
}

void Terminate()
{
    // Stop the stream
    PaError err = Pa_StopStream(stream);
    if (err != paNoError)
    {
        std::cout << "Pa_StopStream() PortAudio Error: " << Pa_GetErrorText(err) << std::endl;
    }

    // Close the stream
    err = Pa_CloseStream(stream);
    if (err != paNoError)
    {
        std::cout << "Pa_CloseStream() PortAudio Error: " << Pa_GetErrorText(err) << std::endl;
    }

    // Terminate PortAudio
    Pa_Terminate();

    // Delete all the playing clips
    for (auto &clip : playingClips)
    {
        delete clip;
    }

    // Delete all the loaded samples
    for (auto &sample : loadedSamples)
    {
        delete sample;
    }
}

AudioSample Load(const std::string &path)
{
    std::cout << "Loading audio file: " << path << std::endl;

    SndfileHandle file = SndfileHandle(path.c_str());
    AudioSampleData *sample = new AudioSampleData();
    sample->data = new float[file.frames() * file.channels()];
    sample->size = file.frames() * file.channels();
    file.read(sample->data, sample->size);

    std::cout << "Loaded audio file: " << path << std::endl;

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
    if (playingClips[clip] == nullptr)
    {
        return;
    }
    playingClips[clip]->paused = false;
}

bool IsPlaying(AudioClip clip)
{
    if (playingClips[clip] == nullptr)
    {
        return false;
    }
    return !playingClips[clip]->complete && !playingClips[clip]->paused;
}

// Stops the specified clip, and frees it.
void Stop(AudioClip clip)
{
    // Free the clip data
    delete playingClips[clip];
    playingClips[clip] = nullptr;

    // Add the clip ID to the available queue
    availableClipsIds.push(clip);
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