#include "Aster/Runtime/RuntimeApp.h"

#include "Aster/Core/FileSystem.h"
#include "Aster/Core/Log.h"
#include "Aster/Project/Project.h"
#include "Aster/Rendering/ParticleSystem.h"

#include <GLFW/glfw3.h>
#include <imgui.h>

#include <algorithm>
#include <cstdlib>
#include <string_view>

namespace Aster
{
    RuntimeApp::RuntimeApp() = default;

    RuntimeApp::~RuntimeApp()
    {
        Shutdown();
    }

    int RuntimeApp::Run(const RuntimeOptions& options)
    {
        Log::Initialize();
        m_Options = options;

        std::string error;
        if (!Initialize(options, &error))
        {
            ASTER_LOG_CRITICAL("Runtime", "Initialization failed: %s", error.c_str());
            Shutdown();
            return 1;
        }

        StartScripts();
        m_LastTime = m_Window.GetTime();
        int renderedFrames = 0;
        while (!m_Window.ShouldClose())
        {
            m_Window.PollEvents();
            if (m_Window.IsKeyDown(GLFW_KEY_ESCAPE))
            {
                m_Window.RequestClose();
            }

            const double now = m_Window.GetTime();
            const float deltaTime = static_cast<float>(std::clamp(now - m_LastTime, 0.0, 0.1));
            m_LastTime = now;
            Update(deltaTime);
            Render();

            ++renderedFrames;
            if (options.FrameLimit > 0 && renderedFrames >= options.FrameLimit)
            {
                ASTER_LOG_INFO("Runtime", "Frame limit reached (%d)", options.FrameLimit);
                break;
            }
        }

        if (!options.ScreenshotPath.empty())
        {
            std::string captureError;
            if (m_Context.CaptureBackbuffer(options.ScreenshotPath, &captureError))
            {
                ASTER_LOG_INFO("Runtime", "Screenshot written to %s", options.ScreenshotPath.c_str());
            }
            else
            {
                ASTER_LOG_ERROR("Runtime", "Screenshot failed: %s", captureError.c_str());
            }
        }

        StopScripts();
        Shutdown();
        return 0;
    }

    bool RuntimeApp::Initialize(const RuntimeOptions& options, std::string* error)
    {
        if (options.ProjectRoot.empty())
        {
            if (error != nullptr)
            {
                *error = "A project path is required";
            }
            return false;
        }

        m_Project = new Project();
        std::filesystem::path projectRoot = options.ProjectRoot;
        if (projectRoot.filename() == "project.aster.yaml")
        {
            projectRoot = projectRoot.parent_path();
        }
        if (!m_Project->Open(projectRoot, error))
        {
            return false;
        }

        const std::string environmentPath = m_Project->GetScene().GetSettings().EnvironmentPath;
        if (!environmentPath.empty())
        {
            std::string environmentError;
            m_Environment = m_Project->GetAssets().LoadEnvironment(environmentPath, &environmentError);
            if (!m_Environment.IsValid())
            {
                ASTER_LOG_WARN("Runtime", "Could not load environment '%s': %s", environmentPath.c_str(),
                    environmentError.c_str());
            }
        }

        WindowDesc windowDesc;
        windowDesc.Width = options.Width;
        windowDesc.Height = options.Height;
        windowDesc.Title = m_Project->GetInfo().Name + " - Aster Player";
        if (!m_Window.Initialize(windowDesc, error))
        {
            return false;
        }

        if (!m_Context.Initialize(m_Window, error))
        {
            return false;
        }

        std::filesystem::path shaderDirectory = options.ShaderDirectory;
        if (shaderDirectory.empty() ||
            !FileSystem::Exists(shaderDirectory / "Pbr.vs.dxil") ||
            !FileSystem::Exists(shaderDirectory / "Pbr.ps.dxil"))
        {
            shaderDirectory = ASTER_SHADER_DIR;
        }
        m_Renderer.SetShaderDirectory(FileSystem::Normalize(shaderDirectory));
        if (!m_Renderer.Initialize(m_Context, error))
        {
            return false;
        }
        if (!m_UI.Initialize(m_Window, m_Context, error))
        {
            return false;
        }

        m_FallbackCamera.SetPerspective(60.0f, m_Window.GetAspectRatio(), 0.1f, 1000.0f);
        m_FallbackCamera.SetPosition(glm::vec3(0.0f, 2.0f, 6.0f));
        m_FallbackCamera.SetRotation(-90.0f, -10.0f);
        m_CameraData = m_FallbackCamera.GetCameraData();
        UpdateCamera();

        std::string audioError;
        if (m_Audio.Initialize(&audioError))
        {
            m_Audio.SetListenerPosition(m_CameraData.Position);
            m_AudioReady = true;
        }
        else
        {
            ASTER_LOG_WARN("Runtime", "Audio is unavailable: %s", audioError.c_str());
        }

        m_Initialized = true;
        ASTER_LOG_INFO("Runtime", "Running project '%s'", m_Project->GetInfo().Name.c_str());
        return true;
    }

