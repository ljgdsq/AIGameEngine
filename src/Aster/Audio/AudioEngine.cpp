#include "Aster/Audio/AudioEngine.h"

#include "Aster/Audio/WavDecoder.h"
#include "Aster/Assets/Asset.h"

#include <xaudio2.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace Aster
{
    namespace
    {
        void SetError(std::string* error, const char* message)
        {
            if (error != nullptr)
            {
                *error = message;
            }
        }
    }

    AudioEngine::AudioEngine() = default;

    AudioEngine::~AudioEngine()
    {
        Shutdown();
    }

    bool AudioEngine::Initialize(std::string* error)
    {
        if (IsInitialized())
        {
            return true;
        }

        const HRESULT comResult = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        m_CoInitialized = SUCCEEDED(comResult);
        if (FAILED(comResult) && comResult != RPC_E_CHANGED_MODE)
        {
            SetError(error, "Could not initialize COM for XAudio2");
            return false;
        }

        HRESULT result = ::XAudio2Create(&m_Engine, 0, XAUDIO2_DEFAULT_PROCESSOR);
        if (FAILED(result) || m_Engine == nullptr)
        {
            SetError(error, "Could not create the XAudio2 engine");
            Shutdown();
            return false;
        }

        result = m_Engine->CreateMasteringVoice(&m_Master);
        if (FAILED(result) || m_Master == nullptr)
        {
            SetError(error, "Could not create the XAudio2 mastering voice");
            Shutdown();
            return false;
        }
        return true;
    }

    void AudioEngine::Shutdown()
    {
        StopAll();
        if (m_Master != nullptr)
        {
            m_Master->DestroyVoice();
            m_Master = nullptr;
        }
        if (m_Engine != nullptr)
        {
            m_Engine->Release();
            m_Engine = nullptr;
        }
        if (m_CoInitialized)
        {
            ::CoUninitialize();
            m_CoInitialized = false;
        }
    }

    bool AudioEngine::Play(const Sound& sound, const glm::vec3& position, float volume, float pitch,
        bool loop, bool spatial, std::string* error)
    {
        if (!IsInitialized())
        {
            SetError(error, "Audio engine is not initialized");
            return false;
        }

        AudioClip clip;
        if (!WavDecoder::Decode(sound.EncodedData.data(), sound.EncodedData.size(), clip, error))
        {
            return false;
        }

        WAVEFORMATEX format{};
        format.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
        format.nChannels = clip.Channels;
        format.nSamplesPerSec = clip.SampleRate;
        format.wBitsPerSample = 32;
        format.nBlockAlign = static_cast<WORD>(clip.Channels * sizeof(float));
        format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
        format.cbSize = 0;

        IXAudio2SourceVoice* voice = nullptr;
        HRESULT result = m_Engine->CreateSourceVoice(&voice, &format);
        if (FAILED(result) || voice == nullptr)
        {
            SetError(error, "Could not create an XAudio2 source voice");
            return false;
        }

        float gain = std::clamp(volume, 0.0f, 1.0f);
        if (spatial)
        {
            const float distance = glm::length(position - m_ListenerPosition);
            gain *= std::clamp(1.0f / (1.0f + distance * 0.25f), 0.02f, 1.0f);
        }
        voice->SetVolume(gain);
        voice->SetFrequencyRatio(std::clamp(pitch, 0.25f, 4.0f));

        XAUDIO2_BUFFER buffer{};
        buffer.AudioBytes = static_cast<UINT32>(clip.Samples.size() * sizeof(float));
        buffer.pAudioData = reinterpret_cast<const BYTE*>(clip.Samples.data());
        buffer.Flags = XAUDIO2_END_OF_STREAM;
        buffer.LoopCount = loop ? XAUDIO2_LOOP_INFINITE : 0;

        result = voice->SubmitSourceBuffer(&buffer);
        if (FAILED(result))
        {
            voice->DestroyVoice();
            SetError(error, "Could not submit the decoded sound buffer");
            return false;
        }

        result = voice->Start(0);
        if (FAILED(result))
        {
            voice->DestroyVoice();
            SetError(error, "Could not start the XAudio2 source voice");
            return false;
        }

        ActiveVoice active;
        active.Source = voice;
        active.Samples = std::move(clip.Samples);
        m_Voices.push_back(std::move(active));
        return true;
    }

    void AudioEngine::StopAll()
    {
        for (ActiveVoice& active : m_Voices)
        {
            active.Source->Stop(0);
            active.Source->FlushSourceBuffers();
            active.Source->DestroyVoice();
        }
        m_Voices.clear();
    }

    void AudioEngine::Update()
    {
        for (size_t index = 0; index < m_Voices.size();)
        {
            XAUDIO2_VOICE_STATE state{};
            m_Voices[index].Source->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);
            if (state.BuffersQueued == 0)
            {
                m_Voices[index].Source->DestroyVoice();
                m_Voices.erase(m_Voices.begin() + static_cast<std::ptrdiff_t>(index));
            }
            else
            {
                ++index;
            }
        }
    }
}
