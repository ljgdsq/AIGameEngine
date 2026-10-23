#include "Aster/Scene/Components.h"

#include <algorithm>
#include <cmath>

namespace Aster
{
    CameraData CameraComponent::BuildCameraData(const glm::mat4& worldMatrix, float aspectRatio) const
    {
        CameraData data;
        data.Position = glm::vec3(worldMatrix[3]);
        const glm::vec3 forward = SafeNormalize(-glm::vec3(worldMatrix[2]), glm::vec3(0.0f, 0.0f, -1.0f));
        const glm::vec3 up = SafeNormalize(glm::vec3(worldMatrix[1]), glm::vec3(0.0f, 1.0f, 0.0f));
        data.View = glm::lookAt(data.Position, data.Position + forward, up);
        const float aspect = std::max(aspectRatio, 0.0001f);
        if (Perspective)
        {
            data.Projection = glm::perspectiveRH_ZO(glm::radians(std::clamp(FieldOfView, 1.0f, 179.0f)), aspect,
                std::max(NearPlane, 0.0001f), std::max(FarPlane, NearPlane + 0.0001f));
        }
        else
        {
            const float halfHeight = std::max(OrthographicSize, 0.0001f) * 0.5f;
            const float halfWidth = halfHeight * aspect;
            data.Projection = glm::orthoRH_ZO(-halfWidth, halfWidth, -halfHeight, halfHeight,
                std::max(NearPlane, 0.0001f), std::max(FarPlane, NearPlane + 0.0001f));
        }
        data.ViewProjection = data.Projection * data.View;
        data.Near = NearPlane;
        data.Far = FarPlane;
        return data;
    }
}