    void RuntimeApp::Shutdown()
    {
        StopScripts();
        m_Physics.Clear();
        m_Audio.Shutdown();
        m_AudioReady = false;
        delete m_Project;
        m_Project = nullptr;
        m_UI.Shutdown();
        m_Renderer.Shutdown();
        m_Context.Shutdown();
        m_Window.Shutdown();
        m_Initialized = false;
    }

    void RuntimeApp::StartScripts()
    {
        if (m_Project == nullptr || !m_Project->IsOpen())
        {
            return;
        }

        Scene& scene = m_Project->GetScene();
        std::string error;
        if (!m_Scripts.Initialize(scene, &error))
        {
            ASTER_LOG_ERROR("Runtime", "Lua initialization failed: %s", error.c_str());
            return;
        }
        m_Scripts.SetAssetManager(&m_Project->GetAssets());
        m_Physics.Rebuild(scene);
        m_Scripts.SetPhysics(&m_Physics);
        m_Scripts.SetKeyDownCallback([this](std::string_view key)
        {
            return m_Window.IsKeyDown(Window::KeyFromName(key));
        });

        for (Entity entity : scene.GetEntities())
        {
            const Registry& registry = scene.GetRegistry();
            if (!registry.Has<ScriptComponent>(entity))
            {
                continue;
            }
            for (const ScriptEntry& entry : registry.Get<ScriptComponent>(entity).Scripts)
            {
                if (!entry.Enabled || entry.Path.empty())
                {
                    continue;
                }
                ScriptHandle handle;
                if (!m_Scripts.LoadScript(m_Project->Resolve(entry.Path).string(), handle, &error))
                {
                    ASTER_LOG_ERROR("Runtime", "Could not load script '%s': %s", entry.Path.c_str(),
                        error.c_str());
                    continue;
                }
                if (!m_Scripts.Start(handle, entity, &error))
                {
                    ASTER_LOG_ERROR("Runtime", "Script '%s' failed OnStart: %s", entry.Path.c_str(),
                        error.c_str());
                    m_Scripts.UnloadScript(handle);
                    continue;
                }
                m_ScriptsPlaying.push_back(handle);
                m_ScriptEntities.push_back(entity);
            }
        }

        (void)m_MarbleGame.Initialize(scene, m_Physics);
        StartAudio();
        ASTER_LOG_INFO("Runtime", "Started %zu script(s) and %zu physics body/bodies",
            m_ScriptsPlaying.size(), m_Physics.GetBodyCount());
    }

    void RuntimeApp::StartAudio()
    {
        if (!m_AudioReady || m_Project == nullptr || !m_Project->IsOpen())
        {
            return;
        }

        Scene& scene = m_Project->GetScene();
        size_t started = 0;
        for (Entity entity : scene.GetEntities())
        {
            const Registry& registry = scene.GetRegistry();
            if (!registry.Has<AudioSourceComponent>(entity))
            {
                continue;
            }
            const AudioSourceComponent& audio = registry.Get<AudioSourceComponent>(entity);
            if (!audio.PlayOnStart || !audio.Clip.IsValid())
            {
                continue;
            }

            std::string error;
            if (m_Audio.Play(*audio.Clip, glm::vec3(scene.GetWorldMatrix(entity)[3]), audio.Volume,
                    audio.Pitch, audio.Loop, audio.Spatial, &error))
            {
                ++started;
            }
            else
            {
                ASTER_LOG_WARN("Runtime", "Could not play '%s': %s", audio.Clip.Path.c_str(), error.c_str());
            }
        }
        if (started > 0)
        {
            ASTER_LOG_INFO("Runtime", "Started %zu audio source(s)", started);
        }
    }

    void RuntimeApp::StopScripts()
    {
        if (m_Project != nullptr && m_Project->IsOpen())
        {
            Scene& scene = m_Project->GetScene();
            std::string error;
            for (size_t index = 0; index < m_ScriptsPlaying.size(); ++index)
            {
                if (index < m_ScriptEntities.size() && scene.IsAlive(m_ScriptEntities[index]))
                {
                    (void)m_Scripts.Destroy(m_ScriptsPlaying[index], m_ScriptEntities[index], &error);
                }
                m_Scripts.UnloadScript(m_ScriptsPlaying[index]);
            }
        }
        m_ScriptsPlaying.clear();
        m_ScriptEntities.clear();
        m_Scripts.Shutdown();
        m_Audio.StopAll();
    }

