#pragma once

#include "Aster/Core/Base.h"
#include "Aster/Math/Math.h"

#include <glm/glm.hpp>

#include <array>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace Aster
{
    enum class ImageFormat : uint8
    {
        RGBA8,
        RGBA32F
    };

    struct Image
    {
        uint32 Width = 0;
        uint32 Height = 0;
        ImageFormat Format = ImageFormat::RGBA8;
        std::vector<uint8> Data;

        [[nodiscard]] bool IsValid() const { return Width > 0 && Height > 0 && !Data.empty(); }
        [[nodiscard]] uint32 GetBytesPerPixel() const { return Format == ImageFormat::RGBA8 ? 4U : 16U; }
        [[nodiscard]] size_t GetRowPitch() const { return static_cast<size_t>(Width) * GetBytesPerPixel(); }
        [[nodiscard]] size_t GetByteSize() const { return GetRowPitch() * Height; }
        [[nodiscard]] const float* GetFloatPixels() const
        {
            return Format == ImageFormat::RGBA32F ? reinterpret_cast<const float*>(Data.data()) : nullptr;
        }
        [[nodiscard]] glm::vec4 Sample(float u, float v) const;
    };

    enum class TextureWrap : uint8
    {
        Repeat,
        Clamp,
        Mirror
    };

    enum class TextureFilter : uint8
    {
        Nearest,
        Linear
    };

    struct Texture
    {
        std::string Name;
        std::string SourcePath;
        Image Pixels;
        bool SRGB = true;
        TextureWrap Wrap = TextureWrap::Repeat;
        TextureFilter Filter = TextureFilter::Linear;
        bool GenerateMips = true;

        [[nodiscard]] bool IsValid() const { return Pixels.IsValid(); }
    };

    struct CameraData;
    struct Texture;
    struct Mesh;
    struct MeshAsset;
    struct Environment;
    struct Sound;

    template<typename T>
    struct AssetRef
    {
        std::shared_ptr<T> Asset;
        std::string Path;

        AssetRef() = default;
        AssetRef(std::shared_ptr<T> asset, std::string path) : Asset(std::move(asset)), Path(std::move(path)) { }

        [[nodiscard]] bool IsValid() const { return Asset != nullptr; }
        [[nodiscard]] explicit operator bool() const { return IsValid(); }
        [[nodiscard]] T* operator->() const { return Asset.get(); }
        [[nodiscard]] T& operator*() const { return *Asset; }
        void Reset()
        {
            Asset.reset();
            Path.clear();
        }
    };

    struct Material
    {
        std::string Name = "Material";
        glm::vec4 BaseColor{ 1.0f };
        float Metallic = 0.0f;
        float Roughness = 0.5f;
        glm::vec3 Emissive{ 0.0f };
        float EmissiveStrength = 1.0f;
        float Opacity = 1.0f;
        float AlphaCutoff = 0.5f;
        bool DoubleSided = false;
        bool AlphaMask = false;
        bool AlphaBlend = false;

        AssetRef<Texture> AlbedoMap;
        AssetRef<Texture> NormalMap;
        AssetRef<Texture> MetallicRoughnessMap;
        AssetRef<Texture> EmissiveMap;
        AssetRef<Texture> OcclusionMap;
    };

    struct Vertex
    {
        glm::vec3 Position{ 0.0f };
        glm::vec3 Normal{ 0.0f, 1.0f, 0.0f };
        glm::vec2 UV{ 0.0f };
        glm::vec4 Tangent{ 1.0f, 0.0f, 0.0f, 1.0f };
    };

    struct SubMesh
    {
        uint32 FirstIndex = 0;
        uint32 IndexCount = 0;
        int32 MaterialIndex = 0;
        Aabb Bounds;
    };

    struct Mesh
    {
        std::string Name;
        std::vector<Vertex> Vertices;
        std::vector<uint32> Indices;
        std::vector<SubMesh> SubMeshes;
        Aabb Bounds;

        [[nodiscard]] bool IsValid() const { return !Vertices.empty() && !Indices.empty(); }
        void RecalculateBounds();
        void RecalculateNormals();
        void GenerateTangents();
    };

    struct MeshAsset
    {
        std::string Name;
        std::string SourcePath;
        std::shared_ptr<Mesh> Geometry;
        std::vector<Material> Materials;

        [[nodiscard]] bool IsValid() const { return Geometry && Geometry->IsValid(); }
    };

    struct ModelNode
    {
        std::string Name;
        Transform LocalTransform;
        std::shared_ptr<MeshAsset> Mesh;
        std::vector<ModelNode> Children;
    };

    struct ModelAsset
    {
        std::string Name;
        std::string SourcePath;
        std::vector<ModelNode> Roots;
    };

    struct CubemapImage
    {
        uint32 FaceSize = 0;
        uint32 MipCount = 0;
        std::vector<glm::vec4> Pixels;

        [[nodiscard]] bool IsValid() const { return FaceSize > 0 && MipCount > 0 && !Pixels.empty(); }
        [[nodiscard]] size_t GetFaceMipOffset(uint32 face, uint32 mip) const;
        [[nodiscard]] size_t GetFaceMipPixelCount(uint32 mip) const;
        [[nodiscard]] const glm::vec4* GetFaceMipData(uint32 face, uint32 mip) const;
        [[nodiscard]] uint32 GetMipSize(uint32 mip) const;
    };

    struct Environment
    {
        std::string Name;
        std::string SourcePath;
        CubemapImage PrefilteredSpecular;
        Image Equirectangular;
        std::array<glm::vec3, 9> IrradianceSh{};
        std::vector<glm::vec2> BrdfLut;
        uint32 BrdfLutSize = 0;
        float Intensity = 1.0f;

        [[nodiscard]] bool IsValid() const { return PrefilteredSpecular.IsValid() && BrdfLutSize > 0; }
    };

    struct Sound
    {
        std::string Name;
        std::string SourcePath;
        std::vector<uint8> EncodedData;
        [[nodiscard]] bool IsValid() const { return !EncodedData.empty(); }
    };
}
