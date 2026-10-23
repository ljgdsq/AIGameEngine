#pragma once

#include "Aster/Assets/Asset.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace Aster
{
    class AssetManager
    {
    public:
        explicit AssetManager(std::filesystem::path root = {});

        void SetRootPath(const std::filesystem::path& root);
        [[nodiscard]] const std::filesystem::path& GetRootPath() const { return m_RootPath; }

        [[nodiscard]] std::filesystem::path ResolvePath(std::string_view assetPath) const;

        [[nodiscard]] AssetRef<ModelAsset> LoadModel(std::string_view path, std::string* error = nullptr);
        [[nodiscard]] AssetRef<MeshAsset> LoadMesh(std::string_view path, std::string* error = nullptr);
        [[nodiscard]] AssetRef<Texture> LoadTexture(std::string_view path, std::string* error = nullptr);
        [[nodiscard]] AssetRef<Environment> LoadEnvironment(std::string_view path, std::string* error = nullptr);
        [[nodiscard]] AssetRef<Sound> LoadSound(std::string_view path, std::string* error = nullptr);
        [[nodiscard]] std::optional<std::string> LoadScript(std::string_view path, std::string* error = nullptr);

        void Clear();
        [[nodiscard]] size_t GetModelCount() const { return m_Models.size(); }
        [[nodiscard]] size_t GetMeshCount() const { return m_Meshes.size(); }
        [[nodiscard]] size_t GetTextureCount() const { return m_Textures.size(); }

    private:
        [[nodiscard]] std::string NormalizeKey(std::string_view path) const;
        [[nodiscard]] std::filesystem::path ResolveKey(const std::string& key) const;
        [[nodiscard]] AssetRef<Texture> LoadTextureFile(const std::string& key, std::string* error);

        std::filesystem::path m_RootPath;
        std::unordered_map<std::string, std::shared_ptr<ModelAsset>> m_Models;
        std::unordered_map<std::string, std::shared_ptr<MeshAsset>> m_Meshes;
        std::unordered_map<std::string, std::shared_ptr<Texture>> m_Textures;
        std::unordered_map<std::string, std::shared_ptr<Environment>> m_Environments;
        std::unordered_map<std::string, std::shared_ptr<Sound>> m_Sounds;
    };
}
