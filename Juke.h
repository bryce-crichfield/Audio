#pragma once

#include <cstdint>
#include <string>

// A simple jukebox audio player that can load and play audio files.
namespace Juke
{

// Acts as a handle to an audio sample.
using AudioSample = uint32_t;

// Acts as a handle to an audio clip, which is a flyweight
// instance of an audio sample.
using AudioClip = uint32_t;

// Initializes the audio system, and loads the audio device.
bool Initialize();

// Terminates the audio system, and frees all resources.
bool Terminate();

// Returns the error message from the last failed operation.
const std::string &GetError();

// Loads an audio sample from the specified path, and returns the sample ID.
// Returns 0 if the sample could not be loaded.
AudioSample Load(const std::string& path);

// Frees the specified sample if it is allocated.
// Stops and frees any clips that are currently playing from the sample.
void Free(AudioSample sample);

// Stops and frees all clips and samples that are currently playing.
void Reset();

// Launches a new audio clip from the specified sample.
// Returns the clip ID, or 0 if the clip could not be launched.
AudioClip Clip(AudioSample sample);

// Plays the specified clip.
void Play(AudioClip clip);

// Returns true if the specified clip is playing
// Ie not paused, looping or otherwise not stopped.
bool IsPlaying(AudioClip clip);

// Cleans up any clips that are no longer playing,
// and returns true if any clips are still playing.
bool Flush();

// Stops the specified clip, and frees it.
void Stop(AudioClip clip);

// Sets the volume of the specified clip.
void SetVolume(AudioClip clip, float volume);

// Sets the pan of the specified clip.
void SetPan(AudioClip clip, float pan);

// Sets the looping of the specified clip.
void SetLoop(AudioClip clip, bool loop);

} // namespace AudioSystem