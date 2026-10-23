#pragma once

#include "Aster/Assets/AssetManager.h"
#include "Aster/Scene/Scene.h"

#include <filesystem>
#include <string>

namespace Aster
{
    struct ProjectInfo
    {
        std::string Name = "Untitled Project";
        std::filesystem::path RootPath;
        std::string DefaultScene = "scenes/main.aster.yaml";
    };

    class Project
    {
    public:
        Project();
        ~Project() = default;

        Project(const Project&) = delete;
        Project& operator=(const Project&) = delete;

        [[nodiscard]] bool Create(const std::filesystem::path& root, const std::string& name,
            std::string* error = nullptr);
        // Creates a complete project plus the Marble Run gameplay sample scene.
        [[nodiscard]] bool CreateGameDemo(const std::filesystem::path& root, const std::string& name,
            std::string* error = nullptr);
        [[nodiscard]] bool Open(const std::filesystem::path& root, std::string* error = nullptr);
        void Close();
        [[nodiscard]] bool Save(std::string* error = nullptr);
        [[nodiscard]] bool SaveScene(std::string* error = nullptr);
        [[nodiscard]] bool SaveSceneAs(const std::string& relativePath, std::string* error = nullptr);
        [[nodiscard]] bool LoadScene(const std::string& relativePath, std::string* error = nullptr);
        [[nodiscard]] bool NewScene(std::string* error = nullptr);

        [[nodiscard]] bool IsOpen() const { return m_Open; }
        [[nodiscard]] Scene& GetScene() { return m_Scene; }
        [[nodiscard]] const Scene& GetScene() const { return m_Scene; }
        [[nodiscard]] AssetManager& GetAssets() { return m_Assets; }
        [[nodiscard]] const AssetManager& GetAssets() const { return m_Assets; }
        [[nodiscard]] const ProjectInfo& GetInfo() const { return m_Info; }
        [[nodiscard]] const std::string& GetCurrentScenePath() const { return m_CurrentScenePath; }
        [[nodiscard]] std::filesystem::path Resolve(const std::string& relativePath) const;
        [[nodiscard]] std::string MakeRelative(const std::filesystem::path& path) const;

    private:
        [[nodiscard]] bool CreateDefaultScene(std::string* error);
        [[nodiscard]] bool CreateMarbleDemoScene(std::string* error);
        [[nodiscard]] bool WriteProjectFile(std::string* error);
        [[nodiscard]] bool ReadProjectFile(const std::filesystem::path& file, std::string* error);

        ProjectInfo m_Info;
        AssetManager m_Assets;
        Scene m_Scene;
        std::string m_CurrentScenePath;
        bool m_Open = false;
    };
}
