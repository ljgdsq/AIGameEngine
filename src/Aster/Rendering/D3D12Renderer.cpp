#include "Aster/Rendering/D3D12Renderer.h"

#include "Aster/Core/Log.h"

#include <glm/gtc/matrix_inverse.hpp>

#include <algorithm>
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
        };

        struct ObjectConstants
        {
            glm::mat4 Model{ 1.0f };
            glm::mat4 NormalMatrix{ 1.0f };
            glm::vec4 BaseColor{ 1.0f };
            glm::vec4 Material{ 0.0f, 0.5f, 0.0f, 1.0f };
            glm::vec4 Emissive{ 0.0f };
        };

        static_assert(sizeof(FrameConstants) == 464, "Frame constant layout must match Pbr.hlsl");
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
        if (!m_FrameBuffer || !m_ObjectBuffer)
        {
            if (error != nullptr)
            {
                *error = "Failed to create renderer constant buffers";
            }
            return false;
        }

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
        if (!m_WhiteTexture || !m_Sampler)
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
        if (vertexShader.empty() || pixelShader.empty())
        {
            if (error != nullptr)
            {
                *error = "Could not read compiled PBR shaders. Expected Pbr.vs.dxil and Pbr.ps.dxil in " +
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
        if (!vs || !ps)
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
        m_BindingLayout = device->createBindingLayout(layoutDesc);

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
        framebufferInfo.addColorFormat(nvrhi::Format::RGBA8_UNORM);
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
        return true;
    }

    void D3D12Renderer::Shutdown()
    {
        m_Meshes.clear();
        m_Textures.clear();
        m_WhiteTexture = nullptr;
        m_Sampler = nullptr;
        m_Pipeline = nullptr;
        m_BindingLayout = nullptr;
        m_FrameBuffer = nullptr;
        m_ObjectBuffer = nullptr;
        m_InputLayout = nullptr;
        m_Context = nullptr;
        m_Stats = {};
        m_WhiteTextureUploaded = false;
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
        const SceneSettings& settings = scene.GetSettings();

        commandList->clearTextureFloat(m_Context->GetCurrentBackbuffer(), allSubresources,
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

        commandList->writeBuffer(m_FrameBuffer, &frame, sizeof(frame));

        const uint32_t width = view.ViewportWidth > 0 ? view.ViewportWidth : m_Context->GetWidth();
        const uint32_t height = view.ViewportHeight > 0 ? view.ViewportHeight : m_Context->GetHeight();
        nvrhi::ViewportState viewportState;
        viewportState.addViewportAndScissorRect(nvrhi::Viewport(
            static_cast<float>(view.ViewportX), static_cast<float>(view.ViewportX + width),
            static_cast<float>(view.ViewportY), static_cast<float>(view.ViewportY + height), 0.0f, 1.0f));

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
                nvrhi::BindingSetHandle bindingSet = m_Context->GetDevice()->createBindingSet(bindingSetDesc,
                    m_BindingLayout);

                nvrhi::GraphicsState state;
                state.setPipeline(m_Pipeline)
                    .setFramebuffer(m_Context->GetCurrentFramebuffer())
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
