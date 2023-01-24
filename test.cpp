#include "Juke.h"

#include <chrono>
#include <iostream>
#include <thread>

int main()
{
    auto err = Juke::Initialize();
    if (!err)
    {
        std::cout << Juke::GetError() << std::endl;
        return 1;
    }

    auto s1 = Juke::Load("loop1.wav");
    if (!s1)
    {
        std::cout << Juke::GetError() << std::endl;
        return 1;
    }

    auto s2 = Juke::Load("loop2.wav");
    if (!s2)
    {
        std::cout << Juke::GetError() << std::endl;
        return 1;
    }

    auto c1 = Juke::Clip(s1);
    Juke::SetVolume(c1, 1);
    Juke::SetPan(c1, 1);
    Juke::Play(c1);

    if (!c1)
    {
        std::cout << Juke::GetError() << std::endl;
        return 1;
    }

    auto c2 = Juke::Clip(s2);
    Juke::SetVolume(c2, 1);
    Juke::SetPan(c2, -1);
    Juke::Play(c2);

    while (Juke::Flush())
    {
    }

    // Not needed, but good practice
    Juke::Stop(c1);
    Juke::Stop(c2);

    // Not needed, but good practice
    Juke::Free(s1);
    Juke::Free(s2);

    Juke::Terminate();
    return 0;
}