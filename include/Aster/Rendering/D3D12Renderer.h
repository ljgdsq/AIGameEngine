#pragma once

#include "Aster/Rendering/Camera.h"
#include "Aster/Rendering/D3D12Context.h"
#include "Aster/Scene/Scene.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <string>
#include <unordered_map>

namespace Aster
{
    struct RenderView
    {
        glm::mat4 ViewProjection{ 1.0f };
        glm::vec3 Position{ 0.0f };
        uint32_t ViewportX = 0;
        uint32_t ViewportY = 0;
        uint32_t ViewportWidth = 0;
        uint32_t ViewportHeight = 0;
    };

    struct RendererStats
    {
        uint32_t DrawCalls = 0;
        uint32_t Triangles = 0;
        uint32_t Meshes = 0;
    };

    class D3D12Renderer
    {
    public:
        D3D12Renderer();
        ~D3D12Renderer();

        D3D12Renderer(const D3D12Renderer&) = delete;
        D3D12Renderer& operator=(const D3D12Renderer&) = delete;

        [[nodiscard]] bool Initialize(D3D12Context& context, std::string* error = nullptr);
        void Shutdown();
        void Render(Scene& scene, const RenderView& view);
        void InvalidateMesh(const MeshAsset* mesh);
        void SetShaderDirectory(std::string directory);

        [[nodiscard]] const RendererStats& GetStats() const { return m_Stats; }
        [[nodiscard]] bool IsInitialized() const { return m_Context != nullptr; }

    private:
        struct GpuMesh
        {
            nvrhi::BufferHandle VertexBuffer;
            nvrhi::BufferHandle IndexBuffer;
            uint32_t IndexCount = 0;
        };

        struct GpuTexture
        {
            nvrhi::TextureHandle Texture;
        };

        [[nodiscard]] GpuMesh* GetOrCreateMesh(const MeshAsset& mesh);
        [[nodiscard]] GpuTexture* GetOrCreateTexture(const Texture& texture);
        bool EnsureHdrTarget(uint32_t width, uint32_t height, std::string* error = nullptr);
        void UploadMesh(GpuMesh& gpuMesh, const Mesh& mesh);
        [[nodiscard]] bool CreatePipeline(std::string* error);
        [[nodiscard]] std::string ResolveShaderPath(const char* name) const;
        [[nodiscard]] static std::vector<uint8_t> ReadFile(const std::string& path);

        D3D12Context* m_Context = nullptr;
        nvrhi::GraphicsPipelineHandle m_Pipeline;
        nvrhi::GraphicsPipelineHandle m_ShadowPipeline;
        nvrhi::GraphicsPipelineHandle m_PostPipeline;
        nvrhi::BindingLayoutHandle m_BindingLayout;
        nvrhi::BindingLayoutHandle m_ShadowBindingLayout;
        nvrhi::BindingLayoutHandle m_PostBindingLayout;
        nvrhi::BufferHandle m_FrameBuffer;
        nvrhi::BufferHandle m_ObjectBuffer;
        nvrhi::BufferHandle m_ShadowObjectBuffer;
        nvrhi::InputLayoutHandle m_InputLayout;
        nvrhi::TextureHandle m_WhiteTexture;
        nvrhi::TextureHandle m_ShadowTexture;
        nvrhi::FramebufferHandle m_ShadowFramebuffer;
        nvrhi::TextureHandle m_HdrTexture;
        nvrhi::FramebufferHandle m_HdrFramebuffer;
        nvrhi::SamplerHandle m_Sampler;
        nvrhi::SamplerHandle m_ShadowSampler;
        std::unordered_map<const MeshAsset*, GpuMesh> m_Meshes;
        std::unordered_map<const Texture*, GpuTexture> m_Textures;
        RendererStats m_Stats;
        uint32_t m_ObjectBufferCapacity = 4096;
        uint32_t m_ShadowMapSize = 2048;
        uint32_t m_HdrWidth = 0;
        uint32_t m_HdrHeight = 0;
        double m_Time = 0.0;
        bool m_WhiteTextureUploaded = false;
        std::string m_ShaderDirectory = ASTER_SHADER_DIR;
    };
}
