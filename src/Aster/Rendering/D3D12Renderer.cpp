#include "Aster/Rendering/D3D12Renderer.h"

#include "Aster/Core/Log.h"
#include "Aster/Rendering/Pbr.h"

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <vector>

namespace Aster
{
    namespace
    {
        struct FrameConstants
        {
            glm::mat4 ViewProjection{ 1.0f };
            glm::vec4 CameraPosition{ 0.0f };
            glm::vec4 LightDirection{ 0.0f, -1.0f, 0.0f, 0.0f };
            glm::vec4 LightColor{ 1.0f };
            glm::vec4 AmbientColor{ 0.1f, 0.1f, 0.1f, 1.0f };
            glm::vec4 PointLightPosition[4]{};
            glm::vec4 PointLightColor[4]{};
            glm::vec4 SpotLightPosition[4]{};
            glm::vec4 SpotLightDirection[4]{};
            glm::vec4 SpotLightColor[4]{};
            int PointLightCount = 0;
            int SpotLightCount = 0;
            float Exposure = 1.0f;
            float Time = 0.0f;
            glm::mat4 LightViewProjection{ 1.0f };
            glm::vec4 ShadowParams{ 0.0025f, 0.00048828125f, 0.0f, 0.0f };
        };

        struct ObjectConstants
        {
            glm::mat4 Model{ 1.0f };
            glm::mat4 NormalMatrix{ 1.0f };
            glm::vec4 BaseColor{ 1.0f };
            glm::vec4 Material{ 0.0f, 0.5f, 0.0f, 1.0f };
            glm::vec4 Emissive{ 0.0f };
        };

        static_assert(sizeof(FrameConstants) == 544, "Frame constant layout must match Pbr.hlsl");
        static_assert(sizeof(ObjectConstants) == 176, "Object constant layout must match Pbr.hlsl");
        constexpr uint64_t ObjectStride = 256;
    }

    D3D12Renderer::D3D12Renderer() = default;

    D3D12Renderer::~D3D12Renderer()
    {
        Shutdown();
    }

    void D3D12Renderer::SetShaderDirectory(std::string directory)
    {
        m_ShaderDirectory = std::move(directory);
    }

    std::string D3D12Renderer::ResolveShaderPath(const char* name) const
    {
        return m_ShaderDirectory + "/" + name;
    }

    bool D3D12Renderer::Initialize(D3D12Context& context, std::string* error)
    {
        if (m_Context != nullptr)
        {
            return true;
        }
        m_Context = &context;

        nvrhi::IDevice* device = m_Context->GetDevice();
        nvrhi::BufferDesc frameDesc;
        frameDesc.setByteSize(sizeof(FrameConstants))
            .setIsConstantBuffer(true)
            .setInitialState(nvrhi::ResourceStates::ConstantBuffer)
            .setKeepInitialState(true)
            .setDebugName("Aster Frame Constants");
        m_FrameBuffer = device->createBuffer(frameDesc);

        nvrhi::BufferDesc objectDesc;
        objectDesc.setByteSize(ObjectStride * m_ObjectBufferCapacity)
            .setIsConstantBuffer(true)
            .setInitialState(nvrhi::ResourceStates::ConstantBuffer)
            .setKeepInitialState(true)
            .setDebugName("Aster Object Constants");
        m_ObjectBuffer = device->createBuffer(objectDesc);

        nvrhi::BufferDesc shadowObjectDesc;
        shadowObjectDesc.setByteSize(ObjectStride * m_ObjectBufferCapacity)
            .setIsConstantBuffer(true)
            .setInitialState(nvrhi::ResourceStates::ConstantBuffer)
            .setKeepInitialState(true)
            .setDebugName("Aster Shadow Object Constants");
        m_ShadowObjectBuffer = device->createBuffer(shadowObjectDesc);
        if (!m_FrameBuffer || !m_ObjectBuffer || !m_ShadowObjectBuffer)
        {
            if (error != nullptr)
            {
                *error = "Failed to create renderer constant buffers";
            }
            return false;
        }

        nvrhi::TextureDesc shadowDesc;
        shadowDesc.setWidth(m_ShadowMapSize)
            .setHeight(m_ShadowMapSize)
            .setFormat(nvrhi::Format::D32)
            .setIsTypeless(true)
            .setIsRenderTarget(true)
            .setInitialState(nvrhi::ResourceStates::DepthWrite)
            .setDebugName("Aster Shadow Map");
        m_ShadowTexture = device->createTexture(shadowDesc);
        if (m_ShadowTexture)
        {
            nvrhi::FramebufferDesc shadowFramebufferDesc;
            shadowFramebufferDesc.setDepthAttachment(m_ShadowTexture);
            m_ShadowFramebuffer = device->createFramebuffer(shadowFramebufferDesc);
        }
        m_ShadowSampler = device->createSampler(nvrhi::SamplerDesc()
            .setAllFilters(false)
            .setAllAddressModes(nvrhi::SamplerAddressMode::Clamp));

        nvrhi::TextureDesc whiteDesc;
        whiteDesc.setWidth(1)
            .setHeight(1)
            .setFormat(nvrhi::Format::RGBA8_UNORM)
            .setInitialState(nvrhi::ResourceStates::ShaderResource)
            .setKeepInitialState(true)
            .setDebugName("Aster White Texture");
        m_WhiteTexture = device->createTexture(whiteDesc);
        m_Sampler = device->createSampler(nvrhi::SamplerDesc()
            .setAllFilters(true)
            .setMipFilter(false)
            .setAllAddressModes(nvrhi::SamplerAddressMode::Repeat)
            .setMaxAnisotropy(4.0f));
        if (!m_WhiteTexture || !m_Sampler || !m_ShadowTexture || !m_ShadowFramebuffer || !m_ShadowSampler)
        {
            if (error != nullptr)
            {
                *error = "Failed to create renderer texture resources";
            }
            return false;
        }

        if (!CreatePipeline(error))
        {
            return false;
        }

        ASTER_LOG_INFO("Renderer", "D3D12 forward PBR renderer initialized");
        return true;
    }

