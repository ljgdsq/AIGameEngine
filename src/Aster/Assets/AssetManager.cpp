#include "Aster/Assets/AssetManager.h"

#include "Aster/Assets/GltfLoader.h"
#include "Aster/Assets/ImageLoader.h"
#include "Aster/Assets/ObjMeshLoader.h"
#include "Aster/Core/FileSystem.h"
#include "Aster/Core/Log.h"
#include "Aster/Rendering/Ibl.h"

#include <algorithm>
#include <cctype>

namespace Aster
{
    namespace
    {
        void SetError(std::string* error, std::string message)
        {
            if (error != nullptr)
            {
                *error = std::move(message);
            }
        }

        bool IsModelExtension(const std::string& extension)
        {
            return extension == "gltf" || extension == "glb";
        }

        bool IsTextureExtension(const std::string& extension)
        {
            return extension == "png" || extension == "jpg" || extension == "jpeg" || extension == "tga" ||
                   extension == "bmp" || extension == "hdr";
        }

        bool IsNonColorTextureName(const std::string& name)
        {
            std::string lower = name;
            std::transform(lower.begin(), lower.end(), lower.begin(),
                [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
            return lower.find("normal") != std::string::npos ||
                   lower.find("roughness") != std::string::npos ||
                   lower.find("metallic") != std::string::npos ||
                   lower.find("_mr") != std::string::npos ||
                   lower.find("occlusion") != std::string::npos ||
                   lower.find("_ao") != std::string::npos;
        }
    }

    AssetManager::AssetManager(std::filesystem::path root)
        : m_RootPath(std::move(root))
    {
    }

    void AssetManager::SetRootPath(const std::filesystem::path& root)
    {
        m_RootPath = root;
    }

    std::filesystem::path AssetManager::ResolvePath(std::string_view assetPath) const
    {
        return ResolveKey(NormalizeKey(assetPath));
    }

    std::string AssetManager::NormalizeKey(std::string_view path) const
    {
        std::filesystem::path result(path);
        result = result.lexically_normal();
        std::string normalized = FileSystem::Normalize(result);
        while (normalized.size() > 2 && normalized[0] == '.' && normalized[1] == '/')
        {
            normalized.erase(0, 2);
        }
        return normalized;
    }

    std::filesystem::path AssetManager::ResolveKey(const std::string& key) const
    {
        return FileSystem::Resolve(m_RootPath, std::filesystem::path(key));
    }

    AssetRef<ModelAsset> AssetManager::LoadModel(std::string_view path, std::string* error)
    {
        const std::string key = NormalizeKey(path);
        const auto cached = m_Models.find(key);
        if (cached != m_Models.end())
        {
            return AssetRef<ModelAsset>(cached->second, key);
        }

        const std::filesystem::path resolved = ResolveKey(key);
        if (!FileSystem::Exists(resolved))
        {
            SetError(error, "Model file does not exist: " + key);
            return {};
        }

        const std::string extension = FileSystem::GetExtension(resolved);
        if (extension == "obj")
        {
            const std::shared_ptr<MeshAsset> mesh = ObjMeshLoader::Load(FileSystem::Normalize(resolved), error);
            if (mesh == nullptr)
            {
                return {};
            }
            mesh->SourcePath = key;
            m_Meshes.emplace(key, mesh);

            auto model = std::make_shared<ModelAsset>();
            model->Name = mesh->Name;
            model->SourcePath = key;
            ModelNode root;
            root.Name = mesh->Name;
            root.Mesh = mesh;
            model->Roots.push_back(std::move(root));
            m_Models.emplace(key, model);
            return AssetRef<ModelAsset>(model, key);
        }

        if (IsModelExtension(extension))
        {
            GltfLoadResult result;
            if (!GltfLoader::Load(FileSystem::Normalize(resolved), key, result))
            {
                SetError(error, result.Error.empty() ? "Failed to load glTF model" : result.Error);
                return {};
            }

            for (auto& [meshKey, mesh] : result.Meshes)
            {
                m_Meshes.emplace(meshKey, std::move(mesh));
            }
            for (auto& [textureKey, texture] : result.Textures)
            {
                m_Textures.emplace(textureKey, std::move(texture));
            }
            m_Models.emplace(key, result.Model);
            return AssetRef<ModelAsset>(result.Model, key);
        }

        SetError(error, "Unsupported model extension: " + extension);
        return {};
    }

    AssetRef<MeshAsset> AssetManager::LoadMesh(std::string_view path, std::string* error)
    {
        const std::string key = NormalizeKey(path);
        const auto cached = m_Meshes.find(key);
        if (cached != m_Meshes.end())
        {
            return AssetRef<MeshAsset>(cached->second, key);
        }

        const size_t hash = key.find('#');
        if (hash != std::string::npos)
        {
            const std::string base = key.substr(0, hash);
            if (LoadModel(base, error).IsValid())
            {
                const auto iterator = m_Meshes.find(key);
                if (iterator != m_Meshes.end())
                {
                    return AssetRef<MeshAsset>(iterator->second, key);
                }
            }
            SetError(error, "Mesh '" + key + "' was not produced by its model");
            return {};
        }

        const std::string extension = FileSystem::GetExtension(key);
        if (IsModelExtension(extension))
        {
            if (!LoadModel(key, error).IsValid())
            {
                return {};
            }
            // Prefer the first mesh of the model when no explicit #mesh index is given.
            const std::string prefix = key + "#mesh";
            for (const auto& [meshKey, mesh] : m_Meshes)
            {
                if (meshKey.rfind(prefix, 0) == 0)
                {
                    return AssetRef<MeshAsset>(mesh, meshKey);
                }
            }
            SetError(error, "glTF model contains no meshes: " + key);
            return {};
        }

        if (extension == "obj")
        {
            if (!LoadModel(key, error).IsValid())
            {
                return {};
            }
            const auto iterator = m_Meshes.find(key);
            if (iterator != m_Meshes.end())
            {
                return AssetRef<MeshAsset>(iterator->second, key);
            }
        }

        SetError(error, "Unsupported mesh extension: " + extension);
        return {};
    }

    AssetRef<Texture> AssetManager::LoadTextureFile(const std::string& key, std::string* error)
    {
        const std::filesystem::path resolved = ResolveKey(key);
        const std::optional<Image> image = ImageLoader::Load(FileSystem::Normalize(resolved), error);
        if (!image.has_value())
        {
            return {};
        }

        auto texture = std::make_shared<Texture>();
        texture->Name = FileSystem::GetStem(key);
        texture->SourcePath = key;
        texture->Pixels = *image;
        texture->SRGB = !IsNonColorTextureName(key) && image->Format == ImageFormat::RGBA8;
        texture->GenerateMips = true;
        m_Textures.emplace(key, texture);
        return AssetRef<Texture>(texture, key);
    }

    AssetRef<Texture> AssetManager::LoadTexture(std::string_view path, std::string* error)
    {
        const std::string key = NormalizeKey(path);
        const auto cached = m_Textures.find(key);
        if (cached != m_Textures.end())
        {
            return AssetRef<Texture>(cached->second, key);
        }

        const size_t hash = key.find('#');
        if (hash != std::string::npos)
        {
            const std::string base = key.substr(0, hash);
            if (LoadModel(base, error).IsValid())
            {
                const auto iterator = m_Textures.find(key);
                if (iterator != m_Textures.end())
                {
                    return AssetRef<Texture>(iterator->second, key);
                }
            }
            SetError(error, "Texture '" + key + "' was not produced by its model");
            return {};
        }

        if (!IsTextureExtension(FileSystem::GetExtension(key)))
        {
            SetError(error, "Unsupported texture extension: " + FileSystem::GetExtension(key));
            return {};
        }
        return LoadTextureFile(key, error);
    }

    AssetRef<Environment> AssetManager::LoadEnvironment(std::string_view path, std::string* error)
    {
        const std::string key = NormalizeKey(path);
        const auto cached = m_Environments.find(key);
        if (cached != m_Environments.end())
        {
            return AssetRef<Environment>(cached->second, key);
        }

        const std::filesystem::path resolved = ResolveKey(key);
        const std::optional<Image> image = ImageLoader::Load(FileSystem::Normalize(resolved), error);
        if (!image.has_value())
        {
            return {};
        }

        auto environment = std::make_shared<Environment>();
        environment->Name = FileSystem::GetStem(key);
        environment->SourcePath = key;
        environment->Equirectangular = *image;
        if (!IblBaker::Bake(*image, *environment, error))
        {
            return {};
        }
        m_Environments.emplace(key, environment);
        return AssetRef<Environment>(environment, key);
    }

    AssetRef<Sound> AssetManager::LoadSound(std::string_view path, std::string* error)
    {
        const std::string key = NormalizeKey(path);
        const auto cached = m_Sounds.find(key);
        if (cached != m_Sounds.end())
        {
            return AssetRef<Sound>(cached->second, key);
        }

        const std::filesystem::path resolved = ResolveKey(key);
        const std::optional<std::vector<uint8>> bytes = FileSystem::ReadBinaryFile(FileSystem::Normalize(resolved));
        if (!bytes.has_value())
        {
            SetError(error, "Could not read sound file: " + key);
            return {};
        }

        auto sound = std::make_shared<Sound>();
        sound->Name = FileSystem::GetStem(key);
        sound->SourcePath = key;
        sound->EncodedData = *bytes;
        m_Sounds.emplace(key, sound);
        return AssetRef<Sound>(sound, key);
    }

    std::optional<std::string> AssetManager::LoadScript(std::string_view path, std::string* error)
    {
        const std::string key = NormalizeKey(path);
        const std::filesystem::path resolved = ResolveKey(key);
        std::optional<std::string> contents = FileSystem::ReadTextFile(FileSystem::Normalize(resolved));
        if (!contents.has_value())
        {
            SetError(error, "Could not read script: " + key);
        }
        return contents;
    }

    void AssetManager::Clear()
    {
        m_Models.clear();
        m_Meshes.clear();
        m_Textures.clear();
        m_Environments.clear();
        m_Sounds.clear();
    }
}
