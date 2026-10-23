#pragma once

#include "Aster/Audio/AudioEngine.h"
#include "Aster/Physics/PhysicsWorld.h"
#include "Aster/Runtime/MarbleGame.h"
#include "Aster/Platform/Window.h"
#include "Aster/Rendering/Camera.h"
#include "Aster/Rendering/D3D12Context.h"
#include "Aster/Rendering/D3D12Renderer.h"
#include "Aster/Rendering/ImGuiRenderer.h"
#include "Aster/Scene/Scene.h"
#include "Aster/Scripting/ScriptEngine.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace Aster
{
    class Project;

    struct RuntimeOptions
    {
        std::filesystem::path ProjectRoot;
        std::filesystem::path ShaderDirectory;
        std::string ScreenshotPath;
        int Width = 1280;
        int Height = 720;
        int FrameLimit = 0;
    };

    // Standalone player: loads a project, runs Lua and physics, and renders with the primary scene camera.
    class RuntimeApp
    {
    public:
        RuntimeApp();
        ~RuntimeApp();

        RuntimeApp(const RuntimeApp&) = delete;
        RuntimeApp& operator=(const RuntimeApp&) = delete;

        int Run(const RuntimeOptions& options);

    private:
        [[nodiscard]] bool Initialize(const RuntimeOptions& options, std::string* error);
        void Shutdown();
        void StartScripts();
        void StopScripts();
        void StartAudio();
        void Update(float deltaTime);
        void UpdateCamera();
        void Render();
        void DrawHud();

        Window m_Window;
        D3D12Context m_Context;
        D3D12Renderer m_Renderer;
        ImGuiRenderer m_UI;
        Project* m_Project = nullptr;        Camera m_FallbackCamera;
        CameraData m_CameraData;
        PhysicsWorld m_Physics;
        ScriptEngine m_Scripts;
        AudioEngine m_Audio;
        MarbleGame m_MarbleGame;
        AssetRef<Environment> m_Environment;
        std::vector<ScriptHandle> m_ScriptsPlaying;
        std::vector<Entity> m_ScriptEntities;
        RuntimeOptions m_Options;
        double m_LastTime = 0.0;
        bool m_Initialized = false;
        bool m_AudioReady = false;
    };
}
