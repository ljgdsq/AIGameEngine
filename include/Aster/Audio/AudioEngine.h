#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <string>
#include <vector>

struct IXAudio2;
struct IXAudio2MasteringVoice;
struct IXAudio2SourceVoice;

namespace Aster
{
    struct Sound;

    // Minimal XAudio2 playback backend. It owns the mastering voice and one-shot/looping source
    // voices, decodes WAV assets on demand, and applies a simple distance attenuation to spatial
    // sources. XAudio2 is part of the Windows SDK, so the Windows/D3D12 target has no extra
    // third-party audio dependency.
    class AudioEngine
    {
    public:
        AudioEngine();
        ~AudioEngine();

        AudioEngine(const AudioEngine&) = delete;
        AudioEngine& operator=(const AudioEngine&) = delete;

        [[nodiscard]] bool Initialize(std::string* error = nullptr);
        void Shutdown();
        [[nodiscard]] bool IsInitialized() const { return m_Engine != nullptr && m_Master != nullptr; }

        [[nodiscard]] bool Play(const Sound& sound, const glm::vec3& position = glm::vec3(0.0f),
            float volume = 1.0f, float pitch = 1.0f, bool loop = false, bool spatial = true,
            std::string* error = nullptr);
        void StopAll();
        void SetListenerPosition(const glm::vec3& position) { m_ListenerPosition = position; }
        void Update();

    private:
        struct ActiveVoice
        {
            IXAudio2SourceVoice* Source = nullptr;
            // XAudio2 reads directly from the submitted PCM buffer, so the decoded samples must
            // outlive the source voice. Moving them here keeps the pointer valid until StopAll/Update.
            std::vector<float> Samples;
        };

        IXAudio2* m_Engine = nullptr;
        IXAudio2MasteringVoice* m_Master = nullptr;
        std::vector<ActiveVoice> m_Voices;
        glm::vec3 m_ListenerPosition{ 0.0f };
        bool m_CoInitialized = false;
    };
}
