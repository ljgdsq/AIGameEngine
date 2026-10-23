#pragma once

#include "Aster/Assets/Asset.h"
#include "Aster/Core/UUID.h"
#include "Aster/ECS/Entity.h"
#include "Aster/Math/Math.h"
#include "Aster/Rendering/Camera.h"

#include <glm/glm.hpp>

#include <string>
#include <vector>

namespace Aster
{
    struct NameComponent
    {
        std::string Name = "Entity";
    };

    struct IDComponent
    {
        UUID Id;
    };

    struct TransformComponent
    {
        Transform Local;
    };

    struct RelationshipComponent
    {
        Entity Parent = NullEntity;
        std::vector<Entity> Children;
    };

    struct CameraComponent
    {
        bool Primary = true;
        bool Perspective = true;
        float FieldOfView = 60.0f;
        float NearPlane = 0.1f;
        float FarPlane = 1000.0f;
        float OrthographicSize = 10.0f;
        float Exposure = 1.0f;

        [[nodiscard]] CameraData BuildCameraData(const glm::mat4& worldMatrix, float aspectRatio) const;
    };

    struct DirectionalLightComponent
    {
        glm::vec3 Color{ 1.0f, 0.97f, 0.92f };
        float Intensity = 3.0f;
        bool CastShadows = true;
        float ShadowBias = 0.0025f;
        float ShadowNormalBias = 0.02f;
    };

    struct PointLightComponent
    {
        glm::vec3 Color{ 1.0f };
        float Intensity = 10.0f;
        float Range = 10.0f;
        bool CastShadows = false;
    };

    struct SpotLightComponent
    {
        glm::vec3 Color{ 1.0f };
        float Intensity = 20.0f;
        float Range = 15.0f;
        float InnerAngle = 20.0f;
        float OuterAngle = 35.0f;
        bool CastShadows = false;
    };

    struct MeshRendererComponent
    {
        AssetRef<MeshAsset> Mesh;
        std::vector<Material> Materials;
        bool CastShadows = true;
        bool ReceiveShadows = true;
        bool Visible = true;

        [[nodiscard]] const std::vector<Material>& GetMaterials() const
        {
            return Materials.empty() && Mesh.IsValid() ? Mesh->Materials : Materials;
        }
    };

    enum class BodyType : uint8
    {
        Static,
        Kinematic,
        Dynamic
    };

    struct RigidBodyComponent
    {
        BodyType Type = BodyType::Dynamic;
        float Mass = 1.0f;
        float Restitution = 0.1f;
        float Friction = 0.6f;
        bool UseGravity = true;
        glm::vec3 LinearVelocity{ 0.0f };
        glm::vec3 AngularVelocity{ 0.0f };
        float LinearDamping = 0.05f;
        float AngularDamping = 0.1f;
        bool Sleep = false;
    };

    struct BoxColliderComponent
    {
        glm::vec3 HalfExtents{ 0.5f };
        glm::vec3 Center{ 0.0f };
        bool IsTrigger = false;
    };

    struct SphereColliderComponent
    {
        float Radius = 0.5f;
        glm::vec3 Center{ 0.0f };
        bool IsTrigger = false;
    };

    struct ScriptEntry
    {
        std::string Path;
        bool Enabled = true;
    };

    struct ScriptComponent
    {
        std::vector<ScriptEntry> Scripts;
    };

    struct AudioSourceComponent
    {
        AssetRef<Sound> Clip;
        bool PlayOnStart = true;
        bool Loop = false;
        bool Spatial = true;
        float Volume = 1.0f;
        float Pitch = 1.0f;
        float MinDistance = 1.0f;
        float MaxDistance = 30.0f;
    };

    struct AudioListenerComponent
    {
        bool Primary = true;
    };
}
