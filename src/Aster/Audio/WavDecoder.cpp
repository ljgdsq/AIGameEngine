#include "Aster/Audio/WavDecoder.h"

#include <algorithm>
#include <cstring>

namespace Aster
{
    namespace
    {
        uint16_t ReadU16(const uint8_t* data)
        {
            return static_cast<uint16_t>(data[0] | (static_cast<uint16_t>(data[1]) << 8));
        }

        uint32_t ReadU32(const uint8_t* data)
        {
            return static_cast<uint32_t>(data[0]) | (static_cast<uint32_t>(data[1]) << 8) |
                (static_cast<uint32_t>(data[2]) << 16) | (static_cast<uint32_t>(data[3]) << 24);
        }

        bool Fail(std::string* error, const char* message)
        {
            if (error != nullptr)
            {
                *error = message;
            }
            return false;
        }
    }

    bool WavDecoder::Decode(const void* data, size_t size, AudioClip& clip, std::string* error)
    {
        clip = {};
        if (data == nullptr || size < 12)
        {
            return Fail(error, "WAV data is too small");
        }

        const auto* bytes = static_cast<const uint8_t*>(data);
        if (std::memcmp(bytes, "RIFF", 4) != 0 || std::memcmp(bytes + 8, "WAVE", 4) != 0)
        {
            return Fail(error, "Not a RIFF/WAVE file");
        }

        uint16_t formatTag = 0;
        uint16_t channels = 0;
        uint16_t bitsPerSample = 0;
        uint16_t blockAlign = 0;
        uint32_t sampleRate = 0;
        const uint8_t* sampleData = nullptr;
        size_t sampleDataSize = 0;

        size_t offset = 12;
        while (offset + 8 <= size)
        {
            const uint8_t* chunk = bytes + offset;
            const uint32_t chunkSize = ReadU32(chunk + 4);
            const size_t payload = offset + 8;
            if (payload > size || chunkSize > size - payload)
            {
                return Fail(error, "WAV chunk extends past the end of the file");
            }

            if (std::memcmp(chunk, "fmt ", 4) == 0)
            {
                if (chunkSize < 16)
                {
                    return Fail(error, "WAV fmt chunk is truncated");
                }
                formatTag = ReadU16(bytes + payload);
                channels = ReadU16(bytes + payload + 2);
                sampleRate = ReadU32(bytes + payload + 4);
                blockAlign = ReadU16(bytes + payload + 12);
                bitsPerSample = ReadU16(bytes + payload + 14);
                if (formatTag == 0xFFFE)
                {
                    if (chunkSize < 40)
                    {
                        return Fail(error, "WAV extensible fmt chunk is truncated");
                    }
                    formatTag = ReadU16(bytes + payload + 24);
                }
            }
            else if (std::memcmp(chunk, "data", 4) == 0)
            {
                sampleData = bytes + payload;
                sampleDataSize = chunkSize;
            }

            const size_t padded = static_cast<size_t>(chunkSize) + (chunkSize & 1u);
            if (padded > size - payload)
            {
                break;
            }
            offset = payload + padded;
        }

        if (channels == 0 || channels > 2)
        {
            return Fail(error, "WAV must contain one or two channels");
        }
        if (sampleRate < 1000 || sampleRate > 384000)
        {
            return Fail(error, "WAV sample rate is out of range");
        }
        if (formatTag != 1 && formatTag != 3)
        {
            return Fail(error, "WAV must use PCM or IEEE float samples");
        }
        if (formatTag == 3 && bitsPerSample != 32)
        {
            return Fail(error, "WAV float samples must be 32-bit");
        }
        if (formatTag == 1 && bitsPerSample != 8 && bitsPerSample != 16 && bitsPerSample != 24 &&
            bitsPerSample != 32)
        {
            return Fail(error, "WAV PCM samples must be 8, 16, 24, or 32-bit");
        }
        if (sampleData == nullptr || sampleDataSize == 0)
        {
            return Fail(error, "WAV has no sample data");
        }

        const uint16_t calculatedAlign = static_cast<uint16_t>(channels * (bitsPerSample / 8));
        if (blockAlign == 0)
        {
            blockAlign = calculatedAlign;
        }
        if (blockAlign != calculatedAlign || blockAlign == 0)
        {
            return Fail(error, "WAV block alignment is invalid");
        }

        const size_t frameCount = sampleDataSize / blockAlign;
        if (frameCount == 0)
        {
            return Fail(error, "WAV contains no complete frames");
        }

        clip.SampleRate = sampleRate;
        clip.Channels = channels;
        clip.Samples.resize(frameCount * channels);

        const uint16_t bytesPerSample = static_cast<uint16_t>(bitsPerSample / 8);
        size_t output = 0;
        for (size_t frame = 0; frame < frameCount; ++frame)
        {
            const uint8_t* source = sampleData + frame * blockAlign;
            for (uint16_t channel = 0; channel < channels; ++channel, ++output)
            {
                if (formatTag == 3)
                {
                    float value = 0.0f;
                    std::memcpy(&value, source, sizeof(float));
                    clip.Samples[output] = value;
                }
                else if (bitsPerSample == 8)
                {
                    clip.Samples[output] = (static_cast<float>(source[0]) - 128.0f) / 128.0f;
                }
                else if (bitsPerSample == 16)
                {
                    const int16_t value = static_cast<int16_t>(source[0] | (source[1] << 8));
                    clip.Samples[output] = static_cast<float>(value) / 32768.0f;
                }
                else if (bitsPerSample == 24)
                {
                    int32_t value = static_cast<int32_t>(source[0]) |
                        (static_cast<int32_t>(source[1]) << 8) | (static_cast<int32_t>(source[2]) << 16);
                    if ((value & 0x00800000) != 0)
                    {
                        value |= static_cast<int32_t>(0xFF000000);
                    }
                    clip.Samples[output] = static_cast<float>(value) / 8388608.0f;
                }
                else
                {
                    const int32_t value = static_cast<int32_t>(ReadU32(source));
                    clip.Samples[output] = static_cast<float>(value) / 2147483648.0f;
                }
                source += bytesPerSample;
            }
        }

        return true;
    }
}
