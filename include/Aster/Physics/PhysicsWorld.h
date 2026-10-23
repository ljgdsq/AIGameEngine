#pragma once

#include "Aster/ECS/Entity.h"
#include "Aster/Math/Math.h"
#include "Aster/Scene/Scene.h"

#include <glm/glm.hpp>

#include <optional>
#include <unordered_set>
#include <vector>

namespace Aster
{
    struct CollisionEvent
    {
        Entity A = NullEntity;
        Entity B = NullEntity;
        glm::vec3 Normal{ 0.0f, 1.0f, 0.0f };
        float Penetration = 0.0f;
        bool Trigger = false;
    };

    struct RaycastHit
    {
        Entity HitEntity = NullEntity;
        float Distance = 0.0f;
        glm::vec3 Point{ 0.0f };
        glm::vec3 Normal{ 0.0f, 1.0f, 0.0f };
    };

    class PhysicsWorld
    {
    public:
        PhysicsWorld();

        void SetGravity(const glm::vec3& gravity) { m_Gravity = gravity; }
        [[nodiscard]] const glm::vec3& GetGravity() const { return m_Gravity; }

        // Recreates simulation bodies from the scene's rigid bodies and colliders.
        void Rebuild(Scene& scene);
        void Clear();
        void Step(float deltaTime);
        void SyncToScene(Scene& scene);

        [[nodiscard]] size_t GetBodyCount() const { return m_Bodies.size(); }
        [[nodiscard]] std::optional<RaycastHit> Raycast(const Ray& ray, float maxDistance) const;
        [[nodiscard]] const std::vector<CollisionEvent>& GetCollisionEvents() const { return m_CollisionEvents; }
        [[nodiscard]] bool IsGrounded(Entity entity) const;

    private:
        enum class ColliderShape : uint8
        {
            Box,
            Sphere
        };

        struct Body
        {
            Entity Owner = NullEntity;
            BodyType Type = BodyType::Dynamic;
            ColliderShape Shape = ColliderShape::Box;
            glm::vec3 Position{ 0.0f };
            glm::vec3 Velocity{ 0.0f };
            glm::vec3 HalfExtents{ 0.5f };
            glm::vec3 CenterOffset{ 0.0f };
            float Radius = 0.5f;
            float Mass = 1.0f;
            float InverseMass = 1.0f;
            float Restitution = 0.1f;
            float Friction = 0.6f;
            float LinearDamping = 0.05f;
            bool UseGravity = true;
            bool IsTrigger = false;
        };

        struct Contact
        {
            Body* A = nullptr;
            Body* B = nullptr;
            glm::vec3 Normal{ 0.0f, 1.0f, 0.0f };
            float Penetration = 0.0f;
            glm::vec3 Point{ 0.0f };
            bool Trigger = false;
        };

        [[nodiscard]] bool Intersect(const Body& a, const Body& b, Contact& contact) const;
        void Resolve(Contact& contact);
        [[nodiscard]] glm::vec3 BoxMin(const Body& body) const;
        [[nodiscard]] glm::vec3 BoxMax(const Body& body) const;

        std::vector<Body> m_Bodies;
        std::vector<CollisionEvent> m_CollisionEvents;
        std::unordered_set<Entity> m_Grounded;
        glm::vec3 m_Gravity{ 0.0f, -9.81f, 0.0f };
    };
}
