#include "AudioSystem.h"

#include <chrono>
#include <iostream>
#include <thread>

int main()
{
    auto err = AudioSystem::Initialize();
    if (!err)
    {
        std::cout << AudioSystem::GetError() << std::endl;
        return 1;
    }

    auto s1 = AudioSystem::Load("loop1.wav");
    if (!s1)
    {
        std::cout << AudioSystem::GetError() << std::endl;
        return 1;
    }

    auto s2 = AudioSystem::Load("loop2.wav");
    if (!s2)
    {
        std::cout << AudioSystem::GetError() << std::endl;
        return 1;
    }

    auto c1 = AudioSystem::Clip(s1);
    AudioSystem::SetVolume(c1, 1);
    AudioSystem::SetPan(c1, 1);
    AudioSystem::Play(c1);

    if (!c1)
    {
        std::cout << AudioSystem::GetError() << std::endl;
        return 1;
    }

    auto c2 = AudioSystem::Clip(s2);
    AudioSystem::SetVolume(c2, 1);
    AudioSystem::SetPan(c2, -1);
    AudioSystem::Play(c2);

    while (AudioSystem::Flush())
    {
    }

    // Not needed, but good practice
    AudioSystem::Stop(c1);
    AudioSystem::Stop(c2);

    // Not needed, but good practice
    AudioSystem::Free(s1);
    AudioSystem::Free(s2);

    AudioSystem::Terminate();
    return 0;
}