    bool D3D12Renderer::CreatePipeline(std::string* error)
    {
        nvrhi::IDevice* device = m_Context->GetDevice();

        const std::vector<uint8_t> vertexShader = ReadFile(ResolveShaderPath("Pbr.vs.dxil"));
        const std::vector<uint8_t> pixelShader = ReadFile(ResolveShaderPath("Pbr.ps.dxil"));
        const std::vector<uint8_t> shadowVertexShader = ReadFile(ResolveShaderPath("Pbr.shadow.vs.dxil"));
        const std::vector<uint8_t> shadowPixelShader = ReadFile(ResolveShaderPath("Pbr.shadow.ps.dxil"));
        const std::vector<uint8_t> postVertexShader = ReadFile(ResolveShaderPath("Pbr.post.vs.dxil"));
        const std::vector<uint8_t> postPixelShader = ReadFile(ResolveShaderPath("Pbr.post.ps.dxil"));
        if (vertexShader.empty() || pixelShader.empty() || shadowVertexShader.empty() || shadowPixelShader.empty() ||
            postVertexShader.empty() || postPixelShader.empty())
        {
            if (error != nullptr)
            {
                *error = "Could not read compiled PBR shaders. Expected Pbr.vs.dxil, Pbr.ps.dxil, "
                    "Pbr.shadow.vs.dxil, Pbr.shadow.ps.dxil, Pbr.post.vs.dxil, and Pbr.post.ps.dxil in " +
                    m_ShaderDirectory;
            }
            return false;
        }

        nvrhi::ShaderHandle vs = device->createShader(
            nvrhi::ShaderDesc().setShaderType(nvrhi::ShaderType::Vertex).setEntryName("VSMain").setDebugName(
                "PBR Vertex"),
            vertexShader.data(), vertexShader.size());
        nvrhi::ShaderHandle ps = device->createShader(
            nvrhi::ShaderDesc().setShaderType(nvrhi::ShaderType::Pixel).setEntryName("PSMain").setDebugName(
                "PBR Pixel"),
            pixelShader.data(), pixelShader.size());
        nvrhi::ShaderHandle shadowVs = device->createShader(
            nvrhi::ShaderDesc().setShaderType(nvrhi::ShaderType::Vertex).setEntryName("VSShadow").setDebugName(
                "Shadow Vertex"),
            shadowVertexShader.data(), shadowVertexShader.size());
        nvrhi::ShaderHandle shadowPs = device->createShader(
            nvrhi::ShaderDesc().setShaderType(nvrhi::ShaderType::Pixel).setEntryName("PSShadow").setDebugName(
                "Shadow Pixel"),
            shadowPixelShader.data(), shadowPixelShader.size());
        nvrhi::ShaderHandle postVs = device->createShader(
            nvrhi::ShaderDesc().setShaderType(nvrhi::ShaderType::Vertex).setEntryName("VSFullscreen")
                .setDebugName("Post Vertex"),
            postVertexShader.data(), postVertexShader.size());
        nvrhi::ShaderHandle postPs = device->createShader(
            nvrhi::ShaderDesc().setShaderType(nvrhi::ShaderType::Pixel).setEntryName("PSTonemap")
                .setDebugName("Post Pixel"),
            postPixelShader.data(), postPixelShader.size());
        if (!vs || !ps || !shadowVs || !shadowPs || !postVs || !postPs)
        {
            if (error != nullptr)
            {
                *error = "Failed to create PBR shaders";
            }
            return false;
        }

        std::vector<nvrhi::VertexAttributeDesc> attributes(4);
        attributes[0].setName("POSITION").setFormat(nvrhi::Format::RGB32_FLOAT).setBufferIndex(0)
            .setOffset(static_cast<uint32_t>(offsetof(Vertex, Position))).setElementStride(sizeof(Vertex));
        attributes[1].setName("NORMAL").setFormat(nvrhi::Format::RGB32_FLOAT).setBufferIndex(0)
            .setOffset(static_cast<uint32_t>(offsetof(Vertex, Normal))).setElementStride(sizeof(Vertex));
        attributes[2].setName("TEXCOORD").setFormat(nvrhi::Format::RG32_FLOAT).setBufferIndex(0)
            .setOffset(static_cast<uint32_t>(offsetof(Vertex, UV))).setElementStride(sizeof(Vertex));
        attributes[3].setName("TANGENT").setFormat(nvrhi::Format::RGBA32_FLOAT).setBufferIndex(0)
            .setOffset(static_cast<uint32_t>(offsetof(Vertex, Tangent))).setElementStride(sizeof(Vertex));
        m_InputLayout = device->createInputLayout(attributes.data(), static_cast<uint32_t>(attributes.size()),
            vs);

        nvrhi::BindingLayoutDesc layoutDesc;
        layoutDesc.setVisibility(nvrhi::ShaderType::All);
        layoutDesc.addItem(nvrhi::BindingLayoutItem::ConstantBuffer(0));
        layoutDesc.addItem(nvrhi::BindingLayoutItem::ConstantBuffer(1));
        layoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(0));
        layoutDesc.addItem(nvrhi::BindingLayoutItem::Sampler(0));
        layoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(1));
        layoutDesc.addItem(nvrhi::BindingLayoutItem::Sampler(1));
        m_BindingLayout = device->createBindingLayout(layoutDesc);

        nvrhi::BindingLayoutDesc shadowLayoutDesc;
        shadowLayoutDesc.setVisibility(nvrhi::ShaderType::All);
        shadowLayoutDesc.addItem(nvrhi::BindingLayoutItem::ConstantBuffer(0));
        shadowLayoutDesc.addItem(nvrhi::BindingLayoutItem::ConstantBuffer(1));
        m_ShadowBindingLayout = device->createBindingLayout(shadowLayoutDesc);

        nvrhi::BindingLayoutDesc postLayoutDesc;
        postLayoutDesc.setVisibility(nvrhi::ShaderType::All);
        postLayoutDesc.addItem(nvrhi::BindingLayoutItem::ConstantBuffer(0));
        postLayoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(2));
        postLayoutDesc.addItem(nvrhi::BindingLayoutItem::Sampler(2));
        m_PostBindingLayout = device->createBindingLayout(postLayoutDesc);

        nvrhi::GraphicsPipelineDesc pipelineDesc;
        pipelineDesc.setPrimType(nvrhi::PrimitiveType::TriangleList)
            .setInputLayout(m_InputLayout)
            .setVertexShader(vs)
            .setPixelShader(ps)
            .addBindingLayout(m_BindingLayout);
        pipelineDesc.renderState.setDepthStencilState(
            nvrhi::DepthStencilState().enableDepthTest().enableDepthWrite()
                .setDepthFunc(nvrhi::ComparisonFunc::Less));
        pipelineDesc.renderState.setRasterState(
            nvrhi::RasterState().setCullMode(nvrhi::RasterCullMode::None).setDepthClipEnable(true));

        nvrhi::FramebufferInfo framebufferInfo;
        framebufferInfo.addColorFormat(nvrhi::Format::RGBA16_FLOAT);
        framebufferInfo.setDepthFormat(nvrhi::Format::D32);
        m_Pipeline = device->createGraphicsPipeline(pipelineDesc, framebufferInfo);
        if (!m_Pipeline)
        {
            m_Context->LogDebugMessages();
            if (error != nullptr)
            {
                *error = "Failed to create the PBR graphics pipeline";
            }
            return false;
        }

        nvrhi::GraphicsPipelineDesc shadowPipelineDesc;
        shadowPipelineDesc.setPrimType(nvrhi::PrimitiveType::TriangleList)
            .setInputLayout(m_InputLayout)
            .setVertexShader(shadowVs)
            .setPixelShader(shadowPs)
            .addBindingLayout(m_ShadowBindingLayout);
        shadowPipelineDesc.renderState.setDepthStencilState(
            nvrhi::DepthStencilState().enableDepthTest().enableDepthWrite()
                .setDepthFunc(nvrhi::ComparisonFunc::Less));
        shadowPipelineDesc.renderState.setRasterState(
            nvrhi::RasterState().setCullMode(nvrhi::RasterCullMode::None).setDepthClipEnable(true)
                .setDepthBias(1000).setSlopeScaleDepthBias(1.5f));

        nvrhi::FramebufferInfo shadowFramebufferInfo;
        shadowFramebufferInfo.setDepthFormat(nvrhi::Format::D32);
        m_ShadowPipeline = device->createGraphicsPipeline(shadowPipelineDesc, shadowFramebufferInfo);
        if (!m_ShadowPipeline)
        {
            m_Context->LogDebugMessages();
            if (error != nullptr)
            {
                *error = "Failed to create the shadow graphics pipeline";
            }
            return false;
        }

        nvrhi::GraphicsPipelineDesc postPipelineDesc;
        postPipelineDesc.setPrimType(nvrhi::PrimitiveType::TriangleList)
            .setVertexShader(postVs)
            .setPixelShader(postPs)
            .addBindingLayout(m_PostBindingLayout);
        postPipelineDesc.renderState.setDepthStencilState(
            nvrhi::DepthStencilState().disableDepthTest().disableDepthWrite());
        postPipelineDesc.renderState.setRasterState(
            nvrhi::RasterState().setCullMode(nvrhi::RasterCullMode::None).setDepthClipEnable(false));
        nvrhi::FramebufferInfo postFramebufferInfo;
        postFramebufferInfo.addColorFormat(nvrhi::Format::RGBA8_UNORM);
        postFramebufferInfo.setDepthFormat(nvrhi::Format::D32);
        m_PostPipeline = device->createGraphicsPipeline(postPipelineDesc, postFramebufferInfo);
        if (!m_PostPipeline)
        {
            m_Context->LogDebugMessages();
            if (error != nullptr)
            {
                *error = "Failed to create the tonemap graphics pipeline";
            }
            return false;
        }
        return true;
    }

    void D3D12Renderer::Shutdown()
    {
        m_Meshes.clear();
        m_Textures.clear();
        m_WhiteTexture = nullptr;
        m_ShadowTexture = nullptr;
        m_ShadowFramebuffer = nullptr;
        m_HdrTexture = nullptr;
        m_HdrFramebuffer = nullptr;
        m_Sampler = nullptr;
        m_ShadowSampler = nullptr;
        m_Pipeline = nullptr;
        m_ShadowPipeline = nullptr;
        m_PostPipeline = nullptr;
        m_BindingLayout = nullptr;
        m_ShadowBindingLayout = nullptr;
        m_PostBindingLayout = nullptr;
        m_FrameBuffer = nullptr;
        m_ObjectBuffer = nullptr;
        m_ShadowObjectBuffer = nullptr;
        m_InputLayout = nullptr;
        m_HdrWidth = 0;
        m_HdrHeight = 0;
        m_Context = nullptr;
        m_Stats = {};
        m_WhiteTextureUploaded = false;
    }

    bool D3D12Renderer::EnsureHdrTarget(uint32_t width, uint32_t height, std::string* error)
    {
        if (m_Context == nullptr || width == 0 || height == 0)
        {
            return false;
        }
        if (m_HdrTexture && m_HdrFramebuffer && m_HdrWidth == width && m_HdrHeight == height)
        {
            return true;
        }

        m_HdrFramebuffer = nullptr;
        m_HdrTexture = nullptr;
        nvrhi::TextureDesc desc;
        desc.setWidth(width)
            .setHeight(height)
            .setFormat(nvrhi::Format::RGBA16_FLOAT)
            .setIsRenderTarget(true)
            .setInitialState(nvrhi::ResourceStates::RenderTarget)
            .setKeepInitialState(true)
            .setDebugName("Aster HDR Color");
        m_HdrTexture = m_Context->GetDevice()->createTexture(desc);
        if (!m_HdrTexture)
        {
            if (error != nullptr)
            {
                *error = "Failed to create the HDR render target";
            }
            return false;
        }

        nvrhi::FramebufferDesc framebufferDesc;
        framebufferDesc.addColorAttachment(m_HdrTexture);
        framebufferDesc.setDepthAttachment(m_Context->GetDepthTexture());
        m_HdrFramebuffer = m_Context->GetDevice()->createFramebuffer(framebufferDesc);
        if (!m_HdrFramebuffer)
        {
            if (error != nullptr)
            {
                *error = "Failed to create the HDR framebuffer";
            }
            return false;
        }
        m_HdrWidth = width;
        m_HdrHeight = height;
        return true;
    }

    D3D12Renderer::GpuMesh* D3D12Renderer::GetOrCreateMesh(const MeshAsset& mesh)
    {
        if (!mesh.Geometry || !mesh.Geometry->IsValid())
        {
            return nullptr;
        }

        const auto existing = m_Meshes.find(&mesh);
        if (existing != m_Meshes.end())
        {
            return &existing->second;
        }

        GpuMesh gpuMesh;
        UploadMesh(gpuMesh, *mesh.Geometry);
        if (!gpuMesh.VertexBuffer || !gpuMesh.IndexBuffer)
        {
            return nullptr;
        }
        const auto [iterator, inserted] = m_Meshes.emplace(&mesh, std::move(gpuMesh));
        return inserted ? &iterator->second : nullptr;
    }

    D3D12Renderer::GpuTexture* D3D12Renderer::GetOrCreateTexture(const Texture& texture)
    {
        if (m_Context == nullptr || !texture.Pixels.IsValid())
        {
            return nullptr;
        }

        const auto existing = m_Textures.find(&texture);
        if (existing != m_Textures.end())
        {
            return &existing->second;
        }

        nvrhi::TextureDesc desc;
        desc.setWidth(texture.Pixels.Width)
            .setHeight(texture.Pixels.Height)
            .setMipLevels(1)
            .setFormat(texture.Pixels.Format == ImageFormat::RGBA32F ? nvrhi::Format::RGBA32_FLOAT :
                nvrhi::Format::RGBA8_UNORM)
            .setInitialState(nvrhi::ResourceStates::ShaderResource)
            .setKeepInitialState(true)
            .setDebugName(texture.Name.empty() ? "Aster Texture" : texture.Name);
        GpuTexture gpuTexture;
        gpuTexture.Texture = m_Context->GetDevice()->createTexture(desc);
        if (!gpuTexture.Texture)
        {
            return nullptr;
        }

        m_Context->GetCommandList()->writeTexture(gpuTexture.Texture, 0, 0, texture.Pixels.Data.data(),
            texture.Pixels.GetRowPitch());
        const auto [iterator, inserted] = m_Textures.emplace(&texture, std::move(gpuTexture));
        return inserted ? &iterator->second : nullptr;
    }

    void D3D12Renderer::UploadMesh(GpuMesh& gpuMesh, const Mesh& mesh)
    {
        if (m_Context == nullptr || mesh.Vertices.empty() || mesh.Indices.empty())
        {
            return;
        }
        nvrhi::IDevice* device = m_Context->GetDevice();
        nvrhi::ICommandList* commandList = m_Context->GetCommandList();

        nvrhi::BufferDesc vertexDesc;
        vertexDesc.setByteSize(mesh.Vertices.size() * sizeof(Vertex))
            .setIsVertexBuffer(true)
            .setInitialState(nvrhi::ResourceStates::VertexBuffer)
            .setKeepInitialState(true)
            .setDebugName(mesh.Name + " Vertices");
        gpuMesh.VertexBuffer = device->createBuffer(vertexDesc);
        commandList->writeBuffer(gpuMesh.VertexBuffer, mesh.Vertices.data(),
            mesh.Vertices.size() * sizeof(Vertex));

        nvrhi::BufferDesc indexDesc;
        indexDesc.setByteSize(mesh.Indices.size() * sizeof(uint32))
            .setIsIndexBuffer(true)
            .setInitialState(nvrhi::ResourceStates::IndexBuffer)
            .setKeepInitialState(true)
            .setDebugName(mesh.Name + " Indices");
        gpuMesh.IndexBuffer = device->createBuffer(indexDesc);
        commandList->writeBuffer(gpuMesh.IndexBuffer, mesh.Indices.data(),
            mesh.Indices.size() * sizeof(uint32));
        gpuMesh.IndexCount = static_cast<uint32_t>(mesh.Indices.size());
    }

    void D3D12Renderer::InvalidateMesh(const MeshAsset* mesh)
    {
        if (mesh != nullptr)
        {
            m_Meshes.erase(mesh);
        }
    }

    void D3D12Renderer::Render(Scene& scene, const RenderView& view)
    {
        if (m_Context == nullptr || !m_Pipeline)
        {
            return;
        }

        m_Stats = {};
        nvrhi::ICommandList* commandList = m_Context->GetCommandList();
        const nvrhi::TextureSubresourceSet allSubresources(0, 1, 0, 1);
        std::string hdrError;
        if (!EnsureHdrTarget(m_Context->GetWidth(), m_Context->GetHeight(), &hdrError))
        {
            ASTER_LOG_ERROR("Renderer", "Could not create the HDR target: %s", hdrError.c_str());
            return;
        }
        commandList->beginTrackingTextureState(m_ShadowTexture, allSubresources,
            nvrhi::ResourceStates::DepthWrite);
        const SceneSettings& settings = scene.GetSettings();

        commandList->clearTextureFloat(m_HdrTexture, allSubresources,
            nvrhi::Color(settings.ClearColor.r, settings.ClearColor.g, settings.ClearColor.b, 1.0f));
        commandList->clearDepthStencilTexture(m_Context->GetDepthTexture(), allSubresources, true, 1.0f, false,
            0);

        if (!m_WhiteTextureUploaded)
        {
            const uint32_t white = 0xFFFFFFFFu;
            commandList->writeTexture(m_WhiteTexture, 0, 0, &white, sizeof(white));
            m_WhiteTextureUploaded = true;
        }

        FrameConstants frame{};
        frame.ViewProjection = view.ViewProjection;
        frame.CameraPosition = glm::vec4(view.Position, 1.0f);
        frame.LightDirection = glm::vec4(glm::normalize(glm::vec3(-0.35f, -1.0f, -0.45f)), 0.0f);
        frame.LightColor = glm::vec4(1.0f, 0.96f, 0.88f, 0.0f) * 3.0f;
        frame.AmbientColor = glm::vec4(settings.AmbientColor * settings.AmbientIntensity, 1.0f);
        frame.Exposure = settings.Exposure;
        frame.PointLightCount = 0;
        frame.SpotLightCount = 0;

        bool hasDirectional = false;
        for (Entity entity : scene.GetEntities())
        {
            const Registry& registry = scene.GetRegistry();
            if (registry.Has<DirectionalLightComponent>(entity) && !hasDirectional)
            {
                const DirectionalLightComponent& light = registry.Get<DirectionalLightComponent>(entity);
                const glm::mat4 world = scene.GetWorldMatrix(entity);
                const glm::vec3 forward = glm::normalize(glm::mat3(world) * glm::vec3(0.0f, 0.0f, -1.0f));
                frame.LightDirection = glm::vec4(forward, 0.0f);
                frame.LightColor = glm::vec4(light.Color * light.Intensity, 0.0f);
                hasDirectional = true;
            }
            else if (registry.Has<PointLightComponent>(entity) && frame.PointLightCount < 4)
            {
                const PointLightComponent& light = registry.Get<PointLightComponent>(entity);
                const Transform world = scene.GetWorldTransform(entity);
                const int index = frame.PointLightCount++;
                frame.PointLightPosition[index] = glm::vec4(world.Translation, 1.0f);
                frame.PointLightColor[index] = glm::vec4(light.Color * light.Intensity, light.Range);
            }
            else if (registry.Has<SpotLightComponent>(entity) && frame.SpotLightCount < 4)
            {
                const SpotLightComponent& light = registry.Get<SpotLightComponent>(entity);
                const glm::mat4 world = scene.GetWorldMatrix(entity);
                const glm::vec3 direction = glm::normalize(glm::mat3(world) * glm::vec3(0.0f, 0.0f, -1.0f));
                const float inner = glm::cos(glm::radians(std::clamp(light.InnerAngle, 0.0f, 89.5f)));
                const float outer = glm::cos(glm::radians(
                    std::clamp(light.OuterAngle, light.InnerAngle + 0.5f, 89.9f)));
                const int index = frame.SpotLightCount++;
                frame.SpotLightPosition[index] = glm::vec4(glm::vec3(world[3]), light.Range);
                frame.SpotLightDirection[index] = glm::vec4(direction, outer);
                frame.SpotLightColor[index] = glm::vec4(light.Color * light.Intensity, inner);
            }
        }

        glm::vec3 lightDirection = glm::normalize(glm::vec3(frame.LightDirection));
        Aabb sceneBounds;
        bool hasBounds = false;
        for (Entity entity : scene.GetEntities())
        {
            const Registry& registry = scene.GetRegistry();
            if (!registry.Has<MeshRendererComponent>(entity))
            {
                continue;
            }
            const MeshRendererComponent& renderer = registry.Get<MeshRendererComponent>(entity);
            if (!renderer.Visible || !renderer.Mesh.IsValid() || !renderer.Mesh.Asset->Geometry)
            {
                continue;
            }
            sceneBounds.Expand(renderer.Mesh.Asset->Geometry->Bounds.Transformed(scene.GetWorldMatrix(entity)));
            hasBounds = true;
        }

        const glm::vec3 shadowCenter = hasBounds ? sceneBounds.Center() : glm::vec3(0.0f, 1.0f, 0.0f);
        const glm::vec3 shadowExtents = hasBounds ? sceneBounds.Extents() : glm::vec3(10.0f);
        const DirectionalShadowMatrix shadow = BuildDirectionalShadowMatrix(shadowCenter, shadowExtents,
            lightDirection);
        frame.LightViewProjection = shadow.ViewProjection;
        frame.ShadowParams = glm::vec4(0.0025f, 1.0f / static_cast<float>(m_ShadowMapSize), shadow.Radius,
            (hasDirectional && settings.EnableShadows) ? 1.0f : 0.0f);

        commandList->writeBuffer(m_FrameBuffer, &frame, sizeof(frame));

        const uint32_t width = view.ViewportWidth > 0 ? view.ViewportWidth : m_Context->GetWidth();
        const uint32_t height = view.ViewportHeight > 0 ? view.ViewportHeight : m_Context->GetHeight();
        nvrhi::ViewportState viewportState;
        viewportState.addViewportAndScissorRect(nvrhi::Viewport(
            static_cast<float>(view.ViewportX), static_cast<float>(view.ViewportX + width),
            static_cast<float>(view.ViewportY), static_cast<float>(view.ViewportY + height), 0.0f, 1.0f));

        if (hasDirectional && m_ShadowPipeline && m_ShadowFramebuffer)
        {
            commandList->clearDepthStencilTexture(m_ShadowTexture, allSubresources, true, 1.0f, false, 0);
            nvrhi::ViewportState shadowViewport;
            shadowViewport.addViewportAndScissorRect(nvrhi::Viewport(0.0f, static_cast<float>(m_ShadowMapSize),
                0.0f, static_cast<float>(m_ShadowMapSize), 0.0f, 1.0f));

            uint32_t shadowDrawCalls = 0;
            for (Entity entity : scene.GetEntities())
            {
                const Registry& registry = scene.GetRegistry();
                if (!registry.Has<MeshRendererComponent>(entity) || shadowDrawCalls >= m_ObjectBufferCapacity)
                {
                    continue;
                }
                const MeshRendererComponent& renderer = registry.Get<MeshRendererComponent>(entity);
                if (!renderer.Visible || !renderer.Mesh.IsValid())
                {
                    continue;
                }
                GpuMesh* gpuMesh = GetOrCreateMesh(*renderer.Mesh.Asset);
                if (gpuMesh == nullptr)
                {
                    continue;
                }

                const glm::mat4 model = scene.GetWorldMatrix(entity);
                const glm::mat3 normalMatrix3 = glm::transpose(glm::inverse(glm::mat3(model)));
                ObjectConstants object{};
                object.Model = model;
                object.NormalMatrix = glm::mat4(normalMatrix3);

                const uint64_t offset = static_cast<uint64_t>(shadowDrawCalls) * ObjectStride;
                commandList->writeBuffer(m_ShadowObjectBuffer, &object, sizeof(object), offset);

                nvrhi::BindingSetDesc bindingSetDesc;
                bindingSetDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, m_FrameBuffer));
                bindingSetDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(1, m_ShadowObjectBuffer,
                    nvrhi::BufferRange(offset, ObjectStride)));
                nvrhi::BindingSetHandle bindingSet = m_Context->GetDevice()->createBindingSet(bindingSetDesc,
                    m_ShadowBindingLayout);

                nvrhi::GraphicsState state;
                state.setPipeline(m_ShadowPipeline)
                    .setFramebuffer(m_ShadowFramebuffer)
                    .setViewport(shadowViewport)
                    .addBindingSet(bindingSet)
                    .addVertexBuffer(nvrhi::VertexBufferBinding().setBuffer(gpuMesh->VertexBuffer).setSlot(0)
                        .setOffset(0))
                    .setIndexBuffer(nvrhi::IndexBufferBinding().setBuffer(gpuMesh->IndexBuffer)
                        .setFormat(nvrhi::Format::R32_UINT).setOffset(0));
                commandList->setGraphicsState(state);
                commandList->drawIndexed(nvrhi::DrawArguments().setVertexCount(gpuMesh->IndexCount));
                ++shadowDrawCalls;
            }
        }

        for (Entity entity : scene.GetEntities())
        {
            const Registry& registry = scene.GetRegistry();
            if (!registry.Has<MeshRendererComponent>(entity))
            {
                continue;
            }
            const MeshRendererComponent& renderer = registry.Get<MeshRendererComponent>(entity);
            if (!renderer.Visible || !renderer.Mesh.IsValid())
            {
                continue;
            }

            GpuMesh* gpuMesh = GetOrCreateMesh(*renderer.Mesh.Asset);
            if (gpuMesh == nullptr)
            {
                continue;
            }

            const glm::mat4 model = scene.GetWorldMatrix(entity);
            const glm::mat3 normalMatrix3 = glm::transpose(glm::inverse(glm::mat3(model)));
            const glm::mat4 normalMatrix(normalMatrix3);
            const std::vector<Material>& materials = renderer.GetMaterials();
            const Mesh& geometry = *renderer.Mesh.Asset->Geometry;

            std::vector<SubMesh> fallbackSubMesh;
            const std::vector<SubMesh>* subMeshes = &geometry.SubMeshes;
            if (subMeshes->empty())
            {
                SubMesh full;
                full.FirstIndex = 0;
                full.IndexCount = static_cast<uint32_t>(geometry.Indices.size());
                full.MaterialIndex = 0;
                fallbackSubMesh.push_back(full);
                subMeshes = &fallbackSubMesh;
            }

            for (const SubMesh& subMesh : *subMeshes)
            {
                if (subMesh.IndexCount == 0 || m_Stats.DrawCalls >= m_ObjectBufferCapacity)
                {
                    continue;
                }

                const Material* material = nullptr;
                if (subMesh.MaterialIndex >= 0 && static_cast<size_t>(subMesh.MaterialIndex) < materials.size())
                {
                    material = &materials[static_cast<size_t>(subMesh.MaterialIndex)];
                }
                else if (!materials.empty())
                {
                    material = &materials.front();
                }

                ObjectConstants object{};
                object.Model = model;
                object.NormalMatrix = normalMatrix;
                if (material != nullptr)
                {
                    object.BaseColor = material->BaseColor;
                    object.Material = glm::vec4(material->Metallic, material->Roughness,
                        material->EmissiveStrength, material->Opacity);
                    object.Emissive = glm::vec4(material->Emissive, 0.0f);
                }
                else
                {
                    object.BaseColor = glm::vec4(0.8f, 0.8f, 0.82f, 1.0f);
                }

                const uint64_t offset = static_cast<uint64_t>(m_Stats.DrawCalls) * ObjectStride;
                commandList->writeBuffer(m_ObjectBuffer, &object, sizeof(object), offset);

                nvrhi::BindingSetDesc bindingSetDesc;
                bindingSetDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, m_FrameBuffer));
                bindingSetDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(1, m_ObjectBuffer,
                    nvrhi::BufferRange(offset, ObjectStride)));

                nvrhi::ITexture* albedoTexture = m_WhiteTexture;
                if (material != nullptr && material->AlbedoMap.IsValid())
                {
                    if (GpuTexture* texture = GetOrCreateTexture(*material->AlbedoMap.Asset))
                    {
                        albedoTexture = texture->Texture;
                    }
                }
                bindingSetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(0, albedoTexture));
                bindingSetDesc.addItem(nvrhi::BindingSetItem::Sampler(0, m_Sampler));
                bindingSetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(1, m_ShadowTexture));
                bindingSetDesc.addItem(nvrhi::BindingSetItem::Sampler(1, m_ShadowSampler));
                nvrhi::BindingSetHandle bindingSet = m_Context->GetDevice()->createBindingSet(bindingSetDesc,
                    m_BindingLayout);

                nvrhi::GraphicsState state;
                state.setPipeline(m_Pipeline)
                    .setFramebuffer(m_HdrFramebuffer)
                    .setViewport(viewportState)
                    .addBindingSet(bindingSet)
                    .addVertexBuffer(nvrhi::VertexBufferBinding().setBuffer(gpuMesh->VertexBuffer).setSlot(0)
                        .setOffset(0))
                    .setIndexBuffer(nvrhi::IndexBufferBinding().setBuffer(gpuMesh->IndexBuffer)
                        .setFormat(nvrhi::Format::R32_UINT).setOffset(0));
                commandList->setGraphicsState(state);
                commandList->drawIndexed(nvrhi::DrawArguments().setVertexCount(subMesh.IndexCount));

                ++m_Stats.DrawCalls;
                m_Stats.Triangles += subMesh.IndexCount / 3;
            }
            ++m_Stats.Meshes;
        }

        nvrhi::ViewportState postViewport;
        postViewport.addViewportAndScissorRect(nvrhi::Viewport(0.0f, static_cast<float>(m_Context->GetWidth()),
            0.0f, static_cast<float>(m_Context->GetHeight()), 0.0f, 1.0f));
        nvrhi::BindingSetDesc postBindingDesc;
        postBindingDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, m_FrameBuffer));
        postBindingDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(2, m_HdrTexture));
        postBindingDesc.addItem(nvrhi::BindingSetItem::Sampler(2, m_Sampler));
        nvrhi::BindingSetHandle postBindingSet = m_Context->GetDevice()->createBindingSet(postBindingDesc,
            m_PostBindingLayout);
        nvrhi::GraphicsState postState;
        postState.setPipeline(m_PostPipeline)
            .setFramebuffer(m_Context->GetCurrentFramebuffer())
            .setViewport(postViewport)
            .addBindingSet(postBindingSet);
        commandList->setGraphicsState(postState);
        commandList->draw(nvrhi::DrawArguments().setVertexCount(3));
    }

    std::vector<uint8_t> D3D12Renderer::ReadFile(const std::string& path)
    {
        std::ifstream stream(path, std::ios::binary | std::ios::ate);
        if (!stream)
        {
            ASTER_LOG_ERROR("Renderer", "Could not open shader '%s'", path.c_str());
            return {};
        }
        const std::streamsize size = stream.tellg();
        stream.seekg(0);
        std::vector<uint8_t> data(static_cast<size_t>(size));
        if (size > 0)
        {
            stream.read(reinterpret_cast<char*>(data.data()), size);
        }
        return data;
    }
}
