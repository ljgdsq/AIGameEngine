#pragma once

#include "Aster/Core/Base.h"
#include "Aster/Math/Math.h"

#include <glm/glm.hpp>

namespace Aster
{
    struct CameraData
    {
        glm::mat4 View{ 1.0f };
        glm::mat4 Projection{ 1.0f };
        glm::mat4 ViewProjection{ 1.0f };
        glm::vec3 Position{ 0.0f };
        float Near = 0.1f;
        float Far = 1000.0f;
    };

    class Camera
    {
    public:
        Camera();

        void SetPerspective(float fieldOfViewDegrees, float aspectRatio, float nearPlane, float farPlane);
        void SetOrthographic(float size, float aspectRatio, float nearPlane, float farPlane);
        void SetViewportSize(uint32 width, uint32 height);
        void SetAspectRatio(float aspectRatio);

        void SetPosition(const glm::vec3& position) { m_Position = position; }
        [[nodiscard]] const glm::vec3& GetPosition() const { return m_Position; }

        void SetRotation(float yawDegrees, float pitchDegrees);
        void SetYaw(float yawDegrees) { SetRotation(yawDegrees, m_PitchDegrees); }
        void SetPitch(float pitchDegrees) { SetRotation(m_YawDegrees, pitchDegrees); }
        [[nodiscard]] float GetYaw() const { return m_YawDegrees; }
        [[nodiscard]] float GetPitch() const { return m_PitchDegrees; }

        [[nodiscard]] glm::vec3 GetForward() const;
        [[nodiscard]] glm::vec3 GetRight() const;
        [[nodiscard]] glm::vec3 GetUp() const;

        [[nodiscard]] glm::mat4 GetView() const;
        [[nodiscard]] const glm::mat4& GetProjection() const { return m_Projection; }
        [[nodiscard]] glm::mat4 GetViewProjection() const { return m_Projection * GetView(); }
        [[nodiscard]] CameraData GetCameraData() const;

        [[nodiscard]] float GetFieldOfView() const { return m_FieldOfViewDegrees; }
        [[nodiscard]] float GetNearPlane() const { return m_NearPlane; }
        [[nodiscard]] float GetFarPlane() const { return m_FarPlane; }
        [[nodiscard]] float GetAspectRatio() const { return m_AspectRatio; }
        [[nodiscard]] bool IsPerspective() const { return m_Perspective; }
        [[nodiscard]] uint32 GetViewportWidth() const { return m_ViewportWidth; }
        [[nodiscard]] uint32 GetViewportHeight() const { return m_ViewportHeight; }

        [[nodiscard]] Ray ScreenToWorldRay(const glm::vec2& screenPosition, const glm::vec2& viewportSize) const;
        [[nodiscard]] bool WorldToScreen(const glm::vec3& worldPosition, const glm::vec2& viewportSize,
            glm::vec2& screenPosition) const;

    private:
        void UpdateProjection();

        glm::vec3 m_Position{ 0.0f, 2.0f, 5.0f };
        float m_YawDegrees = -90.0f;
        float m_PitchDegrees = 0.0f;
        float m_FieldOfViewDegrees = 60.0f;
        float m_AspectRatio = 16.0f / 9.0f;
        float m_NearPlane = 0.1f;
        float m_FarPlane = 1000.0f;
        float m_OrthographicSize = 10.0f;
        uint32 m_ViewportWidth = 16;
        uint32 m_ViewportHeight = 9;
        bool m_Perspective = true;
        glm::mat4 m_Projection{ 1.0f };
    };
}
