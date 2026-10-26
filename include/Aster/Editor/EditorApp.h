#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "Aster/Project/Project.h"
#include "Aster/Core/Log.h"
#include "Aster/Editor/ContentBrowser.h"
#include "Aster/Audio/AudioEngine.h"
#include "Aster/Physics/PhysicsWorld.h"
#include "Aster/Runtime/MarbleGame.h"
#include "Aster/Platform/Window.h"
#include "Aster/Rendering/Camera.h"
#include "Aster/Rendering/D3D12Context.h"
#include "Aster/Rendering/D3D12Renderer.h"
#include "Aster/Rendering/ImGuiRenderer.h"
#include "Aster/Scripting/ScriptEngine.h"

#include <glm/glm.hpp>

#include <deque>
#include <filesystem>
#include <string>
#include <vector>

namespace Aster
{
    enum class GizmoMode
    {
        Translate,
        Rotate,
        Scale
    };

    class EditorApp
    {
    public:
        EditorApp();
        ~EditorApp();

        EditorApp(const EditorApp&) = delete;
        EditorApp& operator=(const EditorApp&) = delete;

        int Run(int argc, char** argv);

    private:
        struct Arguments
        {
            std::string ProjectPath;
            std::string ProjectName = "Aster Project";
            bool CreateProject = false;
            bool CreateDemo = false;
            bool AutoPlay = false;
            std::filesystem::path ExportDirectory;
            std::filesystem::path PlayerPath;
            std::string ScriptPath;
            int FrameLimit = 0;
            std::string ScreenshotPath;
        };

        [[nodiscard]] static Arguments ParseArguments(int argc, char** argv);
        [[nodiscard]] bool Initialize(std::string* error);
        void Shutdown();
        void Frame(float deltaTime);
        void DrawUI();
        void DrawMenuBar();
        void DrawToolbar();
        void DrawStatusBar();
        void DrawProjectHub();
        void DrawHierarchy();
        void DrawInspector();
        void DrawViewport(float deltaTime);
        void DrawViewportOverlay();
        void DrawConsole();
        void DrawContentBrowser();
        void DrawContentTreeNode(const std::string& name, const std::string& relativePath, int depth);
        void DrawContentItemActions(const ContentItem& item);
        void ActivateContentItem(const ContentItem& item);
        void SpawnContentMesh(const ContentItem& item);
        void ApplyContentTexture(const ContentItem& item);
        void AttachContentScript(const ContentItem& item);
        void OpenSceneAsset(const std::string& relativePath);
        void DrawGizmo();
        void DrawEntityNode(Entity entity);

        void UpdateCamera(float deltaTime, bool viewportHovered);
        void SelectEntity(Entity entity);
        void PickEntity(const glm::vec2& viewportMouse, const glm::vec2& viewportSize);
        void FocusSelected();

        void CreateEmpty();
        void CreateMeshEntity(const std::string& name, const std::string& meshPath, const glm::vec3& scale);
        void CreateCameraEntity();
        void CreateLightEntity(bool directional);
        void DeleteSelected();
        void AddComponent(const char* type);
        void SyncCameraFromScene();
        void OnProjectOpened();
        void LoadSceneEnvironment();
        void SetStatus(std::string message);

        void TogglePlayMode();
        void StartPlay();
        void StopPlay();
        void UpdatePlayMode(float deltaTime);
        [[nodiscard]] bool ExportGame(const std::filesystem::path& outputDirectory, std::string* error);
        [[nodiscard]] bool RunSceneScript(const std::string& path, std::string* error);
        void DrawExportPopup();

        [[nodiscard]] std::string UniqueName(const std::string& base) const;
        [[nodiscard]] Entity SelectedParent() const;
        [[nodiscard]] bool CanEditScene() const { return m_Project.IsOpen() && !m_Playing; }

        struct RunningScript
        {
            Entity Owner = NullEntity;
            ScriptHandle Handle;
        };

        Window m_Window;
        D3D12Context m_Context;
        D3D12Renderer m_Renderer;
        ImGuiRenderer m_UI;
        Project m_Project;
        Camera m_EditorCamera;
        PhysicsWorld m_Physics;
        MarbleGame m_MarbleGame;
        ScriptEngine m_Scripts;
        AudioEngine m_Audio;
        ContentBrowser m_ContentBrowser;
        AssetRef<Environment> m_Environment;

        Arguments m_Arguments;
        Entity m_Selected = NullEntity;
        GizmoMode m_GizmoMode = GizmoMode::Translate;
        bool m_Initialized = false;
        bool m_SceneDirty = false;
        bool m_Playing = false;
        bool m_AudioReady = false;
        bool m_ShowProjectHub = true;
        bool m_ShowConsole = true;
        bool m_ShowHierarchy = true;
        bool m_ShowInspector = true;
        bool m_ShowColliders = true;
        bool m_GameView = false;

        float m_ViewportX = 0.0f;
        float m_ViewportY = 0.0f;
        float m_ViewportWidth = 0.0f;
        float m_ViewportHeight = 0.0f;
        float m_ToolbarHeight = 46.0f;
        float m_StatusBarHeight = 26.0f;
        float m_LeftPanelWidth = 280.0f;
        float m_RightPanelWidth = 340.0f;
        float m_ConsolePanelHeight = 236.0f;
        float m_PanelGap = 6.0f;
        bool m_ViewportHovered = false;
        bool m_ViewportFocused = false;
        bool m_FocusViewportRequested = false;
        bool m_PlayInputLogged = false;
        bool m_GizmoHovered = false;
        float m_MoveSpeed = 7.0f;
        bool m_OpenSaveAsPopup = false;
        float m_FrameDelta = 0.0f;

        int m_ActiveGizmoAxis = -1;
        glm::vec3 m_GizmoStartTranslation{ 0.0f };
        glm::vec3 m_GizmoStartScale{ 1.0f };
        glm::vec3 m_GizmoStartRotation{ 0.0f };
        glm::vec2 m_GizmoStartMouse{ 0.0f };

        std::deque<LogMessage> m_ConsoleMessages;
        std::vector<RunningScript> m_RunningScripts;
        std::string m_PlaySnapshot;
        Log::SinkId m_LogSink = 0;
        LogLevel m_ConsoleLevel = ASTER_ENABLE_DEBUG_LOGS ? LogLevel::Debug : LogLevel::Info;
        bool m_ConsoleAutoScroll = true;

        std::string m_ContentSelection;
        std::string m_ContentFilter;
        ContentItem m_ContextItem;
        std::string m_ContentError;
        std::string m_NewFolderName = "New Folder";
        std::string m_RenameName;
        ContentItem m_RenameItem;
        ContentItem m_DeleteItem;
        bool m_OpenNewFolderPopup = false;
        bool m_OpenRenamePopup = false;
        bool m_OpenDeletePopup = false;

        std::string m_WindowTitle;
        std::string m_ExportPath;
        std::string m_ExportError;
        bool m_OpenExportPopup = false;
        int m_ExitCode = 0;
        std::string m_StatusMessage;
        float m_StatusTimer = 0.0f;
        double m_LastTime = 0.0;

        std::string m_HubProjectPath;
        std::string m_HubProjectName = "MyGame";
        std::string m_HubOpenPath;
        std::string m_HubError;
        std::string m_SaveAsPath = "scenes/main.aster.yaml";
    };
}
