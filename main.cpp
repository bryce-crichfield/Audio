#include <Audio.h>
#include <iostream>
int main(void)
{
    Audio::Properties properties = {
        .bufferSize = 256,
        .sampleRate = 44100,
        .maxSampleCount = 256,
        .maxClipCount = 512
    };
    Audio::Initialize(properties);

    int i = 10000000000000000;
    while (i--)
    {
        
    }

    auto sample = Audio::CreateSample("../sample1.wav");
    Audio::LowpassFilter(sample, 0.01f);
    auto clip = Audio::CreateClip(sample);
    Audio::Play(clip);
    

    while (Audio::GetPlayingClipCount() > 0)
    {
        // std::cout << Audio::GetPlayingClipCount() << std::endl;
        Audio::Flush();
    }

    Audio::Terminate();
    return 0;
}