    void RuntimeApp::Update(float deltaTime)
    {
        if (m_Project == nullptr || !m_Project->IsOpen())
        {
            return;
        }

        Scene& scene = m_Project->GetScene();
        m_Physics.Step(deltaTime);
        m_Physics.SyncToScene(scene);

        std::string error;
        for (size_t index = 0; index < m_ScriptsPlaying.size(); ++index)
        {
            if (index < m_ScriptEntities.size() && scene.IsAlive(m_ScriptEntities[index]))
            {
                (void)m_Scripts.Update(m_ScriptsPlaying[index], m_ScriptEntities[index], deltaTime, &error);
            }
        }

        if (m_MarbleGame.IsActive())
        {
            MarbleInput input;
            input.Forward = (m_Window.IsKeyDown(GLFW_KEY_W) ? 1.0f : 0.0f) -
                (m_Window.IsKeyDown(GLFW_KEY_S) ? 1.0f : 0.0f);
            input.Right = (m_Window.IsKeyDown(GLFW_KEY_D) ? 1.0f : 0.0f) -
                (m_Window.IsKeyDown(GLFW_KEY_A) ? 1.0f : 0.0f);
            input.Jump = m_Window.IsKeyDown(GLFW_KEY_SPACE);
            input.Restart = m_Window.IsKeyDown(GLFW_KEY_R);
            m_MarbleGame.Update(deltaTime, input);
        }

        ParticleSystem::Update(scene, deltaTime);
        UpdateCamera();
        if (m_AudioReady)
        {
            m_Audio.SetListenerPosition(m_CameraData.Position);
            m_Audio.Update();
        }
    }

    void RuntimeApp::UpdateCamera()
    {
        if (m_Project == nullptr || !m_Project->IsOpen())
        {
            return;
        }

        Scene& scene = m_Project->GetScene();
        const Entity cameraEntity = scene.GetPrimaryCamera();
        const float aspect = m_Window.GetAspectRatio();
        if (cameraEntity != NullEntity &&
            scene.GetRegistry().Has<CameraComponent>(cameraEntity))
        {
            const CameraComponent& camera = scene.GetRegistry().Get<CameraComponent>(cameraEntity);
            m_CameraData = camera.BuildCameraData(scene.GetWorldMatrix(cameraEntity), aspect);
            return;
        }

        m_FallbackCamera.SetPerspective(60.0f, aspect, 0.1f, 1000.0f);
        m_CameraData = m_FallbackCamera.GetCameraData();
    }

    void RuntimeApp::Render()
    {
        if (m_Project == nullptr || !m_Project->IsOpen())
        {
            return;
        }
        if (!m_Context.BeginFrame())
        {
            return;
        }

        m_UI.BeginFrame();
        DrawHud();

        RenderView view;
        view.ViewProjection = m_CameraData.ViewProjection;
        view.Position = m_CameraData.Position;
        const glm::mat4 inverseView = glm::inverse(m_CameraData.View);
        view.CameraRight = glm::vec3(inverseView[0]);
        view.CameraUp = glm::vec3(inverseView[1]);
        view.ViewportWidth = m_Context.GetWidth();
        view.ViewportHeight = m_Context.GetHeight();
        m_Renderer.Render(m_Project->GetScene(), view,
            m_Environment.IsValid() ? m_Environment.Asset.get() : nullptr);
        m_Context.EndFrame([this](ID3D12GraphicsCommandList* commandList)
        {
            m_UI.EndFrame(commandList);
        });
    }

    void RuntimeApp::DrawHud()
    {
        if (!m_UI.IsInitialized() || m_Project == nullptr)
        {
            return;
        }

        constexpr ImGuiWindowFlags hudFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;

        if (m_MarbleGame.IsActive())
        {
            ImGuiIO& io = ImGui::GetIO();
            ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, 18.0f), ImGuiCond_Always,
                ImVec2(0.5f, 0.0f));
            ImGui::SetNextWindowBgAlpha(0.72f);
            ImGui::Begin("##MarbleHud", nullptr, hudFlags);
            ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.22f, 1.0f), "MARBLE RUN");
            ImGui::SameLine(0.0f, 18.0f);
            ImGui::Text("COINS  %d / %d", m_MarbleGame.GetScore(), m_MarbleGame.GetTotalCoins());
            ImGui::SameLine(0.0f, 18.0f);
            ImGui::Text("%05.1fs", m_MarbleGame.GetElapsedTime());
            if (m_MarbleGame.IsComplete())
            {
                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.35f, 1.0f, 0.55f, 1.0f), "GOAL!  Press R to run again");
            }
            else if (!m_MarbleGame.GetStatusMessage().empty())
            {
                ImGui::TextDisabled("%s", m_MarbleGame.GetStatusMessage().c_str());
            }
            ImGui::End();

            ImGui::SetNextWindowPos(ImVec2(16.0f, io.DisplaySize.y - 54.0f), ImGuiCond_Always);
            ImGui::SetNextWindowBgAlpha(0.55f);
            ImGui::Begin("##MarbleControls", nullptr, hudFlags);
            ImGui::TextDisabled("WASD  Roll    SPACE  Jump    R  Restart    ESC  Quit");
            ImGui::End();
            return;
        }

        ImGui::SetNextWindowPos(ImVec2(16.0f, 16.0f), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.55f);
        if (ImGui::Begin("Aster HUD", nullptr, hudFlags))
        {
            ImGui::TextUnformatted("ASTER PLAYER");
            ImGui::Text("%s", m_Project->GetInfo().Name.c_str());
            ImGui::Separator();
            ImGui::Text("FPS %.1f", ImGui::GetIO().Framerate);
            ImGui::Text("Entities %zu", m_Project->GetScene().GetEntities().size());
            ImGui::TextDisabled("ESC  Quit");
        }
        ImGui::End();
    }
}
