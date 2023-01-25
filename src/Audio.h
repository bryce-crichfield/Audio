#pragma once

#include <string>

namespace Audio
{
/* -------------------------------------------------------------------------- */
/*                              System Properites                             */
/* -------------------------------------------------------------------------- */

// The audio system properties.
// Buffer Size: The size of the audio buffer in samples. This is the number of
//              samples that will be processed per audio thread update.
// Sample Rate: The sample rate of the audio system. This is the number of
//              samples per second.
// Max Sample Count: The maximum number of samples that can be allocated.
// Max Clip Count: The maximum number of clips that can be allocated.
struct Properties
{
    uint32_t bufferSize;
    uint32_t sampleRate;
    uint32_t maxSampleCount;
    uint32_t maxClipCount;
};

/* -------------------------------------------------------------------------- */
/*                              Global Functions                              */
/* -------------------------------------------------------------------------- */

// Initialize the audio system, constructing the audio thread, and allocating
// the audio pools. Returns true if successful, false otherwise.
bool Initialize(const Properties &properties);

// Terminate the audio system, destroying the audio thread, and freeing the
// audio pools.
void Terminate();

// Flush the audio system, reseting any completed clips from the audio pools.
void Flush();

// Get the number of clips currently playing.
int GetPlayingClipCount();

// Get the last error string.
std::string GetErrorString();
/* -------------------------------------------------------------------------- */
/*                                Audio Samples                               */
/* -------------------------------------------------------------------------- */

// An Id representing an allocated audio sample.  
using Sample = unsigned int;

// Create a sample from the given filename. Returns the sample id if successful,
// 0 otherwise.
Sample CreateSample(const std::string &filename);

// Destroy the given sample, freeing any allocated memory.
void DestroySample(Sample sample);

void LowpassFilter(Sample sample, float cutoff);

/* -------------------------------------------------------------------------- */
/*                                 Audio Clips                                */
/* -------------------------------------------------------------------------- */

// An Id representing an allocated audio clip.
using Clip = unsigned int;

// Create a clip from the given sample. Returns the clip id if successful, 0
// otherwise.
Clip CreateClip(Sample sample);

// Destroy the given clip, ending playback and freeing any allocated memory.
void DestroyClip(Clip clip);

// Play the given clip. If the clip is already playing, it will be restarted.
// If the clip is paused, it will be resumed.
void Play(Clip clip);

// Pause the given clip. 
void Pause(Clip clip);

// Set the volume of the given clip. The volume should be in the range [0, 1].
void SetClipVolume(Clip clip, float volume);

// Set the pan of the given clip. The pan should be in the range [-1, 1].
void SetClipPan(Clip clip, float pan);

// Set the loop count of the given clip. The loop count will be applied
// upon playback.  It is not guaranteed that the clip will loop the exact
// number of times specified if the clip is currently playing. 
void SetClipLoop(Clip clip, int count);

/* -------------------------------------------------------------------------- */
}; // namespace Audio