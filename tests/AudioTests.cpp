#include "TestFramework.h"

#include "Aster/Assets/Primitives.h"
#include "Aster/Audio/WavDecoder.h"
#include "Aster/Core/FileSystem.h"

#include <chrono>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>
#include <vector>

using namespace Aster;

namespace
{
    std::filesystem::path MakeTempAudioRoot()
    {
        const auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        return std::filesystem::temp_directory_path() / ("aster_audio_test_" + std::to_string(stamp));
    }
}

ASTER_TEST(Audio, DecodesGeneratedPcmTone)
{
    const std::filesystem::path root = MakeTempAudioRoot();
    std::filesystem::remove_all(root);
    ASTER_CHECK(FileSystem::CreateDirectories(root));

    std::string error;
    const std::filesystem::path wavPath = root / "tone.wav";
    ASTER_CHECK(Primitives::WriteToneWav(wavPath, 440.0f, 0.25f, 24000, &error));
    const auto bytes = FileSystem::ReadBinaryFile(wavPath);
    ASTER_CHECK(bytes.has_value());

    AudioClip clip;
    ASTER_CHECK(WavDecoder::Decode(bytes->data(), bytes->size(), clip, &error));
    ASTER_CHECK(clip.IsValid());
    ASTER_CHECK_EQ(clip.SampleRate, static_cast<uint32>(24000));
    ASTER_CHECK_EQ(clip.Channels, static_cast<uint16>(1));
    ASTER_CHECK_EQ(clip.Samples.size(), static_cast<size_t>(6000));

    float peak = 0.0f;
    for (float sample : clip.Samples)
    {
        peak = std::max(peak, std::abs(sample));
    }
    ASTER_CHECK(peak > 0.2f);
    ASTER_CHECK(std::abs(clip.Samples.front()) < 0.01f);
    ASTER_CHECK(std::abs(clip.Samples.back()) < 0.01f);

    std::filesystem::remove_all(root);
}

ASTER_TEST(Audio, RejectsInvalidWavData)
{
    const char garbage[] = "this is not a wave file";
    AudioClip clip;
    std::string error;
    ASTER_CHECK(!WavDecoder::Decode(garbage, sizeof(garbage), clip, &error));
    ASTER_CHECK(!clip.IsValid());
    ASTER_CHECK(!error.empty());

    const std::vector<uint8> truncated = { 'R', 'I', 'F', 'F' };
    ASTER_CHECK(!WavDecoder::Decode(truncated.data(), truncated.size(), clip, &error));
    ASTER_CHECK(!clip.IsValid());
}
