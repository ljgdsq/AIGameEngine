#include "Aster/Editor/EditorApp.h"

#include "Aster/Project/GameExporter.h"
#include "Aster/Project/Project.h"
#include "Aster/Assets/Primitives.h"
#include "Aster/Core/FileSystem.h"
#include "Aster/Core/Log.h"
#include "Aster/Rendering/Camera.h"
#include "Aster/Rendering/ParticleSystem.h"
#include "Aster/Scene/SceneSerializer.h"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <GLFW/glfw3.h>

#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <limits>
#include <optional>
#include <utility>

namespace Aster
{
    namespace
    {
        constexpr const char* kEntityPayload = "ASTER_ENTITY";

        void DrawVec3(const char* label, glm::vec3& values, float speed = 0.1f, bool* changed = nullptr)
        {
            if (ImGui::DragFloat3(label, &values.x, speed))
            {
                if (changed != nullptr)
                {
                    *changed = true;
                }
            }
        }

        float DistanceToSegment(const ImVec2& point, const ImVec2& a, const ImVec2& b)
        {
            const ImVec2 ab(b.x - a.x, b.y - a.y);
            const ImVec2 ap(point.x - a.x, point.y - a.y);
            const float lengthSquared = ab.x * ab.x + ab.y * ab.y;
            if (lengthSquared <= 0.0001f)
            {
                return std::sqrt(ap.x * ap.x + ap.y * ap.y);
            }
            const float t = std::clamp((ap.x * ab.x + ap.y * ab.y) / lengthSquared, 0.0f, 1.0f);
            const ImVec2 closest(a.x + ab.x * t, a.y + ab.y * t);
            return std::sqrt((point.x - closest.x) * (point.x - closest.x) +
                (point.y - closest.y) * (point.y - closest.y));
        }

        ImVec4 ColorForContentType(ContentItemType type)
        {
            switch (type)
            {
            case ContentItemType::Folder:
                return ImVec4(0.96f, 0.78f, 0.38f, 1.0f);
            case ContentItemType::Scene:
                return ImVec4(0.45f, 0.86f, 1.0f, 1.0f);
            case ContentItemType::Mesh:
                return ImVec4(0.96f, 0.66f, 0.34f, 1.0f);
            case ContentItemType::Texture:
                return ImVec4(0.76f, 0.60f, 0.98f, 1.0f);
            case ContentItemType::Environment:
                return ImVec4(0.98f, 0.56f, 0.78f, 1.0f);
            case ContentItemType::Script:
                return ImVec4(0.42f, 0.72f, 1.0f, 1.0f);
            case ContentItemType::Audio:
                return ImVec4(0.48f, 0.86f, 0.60f, 1.0f);
            case ContentItemType::Other:
                return ImVec4(0.78f, 0.82f, 0.88f, 1.0f);
            }
            return ImVec4(0.78f, 0.82f, 0.88f, 1.0f);
        }

        ImVec4 ColorForLevel(LogLevel level)
        {
            switch (level)
            {
            case LogLevel::Trace:
                return ImVec4(0.55f, 0.55f, 0.6f, 1.0f);
            case LogLevel::Debug:
                return ImVec4(0.65f, 0.65f, 0.7f, 1.0f);
            case LogLevel::Info:
                return ImVec4(0.8f, 0.84f, 0.9f, 1.0f);
            case LogLevel::Warning:
                return ImVec4(0.95f, 0.72f, 0.25f, 1.0f);
            case LogLevel::Error:
            case LogLevel::Critical:
                return ImVec4(0.95f, 0.3f, 0.3f, 1.0f);
            default:
                return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
            }
        }

        glm::vec3 RotationFromDirection(const glm::vec3& direction)
        {
            const glm::vec3 forward = SafeNormalize(direction, glm::vec3(0.0f, 0.0f, -1.0f));
            return glm::vec3(glm::degrees(std::asin(std::clamp(forward.y, -1.0f, 1.0f))),
                glm::degrees(std::atan2(-forward.x, -forward.z)), 0.0f);
        }
    }

    EditorApp::EditorApp() = default;

    EditorApp::~EditorApp()
    {
        Shutdown();
    }

    EditorApp::Arguments EditorApp::ParseArguments(int argc, char** argv)
    {
        Arguments arguments;
        for (int i = 1; i < argc; ++i)
        {
            const std::string argument = argv[i];
            if ((argument == "--project" || argument == "-p") && i + 1 < argc)
            {
                arguments.ProjectPath = argv[++i];
            }
            else if (argument == "--create")
            {
                arguments.CreateProject = true;
            }
            else if (argument == "--demo")
            {
                arguments.CreateDemo = true;
            }
            else if (argument == "--play")
            {
                arguments.AutoPlay = true;
            }
            else if (argument == "--name" && i + 1 < argc)
            {
                arguments.ProjectName = argv[++i];
            }
            else if (argument == "--player" && i + 1 < argc)
            {
                arguments.PlayerPath = argv[++i];
            }
            else if ((argument == "--exec" || argument == "-e") && i + 1 < argc)
            {
                arguments.ScriptPath = argv[++i];
            }
            else if (argument == "--export" && i + 1 < argc)
            {
                arguments.ExportDirectory = argv[++i];
            }
            else if (argument == "--frames" && i + 1 < argc)
            {
                arguments.FrameLimit = std::max(0, std::atoi(argv[++i]));
            }
            else if (argument == "--screenshot" && i + 1 < argc)
            {
                arguments.ScreenshotPath = argv[++i];
            }
            else if (!argument.empty() && argument[0] != '-' && arguments.ProjectPath.empty())
            {
                arguments.ProjectPath = argument;
            }
        }
        return arguments;
    }

    int EditorApp::Run(int argc, char** argv)
    {
        Log::Initialize();
        m_Arguments = ParseArguments(argc, argv);

        std::string error;
        if (!Initialize(&error))
        {
            ASTER_LOG_CRITICAL("Editor", "Initialization failed: %s", error.c_str());
            Shutdown();
            return 1;
        }

        if (!m_Arguments.ProjectPath.empty())
        {
            const bool opened = m_Arguments.CreateDemo
                ? m_Project.CreateGameDemo(m_Arguments.ProjectPath, m_Arguments.ProjectName, &error)
                : (m_Arguments.CreateProject
                          ? m_Project.Create(m_Arguments.ProjectPath, m_Arguments.ProjectName, &error)
                          : m_Project.Open(m_Arguments.ProjectPath, &error));
            if (opened)
            {
                OnProjectOpened();
                if (m_Arguments.CreateDemo)
                {
                    m_GameView = true;
                }
                if (!m_Arguments.ScriptPath.empty())
                {
                    std::string scriptError;
                    if (!RunSceneScript(m_Arguments.ScriptPath, &scriptError))
                    {
                        ASTER_LOG_ERROR("Editor", "Scene script failed: %s", scriptError.c_str());
                        m_ExitCode = 1;
                    }
                    else if (m_Arguments.FrameLimit == 0 && m_Arguments.ExportDirectory.empty())
                    {
                        m_Window.RequestClose();
                    }
                }
                if (m_Arguments.AutoPlay)
                {
                    m_GameView = true;
                    StartPlay();
                }
                if (!m_Arguments.ExportDirectory.empty())
                {
                    std::string exportError;
                    if (!ExportGame(m_Arguments.ExportDirectory, &exportError))
                    {
                        ASTER_LOG_ERROR("Editor", "Export failed: %s", exportError.c_str());
                        m_ExitCode = 1;
                    }
                    if (m_Arguments.FrameLimit == 0)
                    {
                        m_Window.RequestClose();
                    }
                }
            }
            else
            {
                m_HubError = error;
                m_ShowProjectHub = true;
                ASTER_LOG_ERROR("Editor", "%s", error.c_str());
            }
        }

        m_LastTime = m_Window.GetTime();
        int renderedFrames = 0;
        while (!m_Window.ShouldClose())
        {
            m_Window.PollEvents();
            const double now = m_Window.GetTime();
            const float deltaTime = static_cast<float>(std::clamp(now - m_LastTime, 0.0, 0.1));
            m_LastTime = now;
            if (m_StatusTimer > 0.0f)
            {
                m_StatusTimer = std::max(0.0f, m_StatusTimer - deltaTime);
            }
            Frame(deltaTime);
            ++renderedFrames;
            if (m_Arguments.FrameLimit > 0 && renderedFrames >= m_Arguments.FrameLimit)
            {
                ASTER_LOG_INFO("Editor", "Frame limit reached (%d); shutting down", m_Arguments.FrameLimit);
                m_Window.RequestClose();
                break;
            }
        }

        if (!m_Arguments.ScreenshotPath.empty())
        {
            std::string captureError;
            if (m_Context.CaptureBackbuffer(m_Arguments.ScreenshotPath, &captureError))
            {
                ASTER_LOG_INFO("Editor", "Screenshot written to %s", m_Arguments.ScreenshotPath.c_str());
            }
            else
            {
                ASTER_LOG_ERROR("Editor", "Screenshot failed: %s", captureError.c_str());
            }
        }

        Shutdown();
        return m_ExitCode;
    }

    bool EditorApp::Initialize(std::string* error)
    {
        m_Initialized = true;
        WindowDesc desc;
        desc.Width = 1600;
        desc.Height = 900;
        desc.Title = "Aster Engine";
        if (!m_Window.Initialize(desc, error))
        {
            return false;
        }

        if (!m_Context.Initialize(m_Window, error))
        {
            return false;
        }

        if (!m_Renderer.Initialize(m_Context, error))
        {
            return false;
        }

        if (!m_UI.Initialize(m_Window, m_Context, error))
        {
            return false;
        }

        std::string audioError;
        if (m_Audio.Initialize(&audioError))
        {
            m_AudioReady = true;
        }
        else
        {
            ASTER_LOG_WARN("Editor", "Audio is unavailable: %s", audioError.c_str());
        }

        m_EditorCamera.SetPerspective(55.0f, 16.0f / 9.0f, 0.05f, 5000.0f);
        m_EditorCamera.SetPosition(glm::vec3(7.0f, 4.5f, 9.0f));
        m_EditorCamera.SetRotation(-128.0f, -17.0f);

        m_HubProjectPath = (std::filesystem::current_path() / "AsterProjects" / "SampleProject").string();
        m_HubOpenPath = std::filesystem::current_path().string();

        m_LogSink = Log::AddSink([this](const LogMessage& message)
        {
            m_ConsoleMessages.push_back(message);
            if (m_ConsoleMessages.size() > 1000)
            {
                m_ConsoleMessages.pop_front();
            }
        });
        for (const LogMessage& message : Log::GetMessages())
        {
            m_ConsoleMessages.push_back(message);
        }

        m_Initialized = true;
        m_ShowProjectHub = true;
        m_Window.SetTitle("Aster Engine");
        ASTER_LOG_INFO("Editor", "D3D12 adapter: %s", m_Context.GetAdapterName().c_str());
        return true;
    }

    void EditorApp::Shutdown()
    {
        if (m_LogSink != 0)
        {
            Log::RemoveSink(m_LogSink);
            m_LogSink = 0;
        }
        if (!m_Initialized)
        {
            return;
        }
        StopPlay();
        ASTER_LOG_INFO("Editor", "Shutting down ImGui");
        m_UI.Shutdown();
        m_Audio.Shutdown();
        m_AudioReady = false;
        ASTER_LOG_INFO("Editor", "Shutting down renderer");
        m_Renderer.Shutdown();
        ASTER_LOG_INFO("Editor", "Closing project");
        m_ContentBrowser.Clear();
        m_ContentSelection.clear();
        m_Project.Close();
        ASTER_LOG_INFO("Editor", "Shutting down D3D12 context");
        m_Context.Shutdown();
        ASTER_LOG_INFO("Editor", "Shutting down window");
        m_Window.Shutdown();
        m_Initialized = false;
    }

    void EditorApp::Frame(float deltaTime)
    {
        m_FrameDelta = deltaTime;
        if (!m_Project.IsOpen())
        {
            m_Selected = NullEntity;
        }
        else if (m_Selected != NullEntity && !m_Project.GetScene().IsAlive(m_Selected))
        {
            m_Selected = NullEntity;
        }

        if (!m_Project.IsOpen() && m_Playing)
        {
            StopPlay();
        }

        if (!m_Context.BeginFrame())
        {
            return;
        }

        m_UI.BeginFrame();
        DrawUI();
        UpdatePlayMode(deltaTime);
        ParticleSystem::Update(m_Project.GetScene(), deltaTime);

        ImGuiIO& io = ImGui::GetIO();
        const glm::vec2 displaySize(std::max(io.DisplaySize.x, 1.0f), std::max(io.DisplaySize.y, 1.0f));
        const bool viewportMode = m_Project.IsOpen() && !m_ShowProjectHub;

        RenderView view;
        view.ViewProjection = m_EditorCamera.GetViewProjection();
        view.Position = m_EditorCamera.GetPosition();
        view.CameraRight = m_EditorCamera.GetRight();
        view.CameraUp = m_EditorCamera.GetUp();
        view.ShowColliders = m_ShowColliders && m_Project.IsOpen() && !m_GameView;

        if (m_GameView && m_Project.IsOpen())
        {
            Scene& scene = m_Project.GetScene();
            const Entity cameraEntity = scene.GetPrimaryCamera();
            if (cameraEntity != NullEntity && scene.GetRegistry().Has<CameraComponent>(cameraEntity))
            {
                const float aspect = std::max(0.0001f, m_ViewportWidth / std::max(1.0f, m_ViewportHeight));
                const CameraData gameCamera =
                    scene.GetRegistry().Get<CameraComponent>(cameraEntity).BuildCameraData(
                        scene.GetWorldMatrix(cameraEntity), aspect);
                view.ViewProjection = gameCamera.ViewProjection;
                view.Position = gameCamera.Position;
                const glm::mat4 inverseView = glm::inverse(gameCamera.View);
                view.CameraRight = glm::vec3(inverseView[0]);
                view.CameraUp = glm::vec3(inverseView[1]);
            }
        }
        if (viewportMode)
        {
            const float scaleX = static_cast<float>(m_Context.GetWidth()) / displaySize.x;
            const float scaleY = static_cast<float>(m_Context.GetHeight()) / displaySize.y;
            view.ViewportX = static_cast<uint32_t>(m_ViewportX * scaleX);
            view.ViewportY = static_cast<uint32_t>(m_ViewportY * scaleY);
            view.ViewportWidth = static_cast<uint32_t>(std::max(1.0f, m_ViewportWidth * scaleX));
            view.ViewportHeight = static_cast<uint32_t>(std::max(1.0f, m_ViewportHeight * scaleY));
        }
        else
        {
            view.ViewportWidth = m_Context.GetWidth();
            view.ViewportHeight = m_Context.GetHeight();
        }

        m_Renderer.Render(m_Project.GetScene(), view,
            m_Environment.IsValid() ? m_Environment.Asset.get() : nullptr);
        m_Context.EndFrame([this](ID3D12GraphicsCommandList* commandList)
        {
            m_UI.EndFrame(commandList);
        });

        std::string title = "Aster Engine";
        if (m_Project.IsOpen())
        {
            title += " - " + m_Project.GetInfo().Name;
            if (!m_Project.GetCurrentScenePath().empty())
            {
                title += " - " + m_Project.GetCurrentScenePath();
            }
            if (m_SceneDirty)
            {
                title += " *";
            }
        }
        // SetWindowText triggers a non-client redraw; doing it every frame makes the title bar
        // flicker while the user interacts with the editor.
        if (title != m_WindowTitle)
        {
            m_WindowTitle = std::move(title);
            m_Window.SetTitle(m_WindowTitle);
        }

        if (!m_Project.IsOpen())
        {
            m_Selected = NullEntity;
        }
    }

