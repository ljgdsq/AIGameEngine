#include "Aster/Rendering/Camera.h"

#include <algorithm>
#include <cmath>

namespace Aster
{
    Camera::Camera()
    {
        UpdateProjection();
    }

    void Camera::SetPerspective(float fieldOfViewDegrees, float aspectRatio, float nearPlane, float farPlane)
    {
        m_Perspective = true;
        m_FieldOfViewDegrees = std::clamp(fieldOfViewDegrees, 1.0f, 179.0f);
        m_AspectRatio = std::max(aspectRatio, 0.0001f);
        m_NearPlane = std::max(nearPlane, 0.0001f);
        m_FarPlane = std::max(farPlane, m_NearPlane + 0.0001f);
        UpdateProjection();
    }

    void Camera::SetOrthographic(float size, float aspectRatio, float nearPlane, float farPlane)
    {
        m_Perspective = false;
        m_OrthographicSize = std::max(size, 0.0001f);
        m_AspectRatio = std::max(aspectRatio, 0.0001f);
        m_NearPlane = std::max(nearPlane, 0.0001f);
        m_FarPlane = std::max(farPlane, m_NearPlane + 0.0001f);
        UpdateProjection();
    }

    void Camera::SetViewportSize(uint32 width, uint32 height)
    {
        m_ViewportWidth = std::max(width, 1U);
        m_ViewportHeight = std::max(height, 1U);
        SetAspectRatio(static_cast<float>(m_ViewportWidth) / static_cast<float>(m_ViewportHeight));
    }

    void Camera::SetAspectRatio(float aspectRatio)
    {
        m_AspectRatio = std::max(aspectRatio, 0.0001f);
        UpdateProjection();
    }

    void Camera::SetRotation(float yawDegrees, float pitchDegrees)
    {
        m_YawDegrees = yawDegrees;
        m_PitchDegrees = std::clamp(pitchDegrees, -89.0f, 89.0f);
    }

    glm::vec3 Camera::GetForward() const
    {
        const float yaw = glm::radians(m_YawDegrees);
        const float pitch = glm::radians(m_PitchDegrees);
        return SafeNormalize(glm::vec3(std::cos(pitch) * std::cos(yaw), std::sin(pitch),
            std::cos(pitch) * std::sin(yaw)));
    }

    glm::vec3 Camera::GetRight() const
    {
        return SafeNormalize(glm::cross(GetForward(), glm::vec3(0.0f, 1.0f, 0.0f)));
    }

    glm::vec3 Camera::GetUp() const
    {
        return SafeNormalize(glm::cross(GetRight(), GetForward()));
    }

    glm::mat4 Camera::GetView() const
    {
        return glm::lookAt(m_Position, m_Position + GetForward(), glm::vec3(0.0f, 1.0f, 0.0f));
    }

    CameraData Camera::GetCameraData() const
    {
        CameraData data;
        data.View = GetView();
        data.Projection = m_Projection;
        data.ViewProjection = m_Projection * data.View;
        data.Position = m_Position;
        data.Near = m_NearPlane;
        data.Far = m_FarPlane;
        return data;
    }

    Ray Camera::ScreenToWorldRay(const glm::vec2& screenPosition, const glm::vec2& viewportSize) const
    {
        const glm::mat4 inverseViewProjection = glm::inverse(GetViewProjection());
        const glm::vec2 viewport = glm::max(viewportSize, glm::vec2(1.0f));
        const glm::vec2 ndc(screenPosition.x / viewport.x * 2.0f - 1.0f,
            1.0f - screenPosition.y / viewport.y * 2.0f);

        glm::vec4 nearPoint = inverseViewProjection * glm::vec4(ndc, 0.0f, 1.0f);
        glm::vec4 farPoint = inverseViewProjection * glm::vec4(ndc, 1.0f, 1.0f);
        if (std::abs(nearPoint.w) < Epsilon || std::abs(farPoint.w) < Epsilon)
        {
            return Ray{ m_Position, GetForward() };
        }

        nearPoint /= nearPoint.w;
        farPoint /= farPoint.w;
        Ray ray;
        ray.Origin = glm::vec3(nearPoint);
        ray.Direction = SafeNormalize(glm::vec3(farPoint) - ray.Origin);
        return ray;
    }

    bool Camera::WorldToScreen(const glm::vec3& worldPosition, const glm::vec2& viewportSize,
        glm::vec2& screenPosition) const
    {
        const glm::vec4 clip = GetViewProjection() * glm::vec4(worldPosition, 1.0f);
        if (clip.w <= 0.0f)
        {
            return false;
        }

        const glm::vec3 ndc = glm::vec3(clip) / clip.w;
        screenPosition = glm::vec2((ndc.x * 0.5f + 0.5f) * viewportSize.x,
            (0.5f - ndc.y * 0.5f) * viewportSize.y);
        return true;
    }

    void Camera::UpdateProjection()
    {
        if (m_Perspective)
        {
            m_Projection = glm::perspectiveRH_ZO(glm::radians(m_FieldOfViewDegrees), m_AspectRatio,
                m_NearPlane, m_FarPlane);
        }
        else
        {
            const float halfHeight = m_OrthographicSize * 0.5f;
            const float halfWidth = halfHeight * m_AspectRatio;
            m_Projection = glm::orthoRH_ZO(-halfWidth, halfWidth, -halfHeight, halfHeight, m_NearPlane,
                m_FarPlane);
        }
    }
}
