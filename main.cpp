#include "AudioSystem.h"

#include <chrono>
#include <thread>

int main()
{
    AudioSystem::Initialize();
    auto s1 = AudioSystem::Load("testSample.wav");
    auto s2 = AudioSystem::Load("testSample1.wav");
    
    auto c1 = AudioSystem::Clip(s1);
    AudioSystem::SetVolume(c1, 1);
    AudioSystem::SetLoop(c1, true);
    AudioSystem::SetPan(c1, 0);
    AudioSystem::Play(c1);

    auto c2 = AudioSystem::Clip(s2);
    AudioSystem::SetVolume(c2, 1);
    AudioSystem::SetLoop(c2, true);
    AudioSystem::SetPan(c2, 0);
    AudioSystem::Play(c2);


    while (1)
    {
    }

    AudioSystem::Stop(c1);
    AudioSystem::Stop(c2);

    AudioSystem::Free(s1);
    AudioSystem::Free(s2);
    
    AudioSystem::Terminate();
    return 0;
}