    void EditorApp::DrawUI()
    {
        ImGuiIO& io = ImGui::GetIO();
        const float uiWidth = std::max(io.DisplaySize.x, 1.0f);
        const float uiHeight = std::max(io.DisplaySize.y, 1.0f);

        DrawMenuBar();
        DrawExportPopup();

        if (!m_Project.IsOpen() || m_ShowProjectHub)
        {
            m_ViewportX = 0.0f;
            m_ViewportY = 0.0f;
            m_ViewportWidth = uiWidth;
            m_ViewportHeight = uiHeight;
            m_EditorCamera.SetViewportSize(static_cast<uint32>(uiWidth), static_cast<uint32>(uiHeight));
            DrawProjectHub();
            return;
        }

        const float menuHeight = ImGui::GetFrameHeight();
        const float hierarchyWidth = m_ShowHierarchy ? m_LeftPanelWidth : 0.0f;
        const float inspectorWidth = m_ShowInspector ? m_RightPanelWidth : 0.0f;
        const float consoleHeight = m_ShowConsole ? m_ConsolePanelHeight : 0.0f;
        const float top = menuHeight + m_ToolbarHeight + m_PanelGap;
        const float consoleTop = uiHeight - m_StatusBarHeight - consoleHeight - m_PanelGap;
        const float contentBottom = m_ShowConsole ? consoleTop - m_PanelGap : uiHeight - m_StatusBarHeight - m_PanelGap;

        DrawToolbar();

        m_ViewportX = m_PanelGap + hierarchyWidth + (m_ShowHierarchy ? m_PanelGap : 0.0f);
        m_ViewportY = top;
        m_ViewportWidth = std::max(1.0f, uiWidth - m_ViewportX - m_PanelGap -
            (m_ShowInspector ? inspectorWidth + m_PanelGap : 0.0f));
        m_ViewportHeight = std::max(1.0f, contentBottom - top);

        m_EditorCamera.SetViewportSize(static_cast<uint32>(m_ViewportWidth),
            static_cast<uint32>(m_ViewportHeight));

        DrawViewport(m_FrameDelta);
        if (m_ShowHierarchy)
        {
            DrawHierarchy();
        }
        if (m_ShowInspector)
        {
            DrawInspector();
        }
        if (m_ShowConsole)
        {
            DrawConsole();
        }
        DrawStatusBar();

        ImGuiIO& keyboard = ImGui::GetIO();
        if (!keyboard.WantTextInput)
        {
            if (ImGui::IsKeyPressed(ImGuiKey_1))
            {
                m_GizmoMode = GizmoMode::Translate;
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_2))
            {
                m_GizmoMode = GizmoMode::Rotate;
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_3))
            {
                m_GizmoMode = GizmoMode::Scale;
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_Delete))
            {
                DeleteSelected();
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_F))
            {
                FocusSelected();
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_F5))
            {
                TogglePlayMode();
            }
            if (keyboard.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S))
            {
                std::string error;
                if (m_Project.SaveScene(&error))
                {
                    m_SceneDirty = false;
                    SetStatus("Scene saved: " + m_Project.GetCurrentScenePath());
                }
                else
                {
                    SetStatus("Save failed: " + error);
                }
            }
        }

        if (m_OpenSaveAsPopup)
        {
            ImGui::OpenPopup("Save Scene As");
            m_OpenSaveAsPopup = false;
        }
        if (ImGui::BeginPopupModal("Save Scene As", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::InputText("Scene Path", &m_SaveAsPath);
            if (ImGui::Button("Save"))
            {
                std::string error;
                if (m_Project.SaveSceneAs(m_SaveAsPath, &error))
                {
                    m_SceneDirty = false;
                    SetStatus("Scene saved: " + m_Project.GetCurrentScenePath());
                    ImGui::CloseCurrentPopup();
                }
                else
                {
                    SetStatus("Save failed: " + error);
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    void EditorApp::DrawMenuBar()
    {
        if (!ImGui::BeginMainMenuBar())
        {
            return;
        }

        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("New Project..."))
            {
                m_ShowProjectHub = true;
                m_HubError.clear();
            }
            if (ImGui::MenuItem("Open Project..."))
            {
                m_ShowProjectHub = true;
                m_HubError.clear();
            }
            if (m_Project.IsOpen())
            {
                ImGui::Separator();
                if (ImGui::MenuItem("Save Scene", "Ctrl+S"))
                {
                    std::string error;
                    if (m_Project.SaveScene(&error))
                    {
                        m_SceneDirty = false;
                        SetStatus("Scene saved");
                    }
                    else
                    {
                        SetStatus("Save failed: " + error);
                    }
                }
                if (ImGui::MenuItem("Save Scene As..."))
                {
                    m_SaveAsPath = m_Project.GetCurrentScenePath();
                    m_OpenSaveAsPopup = true;
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Export Game..."))
                {
                    m_ExportPath = (std::filesystem::current_path() / "Exports" /
                        m_Project.GetInfo().Name).string();
                    m_ExportError.clear();
                    m_OpenExportPopup = true;
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4"))
            {
                m_Window.RequestClose();
            }
            ImGui::EndMenu();
        }

        if (m_Project.IsOpen() && ImGui::BeginMenu("Play"))
        {
            if (ImGui::MenuItem(m_Playing ? "Stop" : "Play", "F5"))
            {
                TogglePlayMode();
            }
            ImGui::EndMenu();
        }
        if (m_Project.IsOpen() && m_Playing)
        {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.35f, 0.9f, 0.45f, 1.0f), "PLAYING");
        }

        if (m_Project.IsOpen())
        {
            if (ImGui::BeginMenu("Edit"))
            {
                if (ImGui::MenuItem("Delete Selected", "Del", false, m_Selected != NullEntity))
                {
                    DeleteSelected();
                }
                if (ImGui::MenuItem("Focus Selected", "F", false, m_Selected != NullEntity))
                {
                    FocusSelected();
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("GameObject"))
            {
                if (ImGui::MenuItem("Create Empty"))
                {
                    CreateEmpty();
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Create Cube"))
                {
                    CreateMeshEntity("Cube", "assets/models/cube.obj", glm::vec3(1.0f));
                }
                if (ImGui::MenuItem("Create Sphere"))
                {
                    CreateMeshEntity("Sphere", "assets/models/sphere.obj", glm::vec3(1.0f));
                }
                if (ImGui::MenuItem("Create Plane"))
                {
                    CreateMeshEntity("Plane", "assets/models/plane.obj", glm::vec3(4.0f, 1.0f, 4.0f));
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Create Camera"))
                {
                    CreateCameraEntity();
                }
                if (ImGui::MenuItem("Create Directional Light"))
                {
                    CreateLightEntity(true);
                }
                if (ImGui::MenuItem("Create Point Light"))
                {
                    CreateLightEntity(false);
                }
                if (ImGui::MenuItem("Create Particle Emitter"))
                {
                    Scene& scene = m_Project.GetScene();
                    const Entity entity = scene.CreateEntity(UniqueName("Particle Emitter"));
                    scene.GetRegistry().Emplace<ParticleEmitterComponent>(entity,
                        ParticleEmitterComponent{});
                    m_Selected = entity;
                    m_SceneDirty = true;
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("View"))
            {
                ImGui::MenuItem("Hierarchy", nullptr, &m_ShowHierarchy);
                ImGui::MenuItem("Inspector", nullptr, &m_ShowInspector);
                ImGui::MenuItem("Console", nullptr, &m_ShowConsole);
                ImGui::MenuItem("Physics Colliders", nullptr, &m_ShowColliders);
                ImGui::EndMenu();
            }
        }

        if (ImGui::BeginMenu("Help"))
        {
            if (ImGui::MenuItem("About Aster Engine"))
            {
                SetStatus("Aster Engine - C++20 / NVRHI / D3D12 editor prototype");
            }
            ImGui::EndMenu();
        }

        if (m_Project.IsOpen())
        {
            const std::string label = m_Project.GetInfo().Name + (m_SceneDirty ? " *" : "");
            const float width = ImGui::CalcTextSize(label.c_str()).x;
            ImGui::SameLine(ImGui::GetWindowWidth() - width - 20.0f);
            ImGui::TextDisabled("%s", label.c_str());
        }

        ImGui::EndMainMenuBar();
    }

    void EditorApp::DrawToolbar()
    {
        ImGuiIO& io = ImGui::GetIO();
        const ImVec4 accent(0.29f, 0.64f, 1.0f, 1.0f);
        const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBringToFrontOnFocus;

        ImGui::SetNextWindowPos(ImVec2(0.0f, ImGui::GetFrameHeight()), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, m_ToolbarHeight), ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 7.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.043f, 0.050f, 0.064f, 1.0f));
        ImGui::Begin("##AsterToolbar", nullptr, flags);

        ImGui::TextColored(accent, "ASTER");
        ImGui::SameLine(0.0f, 4.0f);
        ImGui::TextDisabled("ENGINE");
        ImGui::SameLine(0.0f, 22.0f);
        ImGui::TextDisabled("|");
        ImGui::SameLine(0.0f, 22.0f);

        if (m_Project.IsOpen())
        {
            const ImVec4 playColor = m_Playing ? ImVec4(0.42f, 0.13f, 0.14f, 1.0f)
                                               : ImVec4(0.10f, 0.34f, 0.22f, 1.0f);
            const ImVec4 playHover = m_Playing ? ImVec4(0.56f, 0.18f, 0.19f, 1.0f)
                                               : ImVec4(0.14f, 0.45f, 0.29f, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Button, playColor);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, playHover);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, playHover);
            ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);
            if (ImGui::Button(m_Playing ? "Stop" : "Play", ImVec2(66.0f, 30.0f)))
            {
                TogglePlayMode();
            }
            ImGui::PopItemFlag();
            ImGui::PopStyleColor(3);

            ImGui::SameLine();
            if (ImGui::Button("Save", ImVec2(62.0f, 30.0f)))
            {
                std::string error;
                if (m_Project.SaveScene(&error))
                {
                    m_SceneDirty = false;
                    SetStatus("Scene saved");
                }
                else
                {
                    SetStatus("Save failed: " + error);
                }
            }

            ImGui::SameLine(0.0f, 22.0f);
            ImGui::TextDisabled("|");
            ImGui::SameLine(0.0f, 16.0f);

            const auto modeButton = [this, accent](const char* label, GizmoMode mode, float width)
            {
                const bool active = m_GizmoMode == mode;
                ImGui::PushStyleColor(ImGuiCol_Button,
                    active ? ImVec4(0.16f, 0.38f, 0.62f, 1.0f) : ImVec4(0.095f, 0.115f, 0.150f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                    active ? ImVec4(0.22f, 0.48f, 0.74f, 1.0f) : ImVec4(0.15f, 0.19f, 0.25f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Text, active ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f) : accent);
                if (ImGui::Button(label, ImVec2(width, 30.0f)))
                {
                    m_GizmoMode = mode;
                }
                ImGui::PopStyleColor(3);
            };
            modeButton("Move", GizmoMode::Translate, 62.0f);
            ImGui::SameLine();
            modeButton("Rotate", GizmoMode::Rotate, 68.0f);
            ImGui::SameLine();
            modeButton("Scale", GizmoMode::Scale, 60.0f);

            ImGui::SameLine(0.0f, 18.0f);
            if (ImGui::Button("Focus", ImVec2(64.0f, 30.0f)))
            {
                FocusSelected();
            }

            ImGui::SameLine(0.0f, 18.0f);
            ImGui::TextDisabled("|");
            ImGui::SameLine(0.0f, 12.0f);
            const auto viewButton = [this](const char* label, bool gameView)
            {
                const bool active = m_GameView == gameView;
                ImGui::PushStyleColor(ImGuiCol_Button,
                    active ? ImVec4(0.16f, 0.38f, 0.62f, 1.0f) : ImVec4(0.095f, 0.115f, 0.150f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.22f, 0.48f, 0.74f, 1.0f));
                if (ImGui::Button(label, ImVec2(62.0f, 30.0f)))
                {
                    m_GameView = gameView;
                }
                ImGui::PopStyleColor(2);
            };
            viewButton("Scene", false);
            ImGui::SameLine();
            viewButton("Game", true);

            const float rightStart = std::max(ImGui::GetCursorPosX() + 12.0f, ImGui::GetWindowWidth() - 290.0f);
            ImGui::SameLine(rightStart);
            ImGui::Checkbox("Colliders", &m_ShowColliders);
            ImGui::SameLine(0.0f, 16.0f);
            ImGui::TextColored(ImVec4(0.55f, 0.62f, 0.72f, 1.0f), "%.0f FPS", io.Framerate);
            if (m_Playing)
            {
                ImGui::SameLine(0.0f, 14.0f);
                ImGui::TextColored(ImVec4(0.42f, 0.90f, 0.55f, 1.0f), "PLAYING");
            }
        }
        else
        {
            ImGui::TextDisabled("No project open");
        }

        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(3);
    }

    void EditorApp::DrawStatusBar()
    {
        ImGuiIO& io = ImGui::GetIO();
        const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBringToFrontOnFocus;

        ImGui::SetNextWindowPos(ImVec2(0.0f, io.DisplaySize.y - m_StatusBarHeight), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, m_StatusBarHeight), ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 4.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.032f, 0.038f, 0.050f, 1.0f));
        ImGui::Begin("##AsterStatus", nullptr, flags);

        const bool hasStatus = !m_StatusMessage.empty() && m_StatusTimer > 0.0f;
        ImGui::TextColored(hasStatus ? ImVec4(1.0f, 0.78f, 0.42f, 1.0f) : ImVec4(0.55f, 0.62f, 0.72f, 1.0f),
            "%s", hasStatus ? m_StatusMessage.c_str() : "Ready");

        ImGui::SameLine(std::max(ImGui::GetCursorPosX() + 16.0f, ImGui::GetWindowWidth() - 390.0f));
        if (m_Project.IsOpen())
        {
            const Scene& scene = m_Project.GetScene();
            ImGui::TextDisabled("%zu entities", scene.GetEntities().size());
            ImGui::SameLine(0.0f, 18.0f);
            ImGui::TextDisabled("|");
            ImGui::SameLine(0.0f, 18.0f);
            ImGui::TextDisabled("%s", m_SceneDirty ? "Unsaved changes" : "Saved");
        }
        ImGui::SameLine(0.0f, 18.0f);
        ImGui::TextDisabled("%.0f FPS", io.Framerate);
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(3);
    }

    void EditorApp::DrawProjectHub()
    {
        ImGuiIO& io = ImGui::GetIO();
        const ImVec2 size(720.0f, 620.0f);
        ImGui::SetNextWindowPos(ImVec2((io.DisplaySize.x - size.x) * 0.5f, (io.DisplaySize.y - size.y) * 0.5f),
            ImGuiCond_Always);
        ImGui::SetNextWindowSize(size, ImGuiCond_Always);
        ImGui::Begin("Aster Engine", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);

        const ImVec4 accent(0.29f, 0.64f, 1.0f, 1.0f);
        ImGui::TextColored(accent, "ASTER ENGINE");
        ImGui::SameLine(0.0f, 8.0f);
        ImGui::TextDisabled("/ EDITOR");
        ImGui::TextDisabled("A C++20 real-time editor built on NVRHI and Direct3D 12.");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (m_Project.IsOpen())
        {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.075f, 0.090f, 0.115f, 1.0f));
            ImGui::BeginChild("##CurrentProject", ImVec2(0.0f, 82.0f), true, ImGuiWindowFlags_NoScrollbar);
            ImGui::TextDisabled("CURRENT PROJECT");
            ImGui::TextColored(ImVec4(0.55f, 0.86f, 0.64f, 1.0f), "%s", m_Project.GetInfo().Name.c_str());
            ImGui::SameLine();
            if (ImGui::Button("Back to Editor", ImVec2(150.0f, 0.0f)))
            {
                m_ShowProjectHub = false;
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();
            ImGui::Spacing();
        }

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.065f, 0.078f, 0.100f, 1.0f));
        ImGui::BeginChild("##CreateProjectCard", ImVec2(0.0f, 280.0f), true, ImGuiWindowFlags_NoScrollbar);
        ImGui::TextDisabled("NEW PROJECT");
        ImGui::Separator();
        ImGui::InputText("Directory", &m_HubProjectPath);
        ImGui::InputText("Project name", &m_HubProjectName);
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, accent);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.40f, 0.73f, 1.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.22f, 0.52f, 0.82f, 1.0f));
        if (ImGui::Button("Create Project", ImVec2(190.0f, 32.0f)))
        {
            StopPlay();
            m_Project.Close();
            std::string error;
            if (m_Project.Create(m_HubProjectPath, m_HubProjectName, &error))
            {
                m_HubError.clear();
                OnProjectOpened();
            }
            else
            {
                m_HubError = error;
            }
        }
        ImGui::PopStyleColor(3);
        ImGui::SameLine();
        if (ImGui::Button("Create Sample Project", ImVec2(210.0f, 32.0f)))
        {
            StopPlay();
            m_Project.Close();
            m_HubProjectPath = (std::filesystem::current_path() / "AsterProjects" / "SampleProject").string();
            m_HubProjectName = "SampleProject";
            std::string error;
            if (m_Project.Create(m_HubProjectPath, m_HubProjectName, &error))
            {
                m_HubError.clear();
                OnProjectOpened();
            }
            else
            {
                m_HubError = error;
            }
        }
        if (ImGui::Button("Create Marble Run Demo", ImVec2(230.0f, 32.0f)))
        {
            StopPlay();
            m_Project.Close();
            m_HubProjectPath = (std::filesystem::current_path() / "AsterProjects" / "MarbleRun").string();
            m_HubProjectName = "MarbleRun";
            std::string error;
            if (m_Project.CreateGameDemo(m_HubProjectPath, m_HubProjectName, &error))
            {
                m_HubError.clear();
                m_GameView = true;
                OnProjectOpened();
            }
            else
            {
                m_HubError = error;
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.065f, 0.078f, 0.100f, 1.0f));
        ImGui::BeginChild("##OpenProjectCard", ImVec2(0.0f, 0.0f), true);
        ImGui::TextDisabled("OPEN PROJECT");
        ImGui::Separator();
        ImGui::InputText("Project directory", &m_HubOpenPath);
        if (ImGui::Button("Open Project", ImVec2(190.0f, 32.0f)))
        {
            StopPlay();
            m_Project.Close();
            std::string error;
            if (m_Project.Open(m_HubOpenPath, &error))
            {
                m_HubError.clear();
                OnProjectOpened();
            }
            else
            {
                m_HubError = error;
            }
        }

        if (!m_HubError.empty())
        {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "%s", m_HubError.c_str());
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextDisabled("Adapter: %s", m_Context.GetAdapterName().c_str());
        ImGui::TextDisabled("Right mouse: look  |  WASD: move  |  1/2/3: gizmo mode");
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::End();
    }

    void EditorApp::DrawViewport(float deltaTime)
    {
        const bool focusViewport = m_FocusViewportRequested;
        if (focusViewport)
        {
            ImGui::SetNextWindowFocus();
            m_FocusViewportRequested = false;
        }
        ImGui::SetNextWindowPos(ImVec2(m_ViewportX, m_ViewportY), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(m_ViewportWidth, m_ViewportHeight), ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::Begin("###AsterViewport", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground |
                ImGuiWindowFlags_NoBringToFrontOnFocus);

        if (focusViewport)
        {
            ImGui::SetWindowFocus();
            ASTER_LOG_DEBUG("Editor", "Viewport focus applied (focused=%d)",
                ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) ? 1 : 0);
        }
        m_ViewportFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        const bool gameplay = m_Playing && m_GameView;

        ImGui::SetCursorPos(ImVec2(0.0f, 0.0f));
        ImGui::InvisibleButton("##ViewportCanvas", ImVec2(m_ViewportWidth, m_ViewportHeight),
            ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
        m_ViewportHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) ||
            ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
        if (m_ViewportHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            ImGui::SetWindowFocus();
            m_ViewportFocused = true;
        }
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASTER_ASSET"))
            {
                if (payload->Data != nullptr && payload->DataSize > 0)
                {
                    ContentItem item;
                    item.RelativePath = static_cast<const char*>(payload->Data);
                    item.Name = std::filesystem::path(item.RelativePath).filename().string();
                    item.Type = ContentBrowser::Classify(item.Name);
                    // Use the same type dispatch as double-clicking the item so dragging a scene,
                    // environment, script, texture, or audio file behaves consistently.
                    ActivateContentItem(item);
                }
            }
            ImGui::EndDragDropTarget();
        }

        if (!gameplay)
        {
            UpdateCamera(deltaTime, m_ViewportHovered);
        }

        DrawViewportOverlay();
        if (!m_Playing)
        {
            DrawGizmo();
        }
        else
        {
            m_ActiveGizmoAxis = -1;
            m_GizmoHovered = false;
        }

        if (!m_Playing && m_ViewportHovered && m_ActiveGizmoAxis < 0 && !m_GizmoHovered &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            const ImVec2 mouse = ImGui::GetMousePos();
            PickEntity(glm::vec2(mouse.x - m_ViewportX, mouse.y - m_ViewportY),
                glm::vec2(m_ViewportWidth, m_ViewportHeight));
        }

        ImGui::End();
        ImGui::PopStyleVar(3);
    }

    void EditorApp::DrawViewportOverlay()
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 min(m_ViewportX, m_ViewportY);
        const ImVec2 max(m_ViewportX + m_ViewportWidth, m_ViewportY + m_ViewportHeight);
        drawList->PushClipRect(min, max, true);

        if (m_Playing && m_GameView)
        {
            const ImU32 textColor = IM_COL32(235, 240, 250, 245);
            drawList->AddRectFilled(ImVec2(min.x + 8.0f, min.y + 8.0f),
                ImVec2(min.x + 388.0f, min.y + 58.0f), IM_COL32(10, 13, 18, 205), 5.0f);
            drawList->AddText(ImVec2(min.x + 16.0f, min.y + 14.0f), IM_COL32(90, 210, 255, 255),
                "GAME VIEW  |  PLAYING");
            drawList->AddText(ImVec2(min.x + 16.0f, min.y + 35.0f), textColor,
                "WASD Roll   SPACE Jump   R Restart   Stop to edit");

            if (m_MarbleGame.IsActive())
            {
                char scoreText[96];
                std::snprintf(scoreText, sizeof(scoreText), "COINS %d / %d    %.1fs",
                    m_MarbleGame.GetScore(), m_MarbleGame.GetTotalCoins(), m_MarbleGame.GetElapsedTime());
                drawList->AddText(ImVec2(min.x + 408.0f, min.y + 16.0f),
                    IM_COL32(255, 196, 80, 255), scoreText);
                if (m_MarbleGame.IsComplete())
                {
                    drawList->AddText(ImVec2(min.x + 408.0f, min.y + 37.0f),
                        IM_COL32(90, 255, 150, 255), "GOAL! Press R to run again");
                }
            }

            char fpsText[32];
            std::snprintf(fpsText, sizeof(fpsText), "%.0f FPS", ImGui::GetIO().Framerate);
            const ImVec2 fpsSize = ImGui::CalcTextSize(fpsText);
            drawList->AddText(ImVec2(max.x - fpsSize.x - 16.0f, min.y + 14.0f), textColor, fpsText);
            drawList->PopClipRect();
            return;
        }

        const ImU32 gridColor = IM_COL32(70, 76, 88, 110);
        const ImU32 axisColorX = IM_COL32(170, 62, 62, 180);
        const ImU32 axisColorZ = IM_COL32(62, 96, 175, 180);
        const glm::vec2 viewportSize(m_ViewportWidth, m_ViewportHeight);
        auto toScreen = [&](const glm::vec3& world, ImVec2& out) -> bool
        {
            glm::vec2 screen;
            if (!m_EditorCamera.WorldToScreen(world, viewportSize, screen))
            {
                return false;
            }
            // ImGui expands lines into triangles; endpoints far outside the viewport can turn into
            // enormous geometry that stalls the GPU. Keep only points near the visible area.
            const float margin = 4.0f * (std::max)(viewportSize.x, viewportSize.y);
            if (!std::isfinite(screen.x) || !std::isfinite(screen.y) ||
                std::abs(screen.x) > margin || std::abs(screen.y) > margin)
            {
                return false;
            }
            out = ImVec2(m_ViewportX + screen.x, m_ViewportY + screen.y);
            return true;
        };

        for (int i = -12; i <= 12; ++i)
        {
            ImVec2 a;
            ImVec2 b;
            if (i == 0)
            {
                if (toScreen(glm::vec3(-12.0f, 0.0f, 0.0f), a) &&
                    toScreen(glm::vec3(12.0f, 0.0f, 0.0f), b))
                {
                    drawList->AddLine(a, b, axisColorX, 1.5f);
                }
                if (toScreen(glm::vec3(0.0f, 0.0f, -12.0f), a) &&
                    toScreen(glm::vec3(0.0f, 0.0f, 12.0f), b))
                {
                    drawList->AddLine(a, b, axisColorZ, 1.5f);
                }
                continue;
            }
            if (toScreen(glm::vec3(static_cast<float>(i), 0.0f, -12.0f), a) &&
                toScreen(glm::vec3(static_cast<float>(i), 0.0f, 12.0f), b))
            {
                drawList->AddLine(a, b, gridColor);
            }
            if (toScreen(glm::vec3(-12.0f, 0.0f, static_cast<float>(i)), a) &&
                toScreen(glm::vec3(12.0f, 0.0f, static_cast<float>(i)), b))
            {
                drawList->AddLine(a, b, gridColor);
            }
        }

        if (m_Selected != NullEntity && m_Project.GetScene().IsAlive(m_Selected))
        {
            Scene& scene = m_Project.GetScene();
            const Registry& registry = scene.GetRegistry();
            if (registry.Has<MeshRendererComponent>(m_Selected))
            {
                const MeshRendererComponent& renderer = registry.Get<MeshRendererComponent>(m_Selected);
                if (renderer.Mesh.IsValid() && renderer.Mesh->Geometry && renderer.Mesh->Geometry->Bounds.IsValid() &&
                    std::isfinite(renderer.Mesh->Geometry->Bounds.Min.x) &&
                    std::isfinite(renderer.Mesh->Geometry->Bounds.Min.y) &&
                    std::isfinite(renderer.Mesh->Geometry->Bounds.Min.z) &&
                    std::isfinite(renderer.Mesh->Geometry->Bounds.Max.x) &&
                    std::isfinite(renderer.Mesh->Geometry->Bounds.Max.y) &&
                    std::isfinite(renderer.Mesh->Geometry->Bounds.Max.z))
                {
                    const Aabb bounds = renderer.Mesh->Geometry->Bounds.Transformed(scene.GetWorldMatrix(m_Selected));
                    if (!std::isfinite(bounds.Min.x) || !std::isfinite(bounds.Min.y) ||
                        !std::isfinite(bounds.Min.z) || !std::isfinite(bounds.Max.x) ||
                        !std::isfinite(bounds.Max.y) || !std::isfinite(bounds.Max.z))
                    {
                        drawList->PopClipRect();
                        return;
                    }
                    const glm::vec3 corners[8] = {
                        { bounds.Min.x, bounds.Min.y, bounds.Min.z }, { bounds.Max.x, bounds.Min.y, bounds.Min.z },
                        { bounds.Max.x, bounds.Max.y, bounds.Min.z }, { bounds.Min.x, bounds.Max.y, bounds.Min.z },
                        { bounds.Min.x, bounds.Min.y, bounds.Max.z }, { bounds.Max.x, bounds.Min.y, bounds.Max.z },
                        { bounds.Max.x, bounds.Max.y, bounds.Max.z }, { bounds.Min.x, bounds.Max.y, bounds.Max.z }
                    };
                    ImVec2 screen[8];
                    bool valid[8]{};
                    for (int i = 0; i < 8; ++i)
                    {
                        valid[i] = toScreen(corners[i], screen[i]);
                    }
                    const int edges[12][2] = { { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 }, { 4, 5 }, { 5, 6 },
                        { 6, 7 }, { 7, 4 }, { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 } };
                    for (const auto& edge : edges)
                    {
                        if (valid[edge[0]] && valid[edge[1]])
                        {
                            drawList->AddLine(screen[edge[0]], screen[edge[1]], IM_COL32(255, 180, 60, 220), 1.5f);
                        }
                    }
                }
            }
        }

        const ImU32 textColor = IM_COL32(225, 230, 240, 230);
        drawList->AddRectFilled(ImVec2(min.x + 8.0f, min.y + 8.0f),
            ImVec2(min.x + 535.0f, min.y + 30.0f), IM_COL32(12, 14, 18, 170), 4.0f);
        drawList->AddText(ImVec2(min.x + 14.0f, min.y + 13.0f), textColor,
            "RMB Look   WASD Move   Q/E Down/Up   1/2/3 Gizmo   F Focus   Del Delete");

        const RendererStats& stats = m_Renderer.GetStats();
        char statsText[160];
        std::snprintf(statsText, sizeof(statsText), "%.1f FPS   %u draws   %u tris",
            ImGui::GetIO().Framerate, stats.DrawCalls, stats.Triangles);
        const ImVec2 statsSize = ImGui::CalcTextSize(statsText);
        drawList->AddRectFilled(ImVec2(max.x - statsSize.x - 24.0f, min.y + 8.0f),
            ImVec2(max.x - 8.0f, min.y + 30.0f), IM_COL32(12, 14, 18, 170), 4.0f);
        drawList->AddText(ImVec2(max.x - statsSize.x - 16.0f, min.y + 13.0f), textColor, statsText);

        if (m_Selected != NullEntity)
        {
            const std::string name = m_Project.GetScene().GetRegistry().Get<NameComponent>(m_Selected).Name;
            std::string label = "Selected: " + name + "  (" + std::to_string(m_Selected.Index()) + ")";
            if (m_StatusTimer > 0.0f && !m_StatusMessage.empty())
            {
                label += "    " + m_StatusMessage;
            }
            drawList->AddText(ImVec2(min.x + 12.0f, max.y - 24.0f), IM_COL32(255, 200, 120, 240), label.c_str());
        }
        else if (m_StatusTimer > 0.0f && !m_StatusMessage.empty())
        {
            drawList->AddText(ImVec2(min.x + 12.0f, max.y - 24.0f), IM_COL32(255, 200, 120, 240),
                m_StatusMessage.c_str());
        }

        drawList->PopClipRect();
    }

    void EditorApp::DrawGizmo()
    {
        if (m_Selected == NullEntity || !m_Project.GetScene().IsAlive(m_Selected))
        {
            m_ActiveGizmoAxis = -1;
            m_GizmoHovered = false;
            return;
        }

        Scene& scene = m_Project.GetScene();
        Registry& registry = scene.GetRegistry();
        const Transform world = scene.GetWorldTransform(m_Selected);
        const glm::vec3 center = world.Translation;
        if (!std::isfinite(center.x) || !std::isfinite(center.y) || !std::isfinite(center.z))
        {
            m_ActiveGizmoAxis = -1;
            m_GizmoHovered = false;
            return;
        }
        const glm::vec2 viewportSize(m_ViewportWidth, m_ViewportHeight);
        glm::vec2 centerScreen;
        if (!m_EditorCamera.WorldToScreen(center, viewportSize, centerScreen))
        {
            m_ActiveGizmoAxis = -1;
            m_GizmoHovered = false;
            return;
        }
        const float screenMargin = 4.0f * (std::max)(viewportSize.x, viewportSize.y);
        if (!std::isfinite(centerScreen.x) || !std::isfinite(centerScreen.y) ||
            std::abs(centerScreen.x) > screenMargin || std::abs(centerScreen.y) > screenMargin)
        {
            m_ActiveGizmoAxis = -1;
            m_GizmoHovered = false;
            return;
        }
        const ImVec2 origin(m_ViewportX + centerScreen.x, m_ViewportY + centerScreen.y);
        const float distance = glm::length(m_EditorCamera.GetPosition() - center);
        if (!std::isfinite(distance))
        {
            m_ActiveGizmoAxis = -1;
            m_GizmoHovered = false;
            return;
        }
        const float axisLength = std::clamp(distance * 0.15f, 0.35f, 12.0f);
        const glm::vec3 axes[3] = { glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f),
            glm::vec3(0.0f, 0.0f, 1.0f) };
        const ImU32 colors[3] = { IM_COL32(235, 70, 70, 255), IM_COL32(80, 220, 100, 255),
            IM_COL32(80, 140, 255, 255) };
        const ImU32 activeColor = IM_COL32(255, 220, 80, 255);

        ImDrawList* drawList = ImGui::GetForegroundDrawList();
        const ImVec2 clipMin(m_ViewportX, m_ViewportY);
        const ImVec2 clipMax(m_ViewportX + m_ViewportWidth, m_ViewportY + m_ViewportHeight);
        drawList->PushClipRect(clipMin, clipMax, true);

        const ImVec2 mouse = ImGui::GetMousePos();
        m_GizmoHovered = false;
        int hoveredAxis = -1;
        float bestDistance = 9.0f;
        ImVec2 axisEnd[3];
        ImVec2 axisDirection[3];
        bool axisValid[3]{};

        for (int axis = 0; axis < 3; ++axis)
        {
            glm::vec2 endScreen;
            const glm::vec3 endWorld = center + axes[axis] * axisLength;
            if (!m_EditorCamera.WorldToScreen(endWorld, viewportSize, endScreen) ||
                !std::isfinite(endScreen.x) || !std::isfinite(endScreen.y) ||
                std::abs(endScreen.x) > screenMargin || std::abs(endScreen.y) > screenMargin)
            {
                continue;
            }
            axisEnd[axis] = ImVec2(m_ViewportX + endScreen.x, m_ViewportY + endScreen.y);
            axisDirection[axis] = ImVec2(axisEnd[axis].x - origin.x, axisEnd[axis].y - origin.y);
            axisValid[axis] = true;
            const ImU32 color = m_ActiveGizmoAxis == axis ? activeColor : colors[axis];

            if (m_GizmoMode == GizmoMode::Rotate)
            {
                const glm::vec3 basisU = axis == 0 ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
                const glm::vec3 basisV = axis == 2 ? glm::vec3(0, 1, 0) : glm::vec3(0, 0, 1);
                constexpr int segments = 48;
                ImVec2 previous;
                bool previousValid = false;
                for (int segment = 0; segment <= segments; ++segment)
                {
                    const float angle = glm::two_pi<float>() * static_cast<float>(segment) / segments;
                    const glm::vec3 point = center + (basisU * std::cos(angle) + basisV * std::sin(angle)) * axisLength;
                    glm::vec2 pointScreen;
                    if (!m_EditorCamera.WorldToScreen(point, viewportSize, pointScreen) ||
                        !std::isfinite(pointScreen.x) || !std::isfinite(pointScreen.y) ||
                        std::abs(pointScreen.x) > screenMargin || std::abs(pointScreen.y) > screenMargin)
                    {
                        previousValid = false;
                        continue;
                    }
                    const ImVec2 current(m_ViewportX + pointScreen.x, m_ViewportY + pointScreen.y);
                    if (previousValid)
                    {
                        drawList->AddLine(previous, current, color, 2.0f);
                        const float segmentDistance = DistanceToSegment(mouse, previous, current);
                        if (segmentDistance < bestDistance)
                        {
                            bestDistance = segmentDistance;
                            hoveredAxis = axis;
                        }
                    }
                    previous = current;
                    previousValid = true;
                }
            }
            else
            {
                drawList->AddLine(origin, axisEnd[axis], color, 2.5f);
                const ImVec2 direction = ImVec2(axisDirection[axis].x, axisDirection[axis].y);
                const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
                if (length > 1.0f)
                {
                    const ImVec2 normal(direction.x / length, direction.y / length);
                    const ImVec2 perpendicular(-normal.y, normal.x);
                    const ImVec2 tip(axisEnd[axis].x + normal.x * 8.0f, axisEnd[axis].y + normal.y * 8.0f);
                    const ImVec2 base(axisEnd[axis].x - normal.x * 9.0f, axisEnd[axis].y - normal.y * 9.0f);
                    drawList->AddTriangleFilled(tip, ImVec2(base.x + perpendicular.x * 5.0f,
                        base.y + perpendicular.y * 5.0f), ImVec2(base.x - perpendicular.x * 5.0f,
                        base.y - perpendicular.y * 5.0f), color);
                }
                const float segmentDistance = DistanceToSegment(mouse, origin, axisEnd[axis]);
                if (segmentDistance < bestDistance)
                {
                    bestDistance = segmentDistance;
                    hoveredAxis = axis;
                }
            }
        }

        m_GizmoHovered = hoveredAxis >= 0;
        if (m_ActiveGizmoAxis < 0 && m_GizmoHovered && m_ViewportHovered &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            m_ActiveGizmoAxis = hoveredAxis;
            m_GizmoStartMouse = glm::vec2(mouse.x, mouse.y);
            m_GizmoStartTranslation = world.Translation;
            const TransformComponent& transform = registry.Get<TransformComponent>(m_Selected);
            m_GizmoStartScale = transform.Local.Scale;
            m_GizmoStartRotation = transform.Local.Rotation;
        }

        if (m_ActiveGizmoAxis >= 0)
        {
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                m_ActiveGizmoAxis = -1;
            }
            else if (axisValid[m_ActiveGizmoAxis])
            {
                const ImVec2 delta(mouse.x - m_GizmoStartMouse.x, mouse.y - m_GizmoStartMouse.y);
                const ImVec2 axisScreen = axisDirection[m_ActiveGizmoAxis];
                const float screenLength = std::sqrt(axisScreen.x * axisScreen.x + axisScreen.y * axisScreen.y);
                if (screenLength > 1.0f)
                {
                    const float pixels = (delta.x * axisScreen.x + delta.y * axisScreen.y) / screenLength;
                    const float worldDelta = pixels * (axisLength / screenLength);
                    if (m_GizmoMode == GizmoMode::Translate)
                    {
                        Transform translated = scene.GetWorldTransform(m_Selected);
                        translated.Translation = m_GizmoStartTranslation + axes[m_ActiveGizmoAxis] * worldDelta;
                        scene.SetWorldTransform(m_Selected, translated);
                    }
                    else if (m_GizmoMode == GizmoMode::Scale)
                    {
                        TransformComponent& transform = registry.Get<TransformComponent>(m_Selected);
                        const float scaleDelta = worldDelta / std::max(axisLength, 0.001f);
                        transform.Local.Scale[m_ActiveGizmoAxis] =
                            std::max(0.01f, m_GizmoStartScale[m_ActiveGizmoAxis] + scaleDelta);
                    }
                    else
                    {
                        TransformComponent& transform = registry.Get<TransformComponent>(m_Selected);
                        transform.Local.Rotation[m_ActiveGizmoAxis] =
                            m_GizmoStartRotation[m_ActiveGizmoAxis] + delta.x * 0.6f - delta.y * 0.6f;
                    }
                    m_SceneDirty = true;
                }
            }
        }

        drawList->PopClipRect();
    }

    void EditorApp::DrawHierarchy()
    {
        ImGui::SetNextWindowPos(ImVec2(m_PanelGap, m_ViewportY), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(m_LeftPanelWidth, m_ViewportHeight), ImGuiCond_Always);
        ImGui::Begin("Hierarchy", &m_ShowHierarchy, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);

        Scene& scene = m_Project.GetScene();
        ImGui::TextDisabled("SCENE");
        ImGui::SameLine();
        ImGui::TextDisabled("(%zu)", scene.GetEntities().size());
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::BeginDisabled(m_Playing);
        if (ImGui::Button("+ Empty", ImVec2(-1.0f, 0.0f)))
        {
            CreateEmpty();
        }
        if (ImGui::Button("+ Cube", ImVec2(74.0f, 0.0f)))
        {
            CreateMeshEntity("Cube", "assets/models/cube.obj", glm::vec3(1.0f));
        }
        ImGui::SameLine();
        if (ImGui::Button("+ Sphere", ImVec2(86.0f, 0.0f)))
        {
            CreateMeshEntity("Sphere", "assets/models/sphere.obj", glm::vec3(1.0f));
        }
        ImGui::SameLine();
        if (ImGui::Button("+ Plane", ImVec2(74.0f, 0.0f)))
        {
            CreateMeshEntity("Plane", "assets/models/plane.obj", glm::vec3(4.0f, 1.0f, 4.0f));
        }
        if (ImGui::Button("+ Camera", ImVec2(96.0f, 0.0f)))
        {
            CreateCameraEntity();
        }
        ImGui::SameLine();
        if (ImGui::Button("+ Light", ImVec2(96.0f, 0.0f)))
        {
            CreateLightEntity(true);
        }
        ImGui::EndDisabled();
        ImGui::Separator();

        ImGui::BeginChild("HierarchyEntities", ImVec2(0.0f, 0.0f), false,
            ImGuiWindowFlags_HorizontalScrollbar);
        const std::vector<Entity> roots = scene.GetRootEntities();
        if (roots.empty())
        {
            ImGui::TextDisabled("Scene is empty.");
        }
        for (Entity entity : roots)
        {
            DrawEntityNode(entity);
        }
        ImGui::EndChild();
        ImGui::End();
    }

    void EditorApp::DrawEntityNode(Entity entity)
    {
        Scene& scene = m_Project.GetScene();
        Registry& registry = scene.GetRegistry();
        const NameComponent& name = registry.Get<NameComponent>(entity);
        const bool hasChildren = !scene.GetChildren(entity).empty();

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnArrow;
        if (!hasChildren)
        {
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        }
        if (entity == m_Selected)
        {
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        const bool open = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<uintptr_t>(entity.Raw())), flags,
            "%s", name.Name.c_str());
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
        {
            SelectEntity(entity);
        }
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            FocusSelected();
        }

        if (ImGui::BeginDragDropSource())
        {
            ImGui::SetDragDropPayload(kEntityPayload, &entity, sizeof(Entity));
            ImGui::TextUnformatted(name.Name.c_str());
            ImGui::EndDragDropSource();
        }
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kEntityPayload))
            {
                if (payload->DataSize == static_cast<int>(sizeof(Entity)))
                {
                    const Entity dragged = *static_cast<const Entity*>(payload->Data);
                    if (dragged != entity && dragged != NullEntity)
                    {
                        scene.SetParent(dragged, entity, true);
                        m_SceneDirty = true;
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }

        if (open && hasChildren)
        {
            for (Entity child : scene.GetChildren(entity))
            {
                DrawEntityNode(child);
            }
            ImGui::TreePop();
        }
    }

    void EditorApp::DrawInspector()
    {
        ImGui::SetNextWindowPos(ImVec2(m_ViewportX + m_ViewportWidth + m_PanelGap, m_ViewportY),
            ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(m_RightPanelWidth, m_ViewportHeight), ImGuiCond_Always);
        ImGui::Begin("Inspector", &m_ShowInspector, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);

        if (m_Selected == NullEntity || !m_Project.GetScene().IsAlive(m_Selected))
        {
            const ImVec2 available = ImGui::GetContentRegionAvail();
            const char* hint = "Select an entity to inspect";
            const ImVec2 textSize = ImGui::CalcTextSize(hint);
            ImGui::SetCursorPos(ImVec2(std::max(12.0f, (available.x - textSize.x) * 0.5f),
                ImGui::GetCursorPosY() + available.y * 0.35f));
            ImGui::TextDisabled("%s", hint);
            ImGui::End();
            return;
        }

        ImGui::BeginDisabled(m_Playing);
        Scene& scene = m_Project.GetScene();
        Registry& registry = scene.GetRegistry();
        NameComponent& name = registry.Get<NameComponent>(m_Selected);
        ImGui::TextDisabled("ENTITY");
        ImGui::SameLine();
        ImGui::TextDisabled("%s", m_Selected.ToString().c_str());
        if (ImGui::InputText("Name", &name.Name))
        {
            m_SceneDirty = true;
        }
        if (registry.Has<IDComponent>(m_Selected))
        {
            ImGui::TextDisabled("UUID %s", registry.Get<IDComponent>(m_Selected).Id.ToString().c_str());
        }
        ImGui::Spacing();
        ImGui::Separator();

        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
        {
            TransformComponent& transform = registry.Get<TransformComponent>(m_Selected);
            bool changed = false;
            DrawVec3("Position", transform.Local.Translation, 0.1f, &changed);
            DrawVec3("Rotation", transform.Local.Rotation, 0.5f, &changed);
            DrawVec3("Scale", transform.Local.Scale, 0.05f, &changed);
            m_SceneDirty |= changed;
        }

        if (registry.Has<CameraComponent>(m_Selected))
        {
            if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
            {
                CameraComponent& camera = registry.Get<CameraComponent>(m_Selected);
                bool changed = ImGui::Checkbox("Primary", &camera.Primary);
                changed |= ImGui::Checkbox("Perspective", &camera.Perspective);
                if (camera.Perspective)
                {
                    changed |= ImGui::DragFloat("Field of View", &camera.FieldOfView, 0.5f, 1.0f, 179.0f, "%.1f");
                }
                else
                {
                    changed |= ImGui::DragFloat("Orthographic Size", &camera.OrthographicSize, 0.1f, 0.1f,
                        1000.0f, "%.2f");
                }
                changed |= ImGui::DragFloat("Near", &camera.NearPlane, 0.01f, 0.001f, 100.0f, "%.3f");
                changed |= ImGui::DragFloat("Far", &camera.FarPlane, 1.0f, 0.1f, 100000.0f, "%.1f");
                changed |= ImGui::DragFloat("Exposure", &camera.Exposure, 0.01f, 0.01f, 10.0f, "%.2f");
                m_SceneDirty |= changed;
                if (ImGui::SmallButton("Remove Camera"))
                {
                    registry.Remove<CameraComponent>(m_Selected);
                    m_SceneDirty = true;
                }
            }
        }

        if (registry.Has<DirectionalLightComponent>(m_Selected))
        {
            if (ImGui::CollapsingHeader("Directional Light", ImGuiTreeNodeFlags_DefaultOpen))
            {
                DirectionalLightComponent& light = registry.Get<DirectionalLightComponent>(m_Selected);
                bool changed = ImGui::ColorEdit3("Color", &light.Color.x);
                changed |= ImGui::DragFloat("Intensity", &light.Intensity, 0.05f, 0.0f, 100.0f, "%.2f");
                changed |= ImGui::Checkbox("Cast Shadows", &light.CastShadows);
                m_SceneDirty |= changed;
                if (ImGui::SmallButton("Remove Light"))
                {
                    registry.Remove<DirectionalLightComponent>(m_Selected);
                    m_SceneDirty = true;
                }
            }
        }

        if (registry.Has<PointLightComponent>(m_Selected))
        {
            if (ImGui::CollapsingHeader("Point Light", ImGuiTreeNodeFlags_DefaultOpen))
            {
                PointLightComponent& light = registry.Get<PointLightComponent>(m_Selected);
                bool changed = ImGui::ColorEdit3("Color", &light.Color.x);
                changed |= ImGui::DragFloat("Intensity", &light.Intensity, 0.1f, 0.0f, 1000.0f, "%.2f");
                changed |= ImGui::DragFloat("Range", &light.Range, 0.1f, 0.01f, 1000.0f, "%.2f");
                m_SceneDirty |= changed;
                if (ImGui::SmallButton("Remove Light"))
                {
                    registry.Remove<PointLightComponent>(m_Selected);
                    m_SceneDirty = true;
                }
            }
        }

        if (registry.Has<SpotLightComponent>(m_Selected))
        {
            if (ImGui::CollapsingHeader("Spot Light", ImGuiTreeNodeFlags_DefaultOpen))
            {
                SpotLightComponent& light = registry.Get<SpotLightComponent>(m_Selected);
                bool changed = ImGui::ColorEdit3("Color", &light.Color.x);
                changed |= ImGui::DragFloat("Intensity", &light.Intensity, 0.1f, 0.0f, 1000.0f, "%.2f");
                changed |= ImGui::DragFloat("Range", &light.Range, 0.1f, 0.01f, 1000.0f, "%.2f");
                changed |= ImGui::DragFloat("Inner Angle", &light.InnerAngle, 0.5f, 0.0f, 89.0f, "%.1f");
                changed |= ImGui::DragFloat("Outer Angle", &light.OuterAngle, 0.5f, 0.0f, 89.0f, "%.1f");
                m_SceneDirty |= changed;
                if (ImGui::SmallButton("Remove Light"))
                {
                    registry.Remove<SpotLightComponent>(m_Selected);
                    m_SceneDirty = true;
                }
            }
        }

        if (registry.Has<ParticleEmitterComponent>(m_Selected))
        {
            if (ImGui::CollapsingHeader("Particle Emitter", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ParticleEmitterComponent& emitter = registry.Get<ParticleEmitterComponent>(m_Selected);
                bool changed = ImGui::Checkbox("Emitting", &emitter.Emitting);
                changed |= ImGui::DragFloat("Emission Rate", &emitter.EmissionRate, 1.0f, 0.0f, 2000.0f,
                    "%.1f /s");
                changed |= ImGui::DragFloat("Lifetime", &emitter.Lifetime, 0.05f, 0.01f, 60.0f, "%.2f s");
                changed |= ImGui::DragFloat("Start Size", &emitter.StartSize, 0.01f, 0.0f, 20.0f, "%.3f");
                changed |= ImGui::DragFloat("End Size", &emitter.EndSize, 0.01f, 0.0f, 20.0f, "%.3f");
                changed |= ImGui::ColorEdit4("Start Color", &emitter.StartColor.x);
                changed |= ImGui::ColorEdit4("End Color", &emitter.EndColor.x);
                changed |= ImGui::DragFloat3("Velocity", &emitter.Velocity.x, 0.05f);
                changed |= ImGui::DragFloat("Velocity Spread", &emitter.VelocitySpread, 0.01f, 0.0f, 40.0f);
                changed |= ImGui::DragFloat3("Gravity", &emitter.Gravity.x, 0.02f);
                changed |= ImGui::DragFloat3("Offset", &emitter.Offset.x, 0.02f);
                int maxParticles = static_cast<int>(emitter.MaxParticles);
                if (ImGui::DragInt("Max Particles", &maxParticles, 8.0f, 1, 4096))
                {
                    emitter.MaxParticles = static_cast<uint32_t>(std::clamp(maxParticles, 1, 4096));
                    changed = true;
                }
                ImGui::TextDisabled("Live Particles: %zu", emitter.Particles.size());
                m_SceneDirty |= changed;
                if (ImGui::SmallButton("Remove Particle Emitter"))
                {
                    registry.Remove<ParticleEmitterComponent>(m_Selected);
                    m_SceneDirty = true;
                }
            }
        }

        if (registry.Has<MeshRendererComponent>(m_Selected))
        {
            if (ImGui::CollapsingHeader("Mesh Renderer", ImGuiTreeNodeFlags_DefaultOpen))
            {
                MeshRendererComponent& renderer = registry.Get<MeshRendererComponent>(m_Selected);
                if (renderer.Mesh.IsValid())
                {
                    ImGui::TextDisabled("Mesh: %s", renderer.Mesh.Path.c_str());
                    if (renderer.Mesh->Geometry)
                    {
                        ImGui::TextDisabled("Vertices: %zu  Indices: %zu",
                            renderer.Mesh->Geometry->Vertices.size(), renderer.Mesh->Geometry->Indices.size());
                    }
                }
                else
                {
                    ImGui::TextDisabled("Mesh: <none>");
                }
                bool changed = ImGui::Checkbox("Visible", &renderer.Visible);
                changed |= ImGui::Checkbox("Cast Shadows", &renderer.CastShadows);
                changed |= ImGui::Checkbox("Receive Shadows", &renderer.ReceiveShadows);
                m_SceneDirty |= changed;

                if (renderer.Materials.empty() && renderer.Mesh.IsValid())
                {
                    renderer.Materials = renderer.Mesh->Materials;
                }
                for (size_t i = 0; i < renderer.Materials.size(); ++i)
                {
                    Material& material = renderer.Materials[i];
                    ImGui::PushID(static_cast<int>(i));
                    const std::string header = "Material " + std::to_string(i) + " - " + material.Name;
                    if (ImGui::CollapsingHeader(header.c_str()))
                    {
                        bool materialChanged = ImGui::InputText("Name", &material.Name);
                        materialChanged |= ImGui::ColorEdit4("Base Color", &material.BaseColor.x);
                        materialChanged |= ImGui::DragFloat("Metallic", &material.Metallic, 0.01f, 0.0f, 1.0f,
                            "%.2f");
                        materialChanged |= ImGui::DragFloat("Roughness", &material.Roughness, 0.01f, 0.0f, 1.0f,
                            "%.2f");
                        materialChanged |= ImGui::ColorEdit3("Emissive", &material.Emissive.x);
                        materialChanged |= ImGui::DragFloat("Emissive Strength", &material.EmissiveStrength, 0.05f,
                            0.0f, 100.0f, "%.2f");
                        materialChanged |= ImGui::DragFloat("Opacity", &material.Opacity, 0.01f, 0.0f, 1.0f, "%.2f");
                        materialChanged |= ImGui::Checkbox("Double Sided", &material.DoubleSided);
                        if (!material.AlbedoMap.Path.empty())
                        {
                            ImGui::TextDisabled("Albedo: %s", material.AlbedoMap.Path.c_str());
                        }
                        m_SceneDirty |= materialChanged;
                        if (renderer.Materials.size() > 1 && ImGui::SmallButton("Remove Material"))
                        {
                            renderer.Materials.erase(renderer.Materials.begin() + static_cast<long long>(i));
                            m_SceneDirty = true;
                            ImGui::PopID();
                            break;
                        }
                    }
                    ImGui::PopID();
                }
                if (ImGui::SmallButton("Remove Mesh Renderer"))
                {
                    registry.Remove<MeshRendererComponent>(m_Selected);
                    m_SceneDirty = true;
                }
            }
        }

        if (registry.Has<RigidBodyComponent>(m_Selected))
        {
            if (ImGui::CollapsingHeader("Rigid Body", ImGuiTreeNodeFlags_DefaultOpen))
            {
                RigidBodyComponent& body = registry.Get<RigidBodyComponent>(m_Selected);
                static const char* bodyTypes[] = { "Static", "Kinematic", "Dynamic" };
                int type = static_cast<int>(body.Type);
                bool changed = ImGui::Combo("Type", &type, bodyTypes, 3);
                body.Type = static_cast<BodyType>(type);
                changed |= ImGui::DragFloat("Mass", &body.Mass, 0.01f, 0.001f, 10000.0f, "%.2f");
                changed |= ImGui::DragFloat("Restitution", &body.Restitution, 0.01f, 0.0f, 1.0f, "%.2f");
                changed |= ImGui::DragFloat("Friction", &body.Friction, 0.01f, 0.0f, 2.0f, "%.2f");
                changed |= ImGui::Checkbox("Use Gravity", &body.UseGravity);
                changed |= ImGui::DragFloat3("Velocity", &body.LinearVelocity.x, 0.05f);
                m_SceneDirty |= changed;
                if (ImGui::SmallButton("Remove Rigid Body"))
                {
                    registry.Remove<RigidBodyComponent>(m_Selected);
                    m_SceneDirty = true;
                }
            }
        }

        if (registry.Has<BoxColliderComponent>(m_Selected))
        {
            if (ImGui::CollapsingHeader("Box Collider", ImGuiTreeNodeFlags_DefaultOpen))
            {
                BoxColliderComponent& collider = registry.Get<BoxColliderComponent>(m_Selected);
                bool changed = ImGui::DragFloat3("Half Extents", &collider.HalfExtents.x, 0.01f);
                changed |= ImGui::DragFloat3("Center", &collider.Center.x, 0.01f);
                changed |= ImGui::Checkbox("Trigger", &collider.IsTrigger);
                m_SceneDirty |= changed;
                if (ImGui::SmallButton("Remove Box Collider"))
                {
                    registry.Remove<BoxColliderComponent>(m_Selected);
                    m_SceneDirty = true;
                }
            }
        }

        if (registry.Has<SphereColliderComponent>(m_Selected))
        {
            if (ImGui::CollapsingHeader("Sphere Collider", ImGuiTreeNodeFlags_DefaultOpen))
            {
                SphereColliderComponent& collider = registry.Get<SphereColliderComponent>(m_Selected);
                bool changed = ImGui::DragFloat("Radius", &collider.Radius, 0.01f, 0.01f, 1000.0f, "%.2f");
                changed |= ImGui::DragFloat3("Center", &collider.Center.x, 0.01f);
                changed |= ImGui::Checkbox("Trigger", &collider.IsTrigger);
                m_SceneDirty |= changed;
                if (ImGui::SmallButton("Remove Sphere Collider"))
                {
                    registry.Remove<SphereColliderComponent>(m_Selected);
                    m_SceneDirty = true;
                }
            }
        }

        if (registry.Has<ScriptComponent>(m_Selected))
        {
            if (ImGui::CollapsingHeader("Scripts", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ScriptComponent& scripts = registry.Get<ScriptComponent>(m_Selected);
                for (size_t i = 0; i < scripts.Scripts.size(); ++i)
                {
                    ImGui::PushID(static_cast<int>(i));
                    ImGui::InputText("Path", &scripts.Scripts[i].Path);
                    ImGui::Checkbox("Enabled", &scripts.Scripts[i].Enabled);
                    if (ImGui::SmallButton("Remove Script"))
                    {
                        scripts.Scripts.erase(scripts.Scripts.begin() + static_cast<long long>(i));
                        m_SceneDirty = true;
                        ImGui::PopID();
                        break;
                    }
                    ImGui::PopID();
                }
                if (ImGui::SmallButton("Add Script"))
                {
                    scripts.Scripts.push_back(ScriptEntry{ "assets/scripts/spin.lua", true });
                    m_SceneDirty = true;
                }
                if (ImGui::SmallButton("Remove Script Component"))
                {
                    registry.Remove<ScriptComponent>(m_Selected);
                    m_SceneDirty = true;
                }
            }
        }

        if (registry.Has<AudioListenerComponent>(m_Selected))
        {
            if (ImGui::CollapsingHeader("Audio Listener", ImGuiTreeNodeFlags_DefaultOpen))
            {
                AudioListenerComponent& listener = registry.Get<AudioListenerComponent>(m_Selected);
                ImGui::Checkbox("Primary", &listener.Primary);
                if (ImGui::SmallButton("Remove Audio Listener"))
                {
                    registry.Remove<AudioListenerComponent>(m_Selected);
                    m_SceneDirty = true;
                }
            }
        }

        ImGui::Separator();
        if (ImGui::Button("Add Component", ImVec2(-1.0f, 0.0f)))
        {
            ImGui::OpenPopup("AddComponentPopup");
        }
        if (ImGui::BeginPopup("AddComponentPopup"))
        {
            if (ImGui::MenuItem("Camera"))
            {
                AddComponent("Camera");
            }
            if (ImGui::MenuItem("Mesh Renderer"))
            {
                AddComponent("MeshRenderer");
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Directional Light"))
            {
                AddComponent("DirectionalLight");
            }
            if (ImGui::MenuItem("Point Light"))
            {
                AddComponent("PointLight");
            }
            if (ImGui::MenuItem("Spot Light"))
            {
                AddComponent("SpotLight");
            }
            if (ImGui::MenuItem("Particle Emitter"))
            {
                AddComponent("ParticleEmitter");
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Rigid Body"))
            {
                AddComponent("RigidBody");
            }
            if (ImGui::MenuItem("Box Collider"))
            {
                AddComponent("BoxCollider");
            }
            if (ImGui::MenuItem("Sphere Collider"))
            {
                AddComponent("SphereCollider");
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Scripts"))
            {
                AddComponent("Scripts");
            }
            if (ImGui::MenuItem("Audio Listener"))
            {
                AddComponent("AudioListener");
            }
            ImGui::EndPopup();
        }

        ImGui::Separator();
        if (ImGui::Button("Delete Entity", ImVec2(-1.0f, 0.0f)))
        {
            DeleteSelected();
        }
        ImGui::EndDisabled();
        ImGui::End();
    }

    bool EditorApp::RunSceneScript(const std::string& path, std::string* error)
    {
        if (!m_Project.IsOpen())
        {
            if (error != nullptr)
            {
                *error = "No project is open";
            }
            return false;
        }

        const std::optional<std::string> source = FileSystem::ReadTextFile(std::filesystem::path(path));
        if (!source)
        {
            if (error != nullptr)
            {
                *error = "Could not read scene script: " + path;
            }
            return false;
        }

        std::string scriptError;
        if (!m_Scripts.Initialize(m_Project.GetScene(), &scriptError))
        {
            if (error != nullptr)
            {
                *error = "Could not initialize Lua: " + scriptError;
            }
            return false;
        }
        m_Scripts.SetAssetManager(&m_Project.GetAssets());
        m_Scripts.SetPhysics(&m_Physics);
        m_Scripts.SetKeyDownCallback([this](std::string_view key)
        {
            return m_Window.IsKeyDown(Window::KeyFromName(key));
        });

        if (!m_Scripts.RunString(*source, &scriptError))
        {
            if (error != nullptr)
            {
                *error = scriptError;
            }
            return false;
        }

        if (!m_Project.SaveScene(&scriptError))
        {
            if (error != nullptr)
            {
                *error = "Could not save the scene after the script: " + scriptError;
            }
            return false;
        }

        ASTER_LOG_INFO("Editor", "Scene script applied and saved: %s", path.c_str());
        return true;
    }

    bool EditorApp::ExportGame(const std::filesystem::path& outputDirectory, std::string* error)
    {
        if (!m_Project.IsOpen())
        {
            if (error != nullptr)
            {
                *error = "Open a project before exporting";
            }
            return false;
        }
        if (m_Playing)
        {
            StopPlay();
        }
        if (!m_Project.SaveScene(error))
        {
            return false;
        }
        m_SceneDirty = false;

        GameExportOptions options;
        options.OutputDirectory = outputDirectory;
        options.PlayerExecutable = m_Arguments.PlayerPath.empty()
            ? FileSystem::ExecutableDirectory() / "AsterPlayer.exe"
            : FileSystem::Absolute(m_Arguments.PlayerPath);
        options.ShaderDirectory = FileSystem::ExecutableDirectory() / "shaders";
        if (!FileSystem::Exists(options.ShaderDirectory / "Pbr.vs.dxil"))
        {
            options.ShaderDirectory = ASTER_SHADER_DIR;
        }
        if (!GameExporter::Export(m_Project, options, error))
        {
            return false;
        }

        SetStatus("Game exported to " + FileSystem::Normalize(options.OutputDirectory));
        ASTER_LOG_INFO("Editor", "Game exported to %s",
            FileSystem::Normalize(options.OutputDirectory).c_str());
        return true;
    }

    void EditorApp::DrawExportPopup()
    {
        if (m_OpenExportPopup)
        {
            ImGui::OpenPopup("Export Game");
            m_OpenExportPopup = false;
        }
        if (ImGui::BeginPopupModal("Export Game", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextUnformatted("Build a self-contained directory with the player, shaders, and project data.");
            ImGui::InputText("Output Directory", &m_ExportPath);
            if (!m_ExportError.empty())
            {
                ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.35f, 1.0f), "%s", m_ExportError.c_str());
            }

            if (ImGui::Button("Export", ImVec2(120.0f, 0.0f)))
            {
                std::string error;
                if (ExportGame(m_ExportPath, &error))
                {
                    m_ExportError.clear();
                    ImGui::CloseCurrentPopup();
                }
                else
                {
                    m_ExportError = error;
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
            {
                m_ExportError.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    void EditorApp::DrawConsole()
    {
        const ImGuiIO& io = ImGui::GetIO();
        const float consoleTop = io.DisplaySize.y - m_StatusBarHeight - m_ConsolePanelHeight - m_PanelGap;
        ImGui::SetNextWindowPos(ImVec2(m_PanelGap, consoleTop), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(std::max(1.0f, io.DisplaySize.x - m_PanelGap * 2.0f),
            m_ConsolePanelHeight), ImGuiCond_Always);
        // The editor owns this layout, so moving or resizing the window must not fight the forced
        // size on the next frame (that fight is visible as edge/title-bar flicker).
        ImGui::Begin("Console", &m_ShowConsole, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);
        const ImVec2 available = ImGui::GetContentRegionAvail();
        const float browserWidth = std::max(420.0f, available.x * 0.58f);
        ImGui::BeginChild("##ContentBrowserPane", ImVec2(browserWidth, available.y), true);
        DrawContentBrowser();
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("##ConsolePane", ImVec2(0.0f, available.y), true);

        static const char* kLevels[] = { "Trace", "Debug", "Info", "Warn", "Error", "Critical" };
        int levelIndex = static_cast<int>(m_ConsoleLevel);
        ImGui::SetNextItemWidth(92.0f);
        if (ImGui::Combo("##ConsoleLevel", &levelIndex, kLevels, IM_ARRAYSIZE(kLevels)))
        {
            m_ConsoleLevel = static_cast<LogLevel>(levelIndex);
            Log::SetLevel(m_ConsoleLevel);
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Clear"))
        {
            m_ConsoleMessages.clear();
        }
        ImGui::SameLine();
        ImGui::Checkbox("Auto", &m_ConsoleAutoScroll);
        ImGui::SameLine();
        size_t visibleMessages = 0;
        for (const LogMessage& message : m_ConsoleMessages)
        {
            if (message.Level >= m_ConsoleLevel)
            {
                ++visibleMessages;
            }
        }
        ImGui::TextDisabled("%zu / %zu", visibleMessages, m_ConsoleMessages.size());
        ImGui::Separator();
        ImGui::BeginChild("ConsoleMessages", ImVec2(0.0f, 0.0f), false);
        for (const LogMessage& message : m_ConsoleMessages)
        {
            if (message.Level < m_ConsoleLevel)
            {
                continue;
            }
            const ImVec4 color = ColorForLevel(message.Level);
            const std::string_view levelName = Log::LevelToString(message.Level);
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::TextWrapped("[%.*s] [%s] %s", static_cast<int>(levelName.size()), levelName.data(),
                message.Category.c_str(), message.Text.c_str());
            ImGui::PopStyleColor();
        }
        if (m_ConsoleAutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f)
        {
            ImGui::SetScrollHereY(1.0f);
        }
        ImGui::EndChild();
        ImGui::EndChild();
        ImGui::End();
    }

    void EditorApp::DrawContentBrowser()
    {
        if (!m_ContentBrowser.IsValid())
        {
            ImGui::TextDisabled("CONTENT BROWSER");
            ImGui::Separator();
            ImGui::TextDisabled("No project directory is loaded.");
            if (!m_ContentError.empty())
            {
                ImGui::TextColored(ImVec4(0.95f, 0.4f, 0.4f, 1.0f), "%s", m_ContentError.c_str());
            }
            return;
        }

        const bool atRoot = m_ContentBrowser.GetCurrentPath().empty();
        ImGui::BeginDisabled(atRoot);
        if (ImGui::SmallButton("Up"))
        {
            (void)m_ContentBrowser.NavigateUp();
            m_ContentSelection.clear();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::SmallButton("Refresh"))
        {
            m_ContentBrowser.Refresh();
            m_ContentError = m_ContentBrowser.GetLastError();
            ASTER_LOG_DEBUG("ContentBrowser", "Refreshed %s",
                m_ContentBrowser.GetCurrentPath().empty() ? "assets" :
                m_ContentBrowser.GetCurrentPath().c_str());
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("New Folder"))
        {
            m_NewFolderName = "New Folder";
            m_OpenNewFolderPopup = true;
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(std::max(120.0f, ImGui::GetContentRegionAvail().x - 4.0f));
        if (ImGui::InputTextWithHint("##AssetSearch", "Search assets...", &m_ContentFilter))
        {
            m_ContentBrowser.SetFilter(m_ContentFilter);
        }

        // Breadcrumb: Project / folder / subfolder. Each crumb navigates directly to that level.
        ImGui::TextDisabled("Project");
        {
            std::string prefix;
            const std::string current = m_ContentBrowser.GetCurrentPath();
            size_t start = 0;
            while (start < current.size())
            {
                const size_t slash = current.find('/', start);
                const std::string part = current.substr(start,
                    slash == std::string::npos ? std::string::npos : slash - start);
                prefix = prefix.empty() ? part : prefix + "/" + part;
                ImGui::SameLine();
                ImGui::TextDisabled("/");
                ImGui::SameLine();
                ImGui::PushID(prefix.c_str());
                if (ImGui::SmallButton(part.c_str()))
                {
                    (void)m_ContentBrowser.NavigateTo(prefix);
                    m_ContentSelection.clear();
                }
                ImGui::PopID();
                if (slash == std::string::npos)
                {
                    break;
                }
                start = slash + 1;
            }
        }

        if (!m_ContentError.empty())
        {
            ImGui::TextColored(ImVec4(0.95f, 0.4f, 0.4f, 1.0f), "%s", m_ContentError.c_str());
        }

        const float treeWidth = std::min(150.0f, std::max(90.0f, ImGui::GetContentRegionAvail().x * 0.22f));
        ImGui::BeginChild("##AssetTree", ImVec2(treeWidth, 0.0f), true);
        ImGuiTreeNodeFlags rootFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth |
            ImGuiTreeNodeFlags_DefaultOpen;
        if (m_ContentBrowser.GetCurrentPath().empty())
        {
            rootFlags |= ImGuiTreeNodeFlags_Selected;
        }
        if (ImGui::TreeNodeEx("##AssetRoot", rootFlags, "Project"))
        {
            if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
            {
                (void)m_ContentBrowser.NavigateTo("");
                m_ContentSelection.clear();
            }
            const std::vector<std::string> children = m_ContentBrowser.GetSubdirectories("");
            for (const std::string& child : children)
            {
                const std::string name = child.substr(child.find_last_of('/') + 1);
                DrawContentTreeNode(name, child, 1);
            }
            ImGui::TreePop();
        }
        else if (ImGui::IsItemClicked())
        {
            (void)m_ContentBrowser.NavigateTo("");
            m_ContentSelection.clear();
        }
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("##AssetList", ImVec2(0.0f, 0.0f), true);
        const std::vector<ContentItem>& items = m_ContentBrowser.GetItems();
        if (items.empty())
        {
            if (m_ContentBrowser.GetFilter().empty())
            {
                ImGui::TextDisabled("This folder is empty.");
            }
            else
            {
                ImGui::TextDisabled("No assets match the filter.");
            }
        }
        else if (ImGui::BeginTable("##AssetTable", 3,
                     ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp |
                         ImGuiTableFlags_ScrollY))
        {
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 3.4f);
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch, 0.9f);
            ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthStretch, 0.7f);
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableHeadersRow();
            for (const ContentItem& item : items)
            {
                ImGui::PushID(item.RelativePath.c_str());
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                const bool selected = m_ContentSelection == item.RelativePath;
                if (ImGui::Selectable(item.Name.c_str(), selected,
                        ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick))
                {
                    m_ContentSelection = item.RelativePath;
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    {
                        ActivateContentItem(item);
                    }
                }
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
                {
                    ImGui::SetDragDropPayload("ASTER_ASSET", item.RelativePath.c_str(),
                        item.RelativePath.size() + 1);
                    ImGui::TextUnformatted(item.Name.c_str());
                    ImGui::TextDisabled("%s", std::string(ContentBrowser::TypeToString(item.Type)).c_str());
                    ImGui::EndDragDropSource();
                }
                if (ImGui::BeginPopupContextItem("##AssetContext"))
                {
                    m_ContextItem = item;
                    DrawContentItemActions(item);
                    ImGui::EndPopup();
                }
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("%s\n%s\n%s", item.RelativePath.c_str(),
                        std::string(ContentBrowser::TypeToString(item.Type)).c_str(),
                        item.IsDirectory ? "Folder" : m_ContentBrowser.GetAssetPath(item).c_str());
                }
                ImGui::TableSetColumnIndex(1);
                ImGui::TextColored(ColorForContentType(item.Type), "%s",
                    std::string(ContentBrowser::TypeToString(item.Type)).c_str());
                ImGui::TableSetColumnIndex(2);
                if (item.IsDirectory)
                {
                    ImGui::TextDisabled("--");
                }
                else if (item.Size >= 1024 * 1024)
                {
                    ImGui::TextDisabled("%.1f MB", static_cast<double>(item.Size) / (1024.0 * 1024.0));
                }
                else
                {
                    ImGui::TextDisabled("%.1f KB", static_cast<double>(item.Size) / 1024.0);
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        ImGui::EndChild();

        if (m_OpenNewFolderPopup)
        {
            ImGui::OpenPopup("New Folder");
            m_OpenNewFolderPopup = false;
        }
        if (ImGui::BeginPopupModal("New Folder", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextUnformatted("Create a folder in the current asset directory.");
            ImGui::InputText("Name", &m_NewFolderName);
            if (ImGui::Button("Create", ImVec2(110.0f, 0.0f)))
            {
                std::string error;
                if (m_ContentBrowser.CreateFolder(m_NewFolderName, &error))
                {
                    m_ContentError.clear();
                    SetStatus("Created folder " + m_NewFolderName);
                    ImGui::CloseCurrentPopup();
                }
                else
                {
                    m_ContentError = error;
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(110.0f, 0.0f)))
            {
                m_ContentError.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        if (m_OpenRenamePopup)
        {
            ImGui::OpenPopup("Rename Asset");
            m_OpenRenamePopup = false;
        }
        if (ImGui::BeginPopupModal("Rename Asset", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextDisabled("%s", m_RenameItem.RelativePath.c_str());
            ImGui::InputText("New Name", &m_RenameName);
            if (ImGui::Button("Rename", ImVec2(110.0f, 0.0f)))
            {
                std::string error;
                if (m_ContentBrowser.Rename(m_RenameItem, m_RenameName, &error))
                {
                    m_ContentError.clear();
                    m_ContentSelection.clear();
                    SetStatus("Renamed asset to " + m_RenameName);
                    ImGui::CloseCurrentPopup();
                }
                else
                {
                    m_ContentError = error;
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(110.0f, 0.0f)))
            {
                m_ContentError.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        if (m_OpenDeletePopup)
        {
            ImGui::OpenPopup("Delete Asset");
            m_OpenDeletePopup = false;
        }
        if (ImGui::BeginPopupModal("Delete Asset", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("Delete \"%s\"?", m_DeleteItem.RelativePath.c_str());
            ImGui::TextDisabled("This cannot be undone.");
            if (ImGui::Button("Delete", ImVec2(110.0f, 0.0f)))
            {
                std::string error;
                if (m_ContentBrowser.Delete(m_DeleteItem, &error))
                {
                    m_ContentError.clear();
                    m_ContentSelection.clear();
                    SetStatus("Deleted " + m_DeleteItem.RelativePath);
                    ImGui::CloseCurrentPopup();
                }
                else
                {
                    m_ContentError = error;
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(110.0f, 0.0f)))
            {
                m_ContentError.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    void EditorApp::DrawContentTreeNode(const std::string& name, const std::string& relativePath, int depth)
    {
        if (depth > 12)
        {
            return;
        }
        std::vector<std::string> children = m_ContentBrowser.GetSubdirectories(relativePath);
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (children.empty())
        {
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        }
        if (m_ContentBrowser.GetCurrentPath() == relativePath)
        {
            flags |= ImGuiTreeNodeFlags_Selected;
        }
        ImGui::PushID(relativePath.c_str());
        const bool open = ImGui::TreeNodeEx("##Node", flags, "%s", name.c_str());
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
        {
            (void)m_ContentBrowser.NavigateTo(relativePath);
            m_ContentSelection.clear();
        }
        if (open && !children.empty())
        {
            for (const std::string& child : children)
            {
                const std::string childName = child.substr(child.find_last_of('/') + 1);
                DrawContentTreeNode(childName, child, depth + 1);
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }

    void EditorApp::DrawContentItemActions(const ContentItem& item)
    {
        ImGui::TextDisabled("%s", item.Name.c_str());
        ImGui::Separator();
        const char* openLabel = item.IsDirectory ? "Open" : (item.Type == ContentItemType::Scene ?
            "Load Scene" : "Use Asset");
        if (ImGui::MenuItem(openLabel))
        {
            ActivateContentItem(item);
        }
        if (item.Type == ContentItemType::Mesh && ImGui::MenuItem("Add to Scene"))
        {
            SpawnContentMesh(item);
        }
        if (item.Type == ContentItemType::Texture && ImGui::MenuItem("Apply to Selection"))
        {
            ApplyContentTexture(item);
        }
        if (item.Type == ContentItemType::Script && ImGui::MenuItem("Attach to Selection"))
        {
            AttachContentScript(item);
        }
        if (item.Type == ContentItemType::Audio && ImGui::MenuItem("Preview"))
        {
            ActivateContentItem(item);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Copy Path"))
        {
            ImGui::SetClipboardText(m_ContentBrowser.GetAssetPath(item).c_str());
            SetStatus("Copied " + m_ContentBrowser.GetAssetPath(item));
        }
        // The project manifest defines the project itself; renaming or deleting it from the asset
        // browser would leave the project unloadable.
        const bool isProjectManifest = item.RelativePath == "project.aster.yaml";
        if (!isProjectManifest && ImGui::MenuItem("Rename"))
        {
            ImGui::CloseCurrentPopup();
            m_RenameItem = item;
            m_RenameName = item.Name;
            m_OpenRenamePopup = true;
        }
        if (!isProjectManifest && ImGui::MenuItem("Delete"))
        {
            ImGui::CloseCurrentPopup();
            m_DeleteItem = item;
            m_OpenDeletePopup = true;
        }
    }

    void EditorApp::ActivateContentItem(const ContentItem& item)
    {
        switch (item.Type)
        {
        case ContentItemType::Folder:
            (void)m_ContentBrowser.NavigateTo(item.RelativePath);
            m_ContentSelection.clear();
            break;
        case ContentItemType::Scene:
            OpenSceneAsset(item.RelativePath);
            break;
        case ContentItemType::Mesh:
            SpawnContentMesh(item);
            break;
        case ContentItemType::Texture:
            ApplyContentTexture(item);
            break;
        case ContentItemType::Environment:
        {
            std::string error;
            const AssetRef<Environment> environment =
                m_Project.GetAssets().LoadEnvironment(m_ContentBrowser.GetAssetPath(item), &error);
            if (environment.IsValid())
            {
                m_Environment = environment;
                m_Project.GetScene().GetSettings().EnvironmentPath = m_ContentBrowser.GetAssetPath(item);
                m_SceneDirty = true;
                SetStatus("Applied environment " + item.Name);
            }
            else
            {
                SetStatus("Could not load environment: " + error);
            }
            break;
        }
        case ContentItemType::Script:
            AttachContentScript(item);
            break;
        case ContentItemType::Audio:
        {
            if (!m_AudioReady)
            {
                SetStatus("Audio is unavailable");
                break;
            }
            std::string error;
            const AssetRef<Sound> sound = m_Project.GetAssets().LoadSound(m_ContentBrowser.GetAssetPath(item),
                &error);
            if (sound.IsValid() && m_Audio.Play(*sound, m_EditorCamera.GetPosition(), 0.8f, 1.0f, false, false,
                    &error))
            {
                SetStatus("Playing " + item.Name);
                ASTER_LOG_DEBUG("ContentBrowser", "Previewing audio asset %s", item.RelativePath.c_str());
            }
            else
            {
                SetStatus("Could not play audio: " + error);
            }
            break;
        }
        case ContentItemType::Other:
            SetStatus("No editor action for " + item.Name);
            break;
        }
    }

    void EditorApp::SpawnContentMesh(const ContentItem& item)
    {
        if (!CanEditScene())
        {
            SetStatus("Stop play mode before adding assets");
            return;
        }
        const std::string name = std::filesystem::path(item.Name).stem().string();
        CreateMeshEntity(name.empty() ? "Mesh" : name, m_ContentBrowser.GetAssetPath(item), glm::vec3(1.0f));
    }

    void EditorApp::ApplyContentTexture(const ContentItem& item)
    {
        if (!CanEditScene())
        {
            SetStatus("Stop play mode before editing materials");
            return;
        }
        if (m_Selected == NullEntity || !m_Project.GetScene().IsAlive(m_Selected) ||
            !m_Project.GetScene().GetRegistry().Has<MeshRendererComponent>(m_Selected))
        {
            SetStatus("Select an entity with a mesh renderer first");
            return;
        }

        std::string error;
        const AssetRef<Texture> texture = m_Project.GetAssets().LoadTexture(
            m_ContentBrowser.GetAssetPath(item), &error);
        if (!texture.IsValid())
        {
            SetStatus("Could not load texture: " + error);
            return;
        }
        MeshRendererComponent& renderer =
            m_Project.GetScene().GetRegistry().Get<MeshRendererComponent>(m_Selected);
        if (renderer.Materials.empty())
        {
            renderer.Materials.push_back(Material{});
        }
        renderer.Materials.front().AlbedoMap = texture;
        m_SceneDirty = true;
        SetStatus("Applied " + item.Name + " to " +
            m_Project.GetScene().GetRegistry().Get<NameComponent>(m_Selected).Name);
    }

    void EditorApp::AttachContentScript(const ContentItem& item)
    {
        if (!CanEditScene())
        {
            SetStatus("Stop play mode before editing scripts");
            return;
        }
        if (m_Selected == NullEntity || !m_Project.GetScene().IsAlive(m_Selected))
        {
            SetStatus("Select an entity before attaching a script");
            return;
        }

        const std::string path = m_ContentBrowser.GetAssetPath(item);
        Registry& registry = m_Project.GetScene().GetRegistry();
        if (!registry.Has<ScriptComponent>(m_Selected))
        {
            registry.Emplace<ScriptComponent>(m_Selected, ScriptComponent{});
        }
        ScriptComponent& scripts = registry.Get<ScriptComponent>(m_Selected);
        for (const ScriptEntry& entry : scripts.Scripts)
        {
            if (entry.Path == path)
            {
                SetStatus("Script is already attached");
                return;
            }
        }
        ScriptEntry entry;
        entry.Path = path;
        entry.Enabled = true;
        scripts.Scripts.push_back(std::move(entry));
        m_SceneDirty = true;
        SetStatus("Attached " + item.Name);
        ASTER_LOG_DEBUG("ContentBrowser", "Attached script %s to entity %d", path.c_str(),
            static_cast<int>(m_Selected.Index()));
    }

    void EditorApp::OpenSceneAsset(const std::string& relativePath)
    {
        if (!m_Project.IsOpen())
        {
            return;
        }
        if (m_Playing)
        {
            StopPlay();
        }
        std::string error;
        // Scene files normally live under scenes/, but a scene dropped in assets/ is valid too.
        // Other paths are interpreted relative to the asset manager root.
        const std::string scenePath = relativePath.rfind("scenes/", 0) == 0 ||
                relativePath.rfind("assets/", 0) == 0
            ? relativePath
            : "assets/" + relativePath;
        if (!m_Project.LoadScene(scenePath, &error))
        {
            SetStatus("Could not load scene: " + error);
            return;
        }
        m_Selected = m_Project.GetScene().GetPrimaryCamera();
        m_SceneDirty = false;
        m_ActiveGizmoAxis = -1;
        m_OpenSaveAsPopup = false;
        LoadSceneEnvironment();
        m_SaveAsPath = m_Project.GetCurrentScenePath();
        SyncCameraFromScene();
        SetStatus("Loaded scene " + scenePath);
        ASTER_LOG_INFO("Editor", "Loaded scene %s from the content browser", scenePath.c_str());
    }

    void EditorApp::UpdateCamera(float deltaTime, bool viewportHovered)
    {
        ImGuiIO& io = ImGui::GetIO();
        if (io.WantTextInput || !m_Project.IsOpen())
        {
            return;
        }

        if (viewportHovered)
        {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
            {
                m_EditorCamera.SetRotation(m_EditorCamera.GetYaw() + io.MouseDelta.x * 0.15f,
                    m_EditorCamera.GetPitch() - io.MouseDelta.y * 0.15f);
                m_ViewportHovered = true;
            }
            if (std::abs(io.MouseWheel) > 0.0f)
            {
                m_MoveSpeed = std::clamp(m_MoveSpeed + io.MouseWheel * 0.8f, 1.0f, 40.0f);
            }
        }

        const bool moving = viewportHovered || ImGui::IsMouseDown(ImGuiMouseButton_Right);
        if (!moving)
        {
            return;
        }

        const float speed = m_MoveSpeed * ((io.KeyShift ? 3.0f : 1.0f)) * deltaTime;
        glm::vec3 position = m_EditorCamera.GetPosition();
        const glm::vec3 forward = m_EditorCamera.GetForward();
        const glm::vec3 right = m_EditorCamera.GetRight();
        if (ImGui::IsKeyDown(ImGuiKey_W))
        {
            position += forward * speed;
        }
        if (ImGui::IsKeyDown(ImGuiKey_S))
        {
            position -= forward * speed;
        }
        if (ImGui::IsKeyDown(ImGuiKey_D))
        {
            position += right * speed;
        }
        if (ImGui::IsKeyDown(ImGuiKey_A))
        {
            position -= right * speed;
        }
        if (ImGui::IsKeyDown(ImGuiKey_E))
        {
            position.y += speed;
        }
        if (ImGui::IsKeyDown(ImGuiKey_Q))
        {
            position.y -= speed;
        }
        m_EditorCamera.SetPosition(position);
    }

    void EditorApp::SelectEntity(Entity entity)
    {
        if (!m_Project.IsOpen() || !m_Project.GetScene().IsAlive(entity))
        {
            m_Selected = NullEntity;
            return;
        }
        m_Selected = entity;
        m_SaveAsPath = m_Project.GetCurrentScenePath();
    }

    void EditorApp::PickEntity(const glm::vec2& viewportMouse, const glm::vec2& viewportSize)
    {
        if (!m_Project.IsOpen())
        {
            return;
        }
        const Ray ray = m_EditorCamera.ScreenToWorldRay(viewportMouse, viewportSize);
        Scene& scene = m_Project.GetScene();
        Entity hit = NullEntity;
        float nearest = std::numeric_limits<float>::max();
        for (Entity entity : scene.GetEntities())
        {
            const Registry& registry = scene.GetRegistry();
            if (!registry.Has<MeshRendererComponent>(entity))
            {
                continue;
            }
            const MeshRendererComponent& renderer = registry.Get<MeshRendererComponent>(entity);
            if (!renderer.Visible || !renderer.Mesh.IsValid() || !renderer.Mesh->Geometry)
            {
                continue;
            }
            const Aabb bounds = renderer.Mesh->Geometry->Bounds;
            if (!bounds.IsValid())
            {
                continue;
            }
            const Aabb worldBounds = bounds.Transformed(scene.GetWorldMatrix(entity));
            if (!std::isfinite(worldBounds.Min.x) || !std::isfinite(worldBounds.Min.y) ||
                !std::isfinite(worldBounds.Min.z) || !std::isfinite(worldBounds.Max.x) ||
                !std::isfinite(worldBounds.Max.y) || !std::isfinite(worldBounds.Max.z))
            {
                continue;
            }
            if (const std::optional<float> distance = IntersectRayAabb(ray, worldBounds, 100000.0f))
            {
                if (std::isfinite(*distance) && *distance < nearest)
                {
                    nearest = *distance;
                    hit = entity;
                }
            }
        }
        SelectEntity(hit);
    }

    void EditorApp::FocusSelected()
    {
        if (m_Selected == NullEntity || !m_Project.IsOpen())
        {
            return;
        }
        const glm::vec3 target = m_Project.GetScene().GetWorldTransform(m_Selected).Translation;
        if (!std::isfinite(target.x) || !std::isfinite(target.y) || !std::isfinite(target.z))
        {
            SetStatus("Selected entity has an invalid transform");
            return;
        }
        m_EditorCamera.SetPosition(target - m_EditorCamera.GetForward() * 6.0f);
        SetStatus("Focused " + m_Project.GetScene().GetRegistry().Get<NameComponent>(m_Selected).Name);
    }

    void EditorApp::CreateEmpty()
    {
        if (!m_Project.IsOpen() || m_Playing)
        {
            return;
        }
        Scene& scene = m_Project.GetScene();
        const Entity entity = scene.CreateEntity(UniqueName("Entity"));
        const glm::vec3 spawn = m_EditorCamera.GetPosition() + m_EditorCamera.GetForward() * 6.0f;
        scene.GetRegistry().Get<TransformComponent>(entity).Local.Translation = spawn;
        SelectEntity(entity);
        m_SceneDirty = true;
    }

    void EditorApp::CreateMeshEntity(const std::string& name, const std::string& meshPath, const glm::vec3& scale)
    {
        if (!m_Project.IsOpen() || m_Playing)
        {
            return;
        }
        std::string error;
        const AssetRef<MeshAsset> mesh = m_Project.GetAssets().LoadMesh(meshPath, &error);
        if (!mesh.IsValid())
        {
            SetStatus("Could not load mesh: " + error);
            return;
        }

        Scene& scene = m_Project.GetScene();
        const Entity entity = scene.CreateEntity(UniqueName(name));
        const glm::vec3 spawn = m_EditorCamera.GetPosition() + m_EditorCamera.GetForward() * 6.0f;
        TransformComponent& transform = scene.GetRegistry().Get<TransformComponent>(entity);
        // Place the authored mesh's lowest point on the ground plane. Primitive meshes are unit
        // sized (cube/sphere are [-0.5, 0.5]), so this keeps cubes, spheres and imported meshes
        // sitting on the floor instead of half-buried or floating.
        float bottom = 0.0f;
        if (mesh->Geometry && mesh->Geometry->Bounds.IsValid())
        {
            bottom = mesh->Geometry->Bounds.Min.y;
        }
        transform.Local.Translation = glm::vec3(spawn.x, -bottom * scale.y, spawn.z);
        transform.Local.Scale = scale;

        Material material;
        material.Name = name + " Material";
        if (name == "Cube")
        {
            material.BaseColor = glm::vec4(0.9f, 0.5f, 0.22f, 1.0f);
            material.Roughness = 0.4f;
        }
        else if (name == "Sphere")
        {
            material.BaseColor = glm::vec4(0.6f, 0.7f, 0.85f, 1.0f);
            material.Metallic = 0.7f;
            material.Roughness = 0.25f;
        }
        else
        {
            material.BaseColor = glm::vec4(0.34f, 0.36f, 0.4f, 1.0f);
            material.Roughness = 0.85f;
        }

        MeshRendererComponent renderer;
        renderer.Mesh = mesh;
        renderer.Materials.push_back(material);
        scene.GetRegistry().Emplace<MeshRendererComponent>(entity, std::move(renderer));
        SelectEntity(entity);
        m_SceneDirty = true;
    }

    void EditorApp::CreateCameraEntity()
    {
        if (!m_Project.IsOpen() || m_Playing)
        {
            return;
        }
        Scene& scene = m_Project.GetScene();
        const Entity entity = scene.CreateEntity(UniqueName("Camera"));
        TransformComponent& transform = scene.GetRegistry().Get<TransformComponent>(entity);
        transform.Local.Translation = m_EditorCamera.GetPosition();
        transform.Local.Rotation = RotationFromDirection(m_EditorCamera.GetForward());
        CameraComponent camera;
        camera.Primary = false;
        camera.FieldOfView = 55.0f;
        scene.GetRegistry().Emplace<CameraComponent>(entity, camera);
        SelectEntity(entity);
        m_SceneDirty = true;
    }

    void EditorApp::CreateLightEntity(bool directional)
    {
        if (!m_Project.IsOpen() || m_Playing)
        {
            return;
        }
        Scene& scene = m_Project.GetScene();
        if (directional)
        {
            const Entity entity = scene.CreateEntity(UniqueName("Directional Light"));
            scene.GetRegistry().Get<TransformComponent>(entity).Local.Rotation = glm::vec3(-50.0f, -30.0f, 0.0f);
            scene.GetRegistry().Emplace<DirectionalLightComponent>(entity, DirectionalLightComponent{});
            SelectEntity(entity);
        }
        else
        {
            const Entity entity = scene.CreateEntity(UniqueName("Point Light"));
            const glm::vec3 spawn = m_EditorCamera.GetPosition() + m_EditorCamera.GetForward() * 5.0f;
            scene.GetRegistry().Get<TransformComponent>(entity).Local.Translation =
                glm::vec3(spawn.x, 2.5f, spawn.z);
            PointLightComponent light;
            light.Intensity = 12.0f;
            light.Range = 12.0f;
            scene.GetRegistry().Emplace<PointLightComponent>(entity, light);
            SelectEntity(entity);
        }
        m_SceneDirty = true;
    }

    void EditorApp::DeleteSelected()
    {
        if (!m_Project.IsOpen() || m_Selected == NullEntity || m_Playing)
        {
            return;
        }
        m_Project.GetScene().DestroyEntity(m_Selected);
        m_Selected = NullEntity;
        m_SceneDirty = true;
    }

    void EditorApp::AddComponent(const char* type)
    {
        if (!m_Project.IsOpen() || m_Selected == NullEntity || m_Playing)
        {
            return;
        }
        Scene& scene = m_Project.GetScene();
        Registry& registry = scene.GetRegistry();
        if (std::strcmp(type, "Camera") == 0 && !registry.Has<CameraComponent>(m_Selected))
        {
            CameraComponent camera;
            camera.Primary = false;
            registry.Emplace<CameraComponent>(m_Selected, camera);
        }
        else if (std::strcmp(type, "MeshRenderer") == 0 && !registry.Has<MeshRendererComponent>(m_Selected))
        {
            std::string error;
            const AssetRef<MeshAsset> mesh = m_Project.GetAssets().LoadMesh("assets/models/cube.obj", &error);
            if (!mesh.IsValid())
            {
                SetStatus("Could not load default cube: " + error);
                return;
            }
            MeshRendererComponent renderer;
            renderer.Mesh = mesh;
            Material material;
            material.Name = "Material";
            renderer.Materials.push_back(material);
            registry.Emplace<MeshRendererComponent>(m_Selected, std::move(renderer));
        }
        else if (std::strcmp(type, "DirectionalLight") == 0 &&
            !registry.Has<DirectionalLightComponent>(m_Selected))
        {
            registry.Emplace<DirectionalLightComponent>(m_Selected, DirectionalLightComponent{});
        }
        else if (std::strcmp(type, "PointLight") == 0 && !registry.Has<PointLightComponent>(m_Selected))
        {
            registry.Emplace<PointLightComponent>(m_Selected, PointLightComponent{});
        }
        else if (std::strcmp(type, "SpotLight") == 0 && !registry.Has<SpotLightComponent>(m_Selected))
        {
            registry.Emplace<SpotLightComponent>(m_Selected, SpotLightComponent{});
        }
        else if (std::strcmp(type, "ParticleEmitter") == 0 &&
            !registry.Has<ParticleEmitterComponent>(m_Selected))
        {
            registry.Emplace<ParticleEmitterComponent>(m_Selected, ParticleEmitterComponent{});
        }
        else if (std::strcmp(type, "RigidBody") == 0 && !registry.Has<RigidBodyComponent>(m_Selected))
        {
            registry.Emplace<RigidBodyComponent>(m_Selected, RigidBodyComponent{});
        }
        else if (std::strcmp(type, "BoxCollider") == 0 && !registry.Has<BoxColliderComponent>(m_Selected))
        {
            registry.Emplace<BoxColliderComponent>(m_Selected, BoxColliderComponent{});
        }
        else if (std::strcmp(type, "SphereCollider") == 0 && !registry.Has<SphereColliderComponent>(m_Selected))
        {
            registry.Emplace<SphereColliderComponent>(m_Selected, SphereColliderComponent{});
        }
        else if (std::strcmp(type, "Scripts") == 0 && !registry.Has<ScriptComponent>(m_Selected))
        {
            ScriptComponent scripts;
            scripts.Scripts.push_back(ScriptEntry{ "assets/scripts/spin.lua", true });
            registry.Emplace<ScriptComponent>(m_Selected, std::move(scripts));
        }
        else if (std::strcmp(type, "AudioListener") == 0 &&
            !registry.Has<AudioListenerComponent>(m_Selected))
        {
            registry.Emplace<AudioListenerComponent>(m_Selected, AudioListenerComponent{});
        }
        m_SceneDirty = true;
    }

    void EditorApp::TogglePlayMode()
    {
        if (m_Playing)
        {
            StopPlay();
        }
        else
        {
            StartPlay();
        }
    }

    void EditorApp::StartPlay()
    {
        if (!m_Project.IsOpen() || m_Playing)
        {
            return;
        }

        Scene& scene = m_Project.GetScene();
        m_PlaySnapshot = SceneSerializer::Serialize(scene);
        if (m_PlaySnapshot.empty())
        {
            SetStatus("Could not snapshot the scene for play mode");
            return;
        }

        std::string error;
        if (!m_Scripts.Initialize(scene, &error))
        {
            m_PlaySnapshot.clear();
            SetStatus("Play mode failed: " + error);
            ASTER_LOG_ERROR("Editor", "Play mode failed: %s", error.c_str());
            return;
        }
        m_Scripts.SetAssetManager(&m_Project.GetAssets());
        m_Scripts.SetPhysics(&m_Physics);
        m_Scripts.SetKeyDownCallback([this](std::string_view key)
        {
            return m_Window.IsKeyDown(Window::KeyFromName(key));
        });

        m_Playing = true;
        m_GameView = true;
        m_FocusViewportRequested = true;
        m_PlayInputLogged = false;
        m_Physics.Rebuild(scene);
        (void)m_MarbleGame.Initialize(scene, m_Physics);

        if (m_AudioReady)
        {
            m_Audio.SetListenerPosition(m_EditorCamera.GetPosition());
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
                std::string audioError;
                if (!m_Audio.Play(*audio.Clip, glm::vec3(scene.GetWorldMatrix(entity)[3]), audio.Volume,
                        audio.Pitch, audio.Loop, audio.Spatial, &audioError))
                {
                    ASTER_LOG_WARN("Editor", "Could not play '%s': %s", audio.Clip.Path.c_str(),
                        audioError.c_str());
                }
            }
        }

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
                const std::filesystem::path path = m_Project.Resolve(entry.Path);
                if (!m_Scripts.LoadScript(path.string(), handle, &error))
                {
                    ASTER_LOG_ERROR("Editor", "Could not load script '%s': %s", entry.Path.c_str(),
                        error.c_str());
                    continue;
                }
                if (!m_Scripts.Start(handle, entity, &error))
                {
                    ASTER_LOG_ERROR("Editor", "Script '%s' failed OnStart: %s", entry.Path.c_str(),
                        error.c_str());
                    m_Scripts.UnloadScript(handle);
                    continue;
                }
                m_RunningScripts.push_back(RunningScript{ entity, handle });
            }
        }

        SetStatus("Playing: " + std::to_string(m_RunningScripts.size()) + " script(s), " +
            std::to_string(m_Physics.GetBodyCount()) + " physics body/bodies");
        ASTER_LOG_INFO("Editor", "Play mode started (%zu scripts, %zu bodies)", m_RunningScripts.size(),
            m_Physics.GetBodyCount());
        ASTER_LOG_DEBUG("Editor", "Play state: gameView=%d focusRequested=%d marbleActive=%d", m_GameView ? 1 : 0,
            m_FocusViewportRequested ? 1 : 0, m_MarbleGame.IsActive() ? 1 : 0);    }

    void EditorApp::StopPlay()
    {
        if (!m_Playing)
        {
            m_Scripts.Shutdown();
            return;
        }

        Scene& scene = m_Project.GetScene();
        std::string error;
        for (RunningScript& running : m_RunningScripts)
        {
            if (scene.IsAlive(running.Owner))
            {
                (void)m_Scripts.Destroy(running.Handle, running.Owner, &error);
            }
            m_Scripts.UnloadScript(running.Handle);
        }
        m_RunningScripts.clear();
        m_Scripts.Shutdown();
        m_Audio.StopAll();
        m_Physics.Clear();

        if (!m_PlaySnapshot.empty() && m_Project.IsOpen())
        {
            if (!SceneSerializer::LoadFromString(scene, m_PlaySnapshot, &m_Project.GetAssets(), &error))
            {
                ASTER_LOG_ERROR("Editor", "Could not restore the scene after play mode: %s", error.c_str());
            }
            m_Selected = scene.FindByName("Cube");
        }
        m_PlaySnapshot.clear();
        m_Playing = false;
        m_GameView = false;
        m_FocusViewportRequested = false;
        m_ViewportFocused = false;
        SetStatus("Stopped play mode");
        ASTER_LOG_INFO("Editor", "Play mode stopped");
        ASTER_LOG_DEBUG("Editor", "Play state cleared: gameView=%d focusRequested=%d", m_GameView ? 1 : 0,
            m_FocusViewportRequested ? 1 : 0);
    }

    void EditorApp::UpdatePlayMode(float deltaTime)
    {
        if (!m_Playing || !m_Project.IsOpen())
        {
            return;
        }

        Scene& scene = m_Project.GetScene();
        m_Physics.Step(deltaTime);
        m_Physics.SyncToScene(scene);

        if (m_MarbleGame.IsActive())
        {
            MarbleInput input;
            const ImGuiIO& io = ImGui::GetIO();
            if (m_GameView && !io.WantTextInput)
            {
                input.Forward = (m_Window.IsKeyDown(GLFW_KEY_W) ? 1.0f : 0.0f) -
                    (m_Window.IsKeyDown(GLFW_KEY_S) ? 1.0f : 0.0f);
                input.Right = (m_Window.IsKeyDown(GLFW_KEY_D) ? 1.0f : 0.0f) -
                    (m_Window.IsKeyDown(GLFW_KEY_A) ? 1.0f : 0.0f);
                input.Jump = m_Window.IsKeyDown(GLFW_KEY_SPACE);
                input.Restart = m_Window.IsKeyDown(GLFW_KEY_R);
                if (!m_PlayInputLogged && (input.Forward != 0.0f || input.Right != 0.0f || input.Jump ||
                        input.Restart))
                {
                    const glm::vec3 player = m_MarbleGame.GetPlayerPosition();
                    ASTER_LOG_DEBUG("Editor", "Play input active=%d W=%d A=%d S=%d D=%d focus=%d pos=(%.2f, %.2f, %.2f)",
                        m_MarbleGame.IsActive() ? 1 : 0, m_Window.IsKeyDown(GLFW_KEY_W) ? 1 : 0,
                        m_Window.IsKeyDown(GLFW_KEY_A) ? 1 : 0, m_Window.IsKeyDown(GLFW_KEY_S) ? 1 : 0,
                        m_Window.IsKeyDown(GLFW_KEY_D) ? 1 : 0, m_ViewportFocused ? 1 : 0,
                        player.x, player.y, player.z);
                    m_PlayInputLogged = true;
                }
            }
            m_MarbleGame.Update(deltaTime, input);
        }

        std::string error;
        for (const RunningScript& running : m_RunningScripts)
        {
            if (!scene.IsAlive(running.Owner))
            {
                continue;
            }
            (void)m_Scripts.Update(running.Handle, running.Owner, deltaTime, &error);
        }

        if (m_AudioReady)
        {
            m_Audio.SetListenerPosition(m_EditorCamera.GetPosition());
            m_Audio.Update();
        }
    }

    void EditorApp::SyncCameraFromScene()
    {
        if (!m_Project.IsOpen())
        {
            return;
        }
        Scene& scene = m_Project.GetScene();
        const Entity cameraEntity = scene.GetPrimaryCamera();
        if (cameraEntity == NullEntity)
        {
            return;
        }
        const glm::mat4 world = scene.GetWorldMatrix(cameraEntity);
        const glm::vec3 position = glm::vec3(world[3]);
        const glm::vec3 forward = SafeNormalize(-glm::vec3(world[2]), glm::vec3(0.0f, 0.0f, -1.0f));
        m_EditorCamera.SetPosition(position);
        m_EditorCamera.SetRotation(glm::degrees(std::atan2(forward.z, forward.x)),
            glm::degrees(std::asin(std::clamp(forward.y, -1.0f, 1.0f))));
        if (scene.GetRegistry().Has<CameraComponent>(cameraEntity))
        {
            const CameraComponent& camera = scene.GetRegistry().Get<CameraComponent>(cameraEntity);
            const float aspect = m_EditorCamera.GetAspectRatio();
            if (camera.Perspective)
            {
                m_EditorCamera.SetPerspective(camera.FieldOfView, aspect, camera.NearPlane, camera.FarPlane);
            }
            else
            {
                m_EditorCamera.SetOrthographic(camera.OrthographicSize, aspect, camera.NearPlane,
                    camera.FarPlane);
            }
        }
    }

    void EditorApp::OnProjectOpened()
    {
        m_ShowProjectHub = false;
        m_ContentBrowser.SetRoot(m_Project.GetInfo().RootPath);
        m_ContentFilter.clear();
        m_ContentSelection.clear();
        m_ContentError = m_ContentBrowser.IsValid() ? std::string() : m_ContentBrowser.GetLastError();
        LoadSceneEnvironment();
        m_Selected = m_Project.GetScene().FindByName("Cube");
        if (m_Selected == NullEntity)
        {
            m_Selected = m_Project.GetScene().GetPrimaryCamera();
        }
        m_SaveAsPath = m_Project.GetCurrentScenePath();
        m_SceneDirty = false;
        m_ActiveGizmoAxis = -1;
        SyncCameraFromScene();
        SetStatus("Opened project: " + m_Project.GetInfo().Name);
        ASTER_LOG_INFO("Editor", "Opened project %s", m_Project.GetInfo().Name.c_str());
    }

    void EditorApp::LoadSceneEnvironment()
    {
        m_Environment.Reset();
        if (!m_Project.IsOpen())
        {
            return;
        }
        const SceneSettings& settings = m_Project.GetScene().GetSettings();
        if (settings.EnvironmentPath.empty())
        {
            return;
        }
        std::string error;
        m_Environment = m_Project.GetAssets().LoadEnvironment(settings.EnvironmentPath, &error);
        if (!m_Environment.IsValid())
        {
            ASTER_LOG_WARN("Editor", "Could not load environment '%s': %s", settings.EnvironmentPath.c_str(),
                error.c_str());
        }
    }

    void EditorApp::SetStatus(std::string message)
    {
        m_StatusMessage = std::move(message);
        m_StatusTimer = 5.0f;
    }

    std::string EditorApp::UniqueName(const std::string& base) const
    {
        if (!m_Project.IsOpen())
        {
            return base;
        }
        if (m_Project.GetScene().FindByName(base) == NullEntity)
        {
            return base;
        }
        for (int i = 1; i < 10000; ++i)
        {
            const std::string candidate = base + " " + std::to_string(i);
            if (m_Project.GetScene().FindByName(candidate) == NullEntity)
            {
                return candidate;
            }
        }
        return base;
    }

    Entity EditorApp::SelectedParent() const
    {
        if (!m_Project.IsOpen() || m_Selected == NullEntity)
        {
            return NullEntity;
        }
        return m_Project.GetScene().GetParent(m_Selected);
    }
}
