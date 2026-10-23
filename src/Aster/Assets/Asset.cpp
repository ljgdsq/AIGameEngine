#include "Aster/Assets/Asset.h"

#include <algorithm>
#include <cmath>

namespace Aster
{
    namespace
    {
        uint32 MipSize(uint32 base, uint32 mip)
        {
            return std::max(1U, base >> mip);
        }
    }

    void Mesh::RecalculateBounds()
    {
        Bounds = Aabb();
        for (const Vertex& vertex : Vertices)
        {
            Bounds.Expand(vertex.Position);
        }

        for (SubMesh& subMesh : SubMeshes)
        {
            subMesh.Bounds = Aabb();
            const uint32 end = std::min(subMesh.FirstIndex + subMesh.IndexCount, static_cast<uint32>(Indices.size()));
            for (uint32 index = subMesh.FirstIndex; index < end; ++index)
            {
                const uint32 vertexIndex = Indices[index];
                if (vertexIndex < Vertices.size())
                {
                    subMesh.Bounds.Expand(Vertices[vertexIndex].Position);
                }
            }
        }
    }

    void Mesh::RecalculateNormals()
    {
        for (Vertex& vertex : Vertices)
        {
            vertex.Normal = glm::vec3(0.0f);
        }

        for (size_t index = 0; index + 2 < Indices.size(); index += 3)
        {
            const uint32 i0 = Indices[index];
            const uint32 i1 = Indices[index + 1];
            const uint32 i2 = Indices[index + 2];
            if (i0 >= Vertices.size() || i1 >= Vertices.size() || i2 >= Vertices.size())
            {
                continue;
            }

            const glm::vec3& p0 = Vertices[i0].Position;
            const glm::vec3& p1 = Vertices[i1].Position;
            const glm::vec3& p2 = Vertices[i2].Position;
            const glm::vec3 normal = glm::cross(p1 - p0, p2 - p0);
            Vertices[i0].Normal += normal;
            Vertices[i1].Normal += normal;
            Vertices[i2].Normal += normal;
        }

        for (Vertex& vertex : Vertices)
        {
            vertex.Normal = SafeNormalize(vertex.Normal, glm::vec3(0.0f, 1.0f, 0.0f));
        }
    }

    void Mesh::GenerateTangents()
    {
        std::vector<glm::vec3> tangentAccumulator(Vertices.size(), glm::vec3(0.0f));
        std::vector<glm::vec3> bitangentAccumulator(Vertices.size(), glm::vec3(0.0f));

        for (size_t index = 0; index + 2 < Indices.size(); index += 3)
        {
            const uint32 i0 = Indices[index];
            const uint32 i1 = Indices[index + 1];
            const uint32 i2 = Indices[index + 2];
            if (i0 >= Vertices.size() || i1 >= Vertices.size() || i2 >= Vertices.size())
            {
                continue;
            }

            const glm::vec3 edge1 = Vertices[i1].Position - Vertices[i0].Position;
            const glm::vec3 edge2 = Vertices[i2].Position - Vertices[i0].Position;
            const glm::vec2 deltaUV1 = Vertices[i1].UV - Vertices[i0].UV;
            const glm::vec2 deltaUV2 = Vertices[i2].UV - Vertices[i0].UV;

            const float determinant = deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y;
            if (std::abs(determinant) < Epsilon)
            {
                continue;
            }

            const float inverse = 1.0f / determinant;
            const glm::vec3 tangent = (edge1 * deltaUV2.y - edge2 * deltaUV1.y) * inverse;
            const glm::vec3 bitangent = (edge2 * deltaUV1.x - edge1 * deltaUV2.x) * inverse;

            for (uint32 vertexIndex : { i0, i1, i2 })
            {
                tangentAccumulator[vertexIndex] += tangent;
                bitangentAccumulator[vertexIndex] += bitangent;
            }
        }

        for (size_t index = 0; index < Vertices.size(); ++index)
        {
            Vertex& vertex = Vertices[index];
            const glm::vec3 normal = SafeNormalize(vertex.Normal, glm::vec3(0.0f, 1.0f, 0.0f));
            glm::vec3 tangent = tangentAccumulator[index];
            tangent -= normal * glm::dot(normal, tangent);
            tangent = SafeNormalize(tangent, glm::vec3(1.0f, 0.0f, 0.0f));

            const float handedness = glm::dot(glm::cross(normal, tangent), bitangentAccumulator[index]) < 0.0f
                ? -1.0f
                : 1.0f;
            vertex.Tangent = glm::vec4(tangent, handedness);
        }
    }

    size_t CubemapImage::GetFaceMipPixelCount(uint32 mip) const
    {
        const uint32 size = GetMipSize(mip);
        return static_cast<size_t>(size) * size;
    }

    uint32 CubemapImage::GetMipSize(uint32 mip) const
    {
        return std::max(1U, FaceSize >> mip);
    }

    size_t CubemapImage::GetFaceMipOffset(uint32 face, uint32 mip) const
    {
        size_t offset = 0;
        for (uint32 currentFace = 0; currentFace < 6; ++currentFace)
        {
            for (uint32 currentMip = 0; currentMip < MipCount; ++currentMip)
            {
                if (currentFace == face && currentMip == mip)
                {
                    return offset;
                }
                offset += GetFaceMipPixelCount(currentMip);
            }
        }
        return offset;
    }

    const glm::vec4* CubemapImage::GetFaceMipData(uint32 face, uint32 mip) const
    {
        if (face >= 6 || mip >= MipCount || Pixels.empty())
        {
            return nullptr;
        }
        return Pixels.data() + GetFaceMipOffset(face, mip);
    }
}
