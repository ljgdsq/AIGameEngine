#include "Aster/Assets/ObjMeshLoader.h"

#include "Aster/Core/FileSystem.h"

#include <charconv>
#include <map>
#include <sstream>
#include <string_view>
#include <tuple>

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

        bool ParseFloat(std::string_view token, float& value)
        {
            const char* begin = token.data();
            const char* end = token.data() + token.size();
            const auto result = std::from_chars(begin, end, value);
            return result.ec == std::errc() && result.ptr == end;
        }

        bool ParseInt(std::string_view token, int& value)
        {
            const char* begin = token.data();
            const char* end = token.data() + token.size();
            const auto result = std::from_chars(begin, end, value);
            return result.ec == std::errc() && result.ptr == end;
        }

        int ResolveIndex(int index, size_t count)
        {
            if (index > 0)
            {
                return index - 1;
            }
            if (index < 0)
            {
                return static_cast<int>(count) + index;
            }
            return -1;
        }

        struct FaceVertex
        {
            int Position = -1;
            int UV = -1;
            int Normal = -1;
        };

        bool ParseFaceVertex(std::string_view token, FaceVertex& out)
        {
            const size_t firstSlash = token.find('/');
            if (firstSlash == std::string_view::npos)
            {
                return ParseInt(token, out.Position);
            }

            const size_t secondSlash = token.find('/', firstSlash + 1);
            if (!ParseInt(token.substr(0, firstSlash), out.Position))
            {
                return false;
            }
            if (secondSlash == std::string_view::npos)
            {
                return ParseInt(token.substr(firstSlash + 1), out.UV);
            }
            if (firstSlash + 1 < secondSlash && !ParseInt(token.substr(firstSlash + 1, secondSlash - firstSlash - 1), out.UV))
            {
                return false;
            }
            if (secondSlash + 1 < token.size())
            {
                return ParseInt(token.substr(secondSlash + 1), out.Normal);
            }
            return true;
        }
    }

    std::shared_ptr<MeshAsset> ObjMeshLoader::Load(const std::string& path, std::string* error)
    {
        const std::optional<std::string> contents = FileSystem::ReadTextFile(path);
        if (!contents.has_value())
        {
            SetError(error, "Could not read OBJ file: " + path);
            return nullptr;
        }

        return Parse(*contents, FileSystem::GetStem(path), error);
    }

    std::shared_ptr<MeshAsset> ObjMeshLoader::Parse(std::string_view contents, const std::string& name,
        std::string* error)
    {
        std::vector<glm::vec3> positions;
        std::vector<glm::vec2> uvs;
        std::vector<glm::vec3> normals;
        std::vector<Vertex> vertices;
        std::vector<uint32> indices;
        std::map<std::tuple<int, int, int>, uint32> uniqueVertices;

        std::istringstream stream{ std::string(contents) };
        std::string line;
        size_t lineNumber = 0;
        while (std::getline(stream, line))
        {
            ++lineNumber;
            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }
            if (line.empty() || line.front() == '#')
            {
                continue;
            }

            std::istringstream lineStream(line);
            std::string prefix;
            lineStream >> prefix;
            if (prefix == "v")
            {
                glm::vec3 position(0.0f);
                if (!(lineStream >> position.x >> position.y >> position.z))
                {
                    SetError(error, "Invalid vertex on line " + std::to_string(lineNumber));
                    return nullptr;
                }
                positions.push_back(position);
            }
            else if (prefix == "vt")
            {
                glm::vec2 uv(0.0f);
                if (!(lineStream >> uv.x >> uv.y))
                {
                    SetError(error, "Invalid texture coordinate on line " + std::to_string(lineNumber));
                    return nullptr;
                }
                uvs.push_back(uv);
            }
            else if (prefix == "vn")
            {
                glm::vec3 normal(0.0f);
                if (!(lineStream >> normal.x >> normal.y >> normal.z))
                {
                    SetError(error, "Invalid normal on line " + std::to_string(lineNumber));
                    return nullptr;
                }
                normals.push_back(normal);
            }
            else if (prefix == "f")
            {
                std::vector<FaceVertex> face;
                std::string token;
                while (lineStream >> token)
                {
                    FaceVertex faceVertex;
                    if (!ParseFaceVertex(token, faceVertex))
                    {
                        SetError(error, "Invalid face vertex on line " + std::to_string(lineNumber));
                        return nullptr;
                    }
                    face.push_back(faceVertex);
                }

                if (face.size() < 3)
                {
                    SetError(error, "Face with fewer than three vertices on line " + std::to_string(lineNumber));
                    return nullptr;
                }

                for (size_t corner = 0; corner < face.size(); ++corner)
                {
                    FaceVertex& faceVertex = face[corner];
                    faceVertex.Position = ResolveIndex(faceVertex.Position, positions.size());
                    faceVertex.UV = faceVertex.UV != 0 ? ResolveIndex(faceVertex.UV, uvs.size()) : -1;
                    faceVertex.Normal = faceVertex.Normal != 0 ? ResolveIndex(faceVertex.Normal, normals.size()) : -1;
                    if (faceVertex.Position < 0 || static_cast<size_t>(faceVertex.Position) >= positions.size() ||
                        (faceVertex.UV >= 0 && static_cast<size_t>(faceVertex.UV) >= uvs.size()) ||
                        (faceVertex.Normal >= 0 && static_cast<size_t>(faceVertex.Normal) >= normals.size()))
                    {
                        SetError(error, "Face references an out of range vertex on line " + std::to_string(lineNumber));
                        return nullptr;
                    }
                }

                // Fan triangulation of convex polygons.
                for (size_t corner = 1; corner + 1 < face.size(); ++corner)
                {
                    for (const FaceVertex& faceVertex : { face[0], face[corner], face[corner + 1] })
                    {
                        const auto key = std::make_tuple(faceVertex.Position, faceVertex.UV, faceVertex.Normal);
                        auto iterator = uniqueVertices.find(key);
                        if (iterator == uniqueVertices.end())
                        {
                            Vertex vertex;
                            vertex.Position = positions[static_cast<size_t>(faceVertex.Position)];
                            if (faceVertex.UV >= 0)
                            {
                                vertex.UV = uvs[static_cast<size_t>(faceVertex.UV)];
                                vertex.UV.y = 1.0f - vertex.UV.y;
                            }
                            if (faceVertex.Normal >= 0)
                            {
                                vertex.Normal = SafeNormalize(normals[static_cast<size_t>(faceVertex.Normal)]);
                            }
                            const uint32 index = static_cast<uint32>(vertices.size());
                            vertices.push_back(vertex);
                            iterator = uniqueVertices.emplace(key, index).first;
                        }
                        indices.push_back(iterator->second);
                    }
                }
            }
        }

        if (positions.empty() || indices.empty())
        {
            SetError(error, "OBJ file contains no geometry");
            return nullptr;
        }

        bool needsNormals = false;
        for (const Vertex& vertex : vertices)
        {
            if (glm::dot(vertex.Normal, vertex.Normal) < 0.5f)
            {
                needsNormals = true;
                break;
            }
        }

        auto mesh = std::make_shared<Mesh>();
        mesh->Name = name;
        mesh->Vertices = std::move(vertices);
        mesh->Indices = std::move(indices);
        if (needsNormals)
        {
            mesh->RecalculateNormals();
        }
        mesh->GenerateTangents();

        SubMesh subMesh;
        subMesh.FirstIndex = 0;
        subMesh.IndexCount = static_cast<uint32>(mesh->Indices.size());
        subMesh.MaterialIndex = 0;
        mesh->SubMeshes.push_back(subMesh);
        mesh->RecalculateBounds();

        auto asset = std::make_shared<MeshAsset>();
        asset->Name = name;
        asset->Geometry = std::move(mesh);
        Material material;
        material.Name = name + " Material";
        asset->Materials.push_back(material);
        return asset;
    }
}
