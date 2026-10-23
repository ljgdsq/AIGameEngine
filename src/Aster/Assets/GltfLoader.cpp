#include "Aster/Assets/GltfLoader.h"

#include "Aster/Assets/ImageLoader.h"
#include "Aster/Core/FileSystem.h"
#include "Aster/Core/Log.h"

#include <cgltf.h>

#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace Aster
{
    namespace
    {
        void SetError(GltfLoadResult& result, std::string message)
        {
            result.Error = std::move(message);
        }

        const char* ResultString(cgltf_result result)
        {
            switch (result)
            {
                case cgltf_result_success: return "success";
                case cgltf_result_data_too_short: return "data too short";
                case cgltf_result_unknown_format: return "unknown format";
                case cgltf_result_invalid_json: return "invalid JSON";
                case cgltf_result_invalid_gltf: return "invalid glTF";
                case cgltf_result_invalid_options: return "invalid options";
                case cgltf_result_file_not_found: return "file not found";
                case cgltf_result_io_error: return "I/O error";
                case cgltf_result_out_of_memory: return "out of memory";
                case cgltf_result_legacy_gltf: return "legacy glTF";
                default: return "unknown cgltf error";
            }
        }

        int Base64Value(char character)
        {
            if (character >= 'A' && character <= 'Z') return character - 'A';
            if (character >= 'a' && character <= 'z') return character - 'a' + 26;
            if (character >= '0' && character <= '9') return character - '0' + 52;
            if (character == '+') return 62;
            if (character == '/') return 63;
            return -1;
        }

        std::vector<uint8> DecodeBase64(const char* text, size_t size)
        {
            std::vector<uint8> output;
            output.reserve(size / 4 * 3);
            uint32 accumulator = 0;
            int bits = 0;
            for (size_t index = 0; index < size; ++index)
            {
                const char character = text[index];
                if (character == '=')
                {
                    break;
                }
                const int value = Base64Value(character);
                if (value < 0)
                {
                    if (character == '\n' || character == '\r' || character == ' ' || character == '\t')
                    {
                        continue;
                    }
                    return {};
                }
                accumulator = (accumulator << 6U) | static_cast<uint32>(value);
                bits += 6;
                if (bits >= 8)
                {
                    bits -= 8;
                    output.push_back(static_cast<uint8>((accumulator >> static_cast<uint32>(bits)) & 0xFFU));
                }
            }
            return output;
        }

        std::string TextureKey(const std::string& source, size_t index)
        {
            return source + "#tex" + std::to_string(index);
        }

        std::string MeshKey(const std::string& source, size_t index)
        {
            return source + "#mesh" + std::to_string(index);
        }

        bool DecodeImage(const std::filesystem::path& baseDirectory, const cgltf_image& image,
            std::vector<uint8>& encoded, std::string& error)
        {
            if (image.buffer_view != nullptr)
            {
                const uint8* data = cgltf_buffer_view_data(image.buffer_view);
                if (data == nullptr)
                {
                    error = "Image buffer view has no data";
                    return false;
                }
                encoded.assign(data, data + image.buffer_view->size);
                return true;
            }

            if (image.uri == nullptr)
            {
                error = "Image has neither a buffer view nor a URI";
                return false;
            }

            std::string uri(image.uri);
            if (uri.rfind("data:", 0) == 0)
            {
                const size_t comma = uri.find(',');
                if (comma == std::string::npos)
                {
                    error = "Malformed image data URI";
                    return false;
                }
                const std::string header = uri.substr(0, comma);
                if (header.find(";base64") == std::string::npos)
                {
                    error = "Only base64 image data URIs are supported";
                    return false;
                }
                encoded = DecodeBase64(uri.c_str() + comma + 1, uri.size() - comma - 1);
                if (encoded.empty())
                {
                    error = "Failed to decode image data URI";
                    return false;
                }
                return true;
            }

            const std::filesystem::path imagePath = FileSystem::Resolve(baseDirectory, std::filesystem::path(uri));
            const std::optional<std::vector<uint8>> bytes = FileSystem::ReadBinaryFile(imagePath);
            if (!bytes.has_value())
            {
                error = "Could not read image: " + FileSystem::Normalize(imagePath);
                return false;
            }
            encoded = *bytes;
            return true;
        }

        TextureWrap ToWrap(cgltf_wrap_mode mode)
        {
            switch (mode)
            {
                case cgltf_wrap_mode_clamp_to_edge: return TextureWrap::Clamp;
                case cgltf_wrap_mode_mirrored_repeat: return TextureWrap::Mirror;
                default: return TextureWrap::Repeat;
            }
        }

        TextureFilter ToFilter(cgltf_filter_type filter)
        {
            return filter == cgltf_filter_type_nearest || filter == cgltf_filter_type_nearest_mipmap_nearest ||
                           filter == cgltf_filter_type_nearest_mipmap_linear
                ? TextureFilter::Nearest
                : TextureFilter::Linear;
        }

        std::shared_ptr<Texture> LoadTexture(const std::string& source, const std::filesystem::path& baseDirectory,
            const cgltf_data& data, size_t index, std::string& error)
        {
            const cgltf_texture& texture = data.textures[index];
            if (texture.image == nullptr)
            {
                error = "Texture has no image";
                return nullptr;
            }

            std::vector<uint8> encoded;
            if (!DecodeImage(baseDirectory, *texture.image, encoded, error))
            {
                return nullptr;
            }

            const std::string uri = texture.image->uri != nullptr ? texture.image->uri : "";
            const bool isHdr = uri.size() >= 4 &&
                (uri.compare(uri.size() - 4, 4, ".hdr") == 0 ||
                    (texture.image->mime_type != nullptr &&
                        std::string_view(texture.image->mime_type).find("radiance") != std::string_view::npos));
            const std::optional<Image> image = ImageLoader::LoadFromMemory(encoded.data(), encoded.size(), isHdr, false,
                &error);
            if (!image.has_value())
            {
                return nullptr;
            }

            auto result = std::make_shared<Texture>();
            result->Name = texture.image->name != nullptr ? texture.image->name : "glTF Texture " + std::to_string(index);
            result->SourcePath = TextureKey(source, index);
            result->Pixels = *image;
            result->SRGB = !isHdr;
            if (texture.sampler != nullptr)
            {
                result->Wrap = ToWrap(texture.sampler->wrap_s);
                if (ToWrap(texture.sampler->wrap_s) != ToWrap(texture.sampler->wrap_t))
                {
                    // The engine texture only stores one wrap mode; clamp is the safer fallback.
                    if (ToWrap(texture.sampler->wrap_s) == TextureWrap::Clamp ||
                        ToWrap(texture.sampler->wrap_t) == TextureWrap::Clamp)
                    {
                        result->Wrap = TextureWrap::Clamp;
                    }
                }
                result->Filter = ToFilter(texture.sampler->min_filter);
                if (texture.sampler->mag_filter != cgltf_filter_type_undefined &&
                    ToFilter(texture.sampler->mag_filter) == TextureFilter::Nearest)
                {
                    result->Filter = TextureFilter::Nearest;
                }
            }
            return result;
        }

        AssetRef<Texture> MakeTextureRef(const cgltf_texture_view& view, const cgltf_data& data,
            const GltfLoadResult& result)
        {
            if (!view.texture)
            {
                return {};
            }
            const size_t index = static_cast<size_t>(view.texture - data.textures);
            const std::string key = TextureKey(result.Model->SourcePath, index);
            const auto iterator = result.Textures.find(key);
            if (iterator == result.Textures.end())
            {
                return {};
            }
            return AssetRef<Texture>(iterator->second, key);
        }

        Material ConvertMaterial(const cgltf_material& material, const cgltf_data& data, const GltfLoadResult& result)
        {
            Material output;
            output.Name = material.name != nullptr ? material.name : "glTF Material";

            if (material.has_pbr_metallic_roughness)
            {
                const auto& pbr = material.pbr_metallic_roughness;
                output.BaseColor = glm::vec4(pbr.base_color_factor[0], pbr.base_color_factor[1],
                    pbr.base_color_factor[2], pbr.base_color_factor[3]);
                output.Metallic = pbr.metallic_factor;
                output.Roughness = pbr.roughness_factor;
                output.AlbedoMap = MakeTextureRef(pbr.base_color_texture, data, result);
                output.MetallicRoughnessMap = MakeTextureRef(pbr.metallic_roughness_texture, data, result);
            }

            output.Emissive = glm::vec3(material.emissive_factor[0], material.emissive_factor[1],
                material.emissive_factor[2]);
            if (material.has_emissive_strength)
            {
                output.EmissiveStrength = material.emissive_strength.emissive_strength;
            }
            output.EmissiveMap = MakeTextureRef(material.emissive_texture, data, result);
            output.NormalMap = MakeTextureRef(material.normal_texture, data, result);
            output.OcclusionMap = MakeTextureRef(material.occlusion_texture, data, result);
            output.DoubleSided = material.double_sided != 0;
            output.AlphaCutoff = material.alpha_cutoff > 0.0f ? material.alpha_cutoff : 0.5f;

            switch (material.alpha_mode)
            {
                case cgltf_alpha_mode_mask:
                    output.AlphaMask = true;
                    break;
                case cgltf_alpha_mode_blend:
                    output.AlphaBlend = true;
                    break;
                default:
                    break;
            }
            return output;
        }

        std::shared_ptr<MeshAsset> BuildMesh(const std::string& source, const cgltf_data& data, size_t meshIndex,
            const std::unordered_map<const cgltf_material*, size_t>& materialIndices, std::string& error)
        {
            const cgltf_mesh& gltfMesh = data.meshes[meshIndex];
            auto mesh = std::make_shared<Mesh>();
            mesh->Name = gltfMesh.name != nullptr ? gltfMesh.name : "Mesh " + std::to_string(meshIndex);

            auto asset = std::make_shared<MeshAsset>();
            asset->Name = mesh->Name;
            asset->SourcePath = source;
            asset->Geometry = mesh;

            bool hasNormals = true;
            bool hasTangents = true;

            for (size_t primitiveIndex = 0; primitiveIndex < gltfMesh.primitives_count; ++primitiveIndex)
            {
                const cgltf_primitive& primitive = gltfMesh.primitives[primitiveIndex];
                if (primitive.type != cgltf_primitive_type_triangles)
                {
                    ASTER_LOG_WARN("GltfLoader", "Skipping non-triangle primitive %zu in '%s'", primitiveIndex,
                        source.c_str());
                    continue;
                }

                const cgltf_accessor* positionAccessor = nullptr;
                const cgltf_accessor* normalAccessor = nullptr;
                const cgltf_accessor* uvAccessor = nullptr;
                const cgltf_accessor* tangentAccessor = nullptr;
                for (size_t attributeIndex = 0; attributeIndex < primitive.attributes_count; ++attributeIndex)
                {
                    const cgltf_attribute& attribute = primitive.attributes[attributeIndex];
                    if (attribute.type == cgltf_attribute_type_position) positionAccessor = attribute.data;
                    else if (attribute.type == cgltf_attribute_type_normal) normalAccessor = attribute.data;
                    else if (attribute.type == cgltf_attribute_type_texcoord && attribute.index == 0) uvAccessor = attribute.data;
                    else if (attribute.type == cgltf_attribute_type_tangent) tangentAccessor = attribute.data;
                }

                if (positionAccessor == nullptr || positionAccessor->count == 0)
                {
                    continue;
                }

                const cgltf_size vertexCount = positionAccessor->count;
                const uint32 firstVertex = static_cast<uint32>(mesh->Vertices.size());
                mesh->Vertices.resize(mesh->Vertices.size() + vertexCount);

                std::vector<float> scratch(static_cast<size_t>(cgltf_accessor_unpack_floats(positionAccessor, nullptr, 0)));
                if (cgltf_accessor_unpack_floats(positionAccessor, scratch.data(), scratch.size()) == 0)
                {
                    error = "Failed to read position accessor";
                    return nullptr;
                }
                for (cgltf_size vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
                {
                    Vertex& vertex = mesh->Vertices[firstVertex + vertexIndex];
                    vertex.Position = glm::vec3(scratch[vertexIndex * 3], scratch[vertexIndex * 3 + 1],
                        scratch[vertexIndex * 3 + 2]);
                }

                if (normalAccessor != nullptr)
                {
                    std::vector<float> normals(static_cast<size_t>(cgltf_accessor_unpack_floats(normalAccessor, nullptr, 0)));
                    if (cgltf_accessor_unpack_floats(normalAccessor, normals.data(), normals.size()) != 0 &&
                        normals.size() >= vertexCount * 3)
                    {
                        for (cgltf_size vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
                        {
                            mesh->Vertices[firstVertex + vertexIndex].Normal = glm::vec3(normals[vertexIndex * 3],
                                normals[vertexIndex * 3 + 1], normals[vertexIndex * 3 + 2]);
                        }
                    }
                }
                else
                {
                    hasNormals = false;
                }

                if (uvAccessor != nullptr)
                {
                    std::vector<float> uvs(static_cast<size_t>(cgltf_accessor_unpack_floats(uvAccessor, nullptr, 0)));
                    if (cgltf_accessor_unpack_floats(uvAccessor, uvs.data(), uvs.size()) != 0 &&
                        uvs.size() >= vertexCount * 2)
                    {
                        for (cgltf_size vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
                        {
                            mesh->Vertices[firstVertex + vertexIndex].UV = glm::vec2(uvs[vertexIndex * 2],
                                uvs[vertexIndex * 2 + 1]);
                        }
                    }
                }

                if (tangentAccessor != nullptr)
                {
                    std::vector<float> tangents(static_cast<size_t>(cgltf_accessor_unpack_floats(tangentAccessor, nullptr, 0)));
                    if (cgltf_accessor_unpack_floats(tangentAccessor, tangents.data(), tangents.size()) != 0 &&
                        tangents.size() >= vertexCount * 4)
                    {
                        for (cgltf_size vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
                        {
                            mesh->Vertices[firstVertex + vertexIndex].Tangent = glm::vec4(
                                tangents[vertexIndex * 4], tangents[vertexIndex * 4 + 1],
                                tangents[vertexIndex * 4 + 2], tangents[vertexIndex * 4 + 3]);
                        }
                    }
                }
                else
                {
                    hasTangents = false;
                }

                SubMesh subMesh;
                subMesh.FirstIndex = static_cast<uint32>(mesh->Indices.size());
                subMesh.MaterialIndex = 0;
                if (primitive.material != nullptr)
                {
                    const auto materialIterator = materialIndices.find(primitive.material);
                    if (materialIterator != materialIndices.end())
                    {
                        subMesh.MaterialIndex = static_cast<int32>(materialIterator->second);
                    }
                }

                if (primitive.indices != nullptr)
                {
                    const size_t indexCount = primitive.indices->count;
                    std::vector<uint32> indices(indexCount);
                    if (cgltf_accessor_unpack_indices(primitive.indices, indices.data(), sizeof(uint32), indexCount) == 0)
                    {
                        error = "Failed to read index accessor";
                        return nullptr;
                    }
                    mesh->Indices.insert(mesh->Indices.end(), indices.begin(), indices.end());
                }
                else
                {
                    for (cgltf_size vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
                    {
                        mesh->Indices.push_back(firstVertex + static_cast<uint32>(vertexIndex));
                    }
                }

                subMesh.IndexCount = static_cast<uint32>(mesh->Indices.size()) - subMesh.FirstIndex;
                mesh->SubMeshes.push_back(subMesh);
            }

            if (mesh->Vertices.empty() || mesh->Indices.empty())
            {
                error = "glTF mesh contains no drawable triangles";
                return nullptr;
            }

            if (!hasNormals)
            {
                mesh->RecalculateNormals();
            }
            if (!hasTangents)
            {
                mesh->GenerateTangents();
            }
            mesh->RecalculateBounds();
            return asset;
        }

        void BuildNode(const cgltf_node& gltfNode, const cgltf_data& data, const GltfLoadResult& result,
            ModelNode& output)
        {
            output.Name = gltfNode.name != nullptr ? gltfNode.name : "Node";

            cgltf_float transform[16];
            cgltf_node_transform_local(&gltfNode, transform);
            const glm::mat4 local = glm::make_mat4(transform);
            Transform decomposed;
            if (DecomposeTransform(local, decomposed))
            {
                output.LocalTransform = decomposed;
            }

            if (gltfNode.mesh != nullptr)
            {
                const size_t meshIndex = static_cast<size_t>(gltfNode.mesh - data.meshes);
                const std::string key = MeshKey(result.Model->SourcePath, meshIndex);
                const auto meshIterator = result.Meshes.find(key);
                if (meshIterator != result.Meshes.end())
                {
                    output.Mesh = meshIterator->second;
                }
            }

            for (size_t childIndex = 0; childIndex < gltfNode.children_count; ++childIndex)
            {
                ModelNode child;
                BuildNode(*gltfNode.children[childIndex], data, result, child);
                output.Children.push_back(std::move(child));
            }
        }
    }

    bool GltfLoader::Load(const std::string& path, const std::string& keyPath, GltfLoadResult& result)
    {
        result = GltfLoadResult{};
        if (!FileSystem::Exists(path))
        {
            SetError(result, "glTF file does not exist: " + path);
            return false;
        }

        cgltf_options options{};
        cgltf_data* data = nullptr;
        cgltf_result parseResult = cgltf_parse_file(&options, path.c_str(), &data);
        if (parseResult != cgltf_result_success)
        {
            SetError(result, std::string("Failed to parse glTF: ") + ResultString(parseResult));
            return false;
        }

        struct DataGuard
        {
            cgltf_data* Data;
            ~DataGuard() { cgltf_free(Data); }
        } guard{ data };

        const std::string source = FileSystem::Normalize(keyPath);
        const std::filesystem::path baseDirectory = FileSystem::GetDirectory(path);
        parseResult = cgltf_load_buffers(&options, data, path.c_str());
        if (parseResult != cgltf_result_success)
        {
            SetError(result, std::string("Failed to load glTF buffers: ") + ResultString(parseResult));
            return false;
        }

        parseResult = cgltf_validate(data);
        if (parseResult != cgltf_result_success)
        {
            SetError(result, std::string("Invalid glTF: ") + ResultString(parseResult));
            return false;
        }

        auto model = std::make_shared<ModelAsset>();
        model->Name = FileSystem::GetStem(path);
        model->SourcePath = source;
        result.Model = model;

        // Textures must be loaded before materials can resolve their texture references.
        for (size_t index = 0; index < data->textures_count; ++index)
        {
            std::string error;
            const std::shared_ptr<Texture> texture = LoadTexture(source, baseDirectory, *data, index, error);
            if (texture != nullptr)
            {
                result.Textures.emplace(TextureKey(source, index), texture);
            }
            else
            {
                ASTER_LOG_WARN("GltfLoader", "Skipping texture %zu in '%s': %s", index, path.c_str(), error.c_str());
            }
        }

        std::unordered_map<const cgltf_material*, size_t> materialIndices;
        std::vector<Material> materials;
        for (size_t index = 0; index < data->materials_count; ++index)
        {
            materialIndices.emplace(&data->materials[index], materials.size());
            materials.push_back(ConvertMaterial(data->materials[index], *data, result));
        }

        for (size_t meshIndex = 0; meshIndex < data->meshes_count; ++meshIndex)
        {
            std::string error;
            const std::shared_ptr<MeshAsset> mesh = BuildMesh(source, *data, meshIndex, materialIndices, error);
            if (mesh == nullptr)
            {
                ASTER_LOG_WARN("GltfLoader", "Skipping mesh %zu in '%s': %s", meshIndex, path.c_str(), error.c_str());
                continue;
            }
            mesh->Materials = materials;
            if (mesh->Materials.empty())
            {
                mesh->Materials.push_back(Material{});
            }
            const std::string meshKey = MeshKey(source, meshIndex);
            mesh->SourcePath = meshKey;
            result.Meshes.emplace(meshKey, mesh);
        }

        if (data->scene != nullptr)
        {
            for (size_t index = 0; index < data->scene->nodes_count; ++index)
            {
                ModelNode root;
                BuildNode(*data->scene->nodes[index], *data, result, root);
                model->Roots.push_back(std::move(root));
            }
        }
        else
        {
            for (size_t index = 0; index < data->nodes_count; ++index)
            {
                if (data->nodes[index].parent == nullptr)
                {
                    ModelNode root;
                    BuildNode(data->nodes[index], *data, result, root);
                    model->Roots.push_back(std::move(root));
                }
            }
        }

        if (model->Roots.empty() && result.Meshes.empty())
        {
            SetError(result, "glTF file contains no scene nodes or meshes");
            return false;
        }

        return true;
    }
}
