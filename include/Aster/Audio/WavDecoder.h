#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Aster
{
    struct AudioClip
    {
        uint32_t SampleRate = 0;
        uint16_t Channels = 0;
        std::vector<float> Samples;

        [[nodiscard]] bool IsValid() const
        {
            return SampleRate > 0 && Channels > 0 && !Samples.empty();
        }
    };

    // Decodes RIFF/WAVE PCM and IEEE-float files into interleaved float samples. The engine keeps
    // asset files encoded and decodes them only when a sound is submitted to the audio device.
    class WavDecoder
    {
    public:
        [[nodiscard]] static bool Decode(const void* data, size_t size, AudioClip& clip,
            std::string* error = nullptr);
    };
}
