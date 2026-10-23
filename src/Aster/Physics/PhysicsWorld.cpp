#include "Aster/Physics/PhysicsWorld.h"

#include "Aster/Core/Log.h"

#include <algorithm>
#include <cmath>

namespace Aster
{
    namespace
    {
        constexpr float PhysicsEpsilon = 1.0e-5f;
    }

    PhysicsWorld::PhysicsWorld() = default;

    glm::vec3 PhysicsWorld::BoxMin(const Body& body) const
    {
        return body.Position - body.HalfExtents;
    }

    glm::vec3 PhysicsWorld::BoxMax(const Body& body) const
    {
        return body.Position + body.HalfExtents;
    }

    void PhysicsWorld::Clear()
    {
        m_Bodies.clear();
        m_CollisionEvents.clear();
        m_Grounded.clear();
    }

    void PhysicsWorld::Rebuild(Scene& scene)
    {
        Clear();
        for (Entity entity : scene.GetEntities())
        {
            const Registry& registry = scene.GetRegistry();
            if (!registry.Has<RigidBodyComponent>(entity))
            {
                continue;
            }

            const RigidBodyComponent& rigidBody = registry.Get<RigidBodyComponent>(entity);
            Body body;
            body.Owner = entity;
            body.Type = rigidBody.Type;
            body.Mass = std::max(rigidBody.Mass, 0.001f);
            body.Restitution = std::clamp(rigidBody.Restitution, 0.0f, 1.0f);
            body.Friction = std::clamp(rigidBody.Friction, 0.0f, 2.0f);
            body.Velocity = rigidBody.LinearVelocity;
            body.LinearDamping = std::clamp(rigidBody.LinearDamping, 0.0f, 100.0f);
            body.UseGravity = rigidBody.UseGravity;
            body.InverseMass = body.Type == BodyType::Dynamic ? 1.0f / body.Mass : 0.0f;

            const Transform world = scene.GetWorldTransform(entity);
            const glm::vec3 scale = glm::max(glm::abs(world.Scale), glm::vec3(0.0001f));
            if (registry.Has<BoxColliderComponent>(entity))
            {
                const BoxColliderComponent& collider = registry.Get<BoxColliderComponent>(entity);
                body.Shape = ColliderShape::Box;
                body.HalfExtents = glm::max(glm::abs(collider.HalfExtents) * scale, glm::vec3(0.0001f));
                body.CenterOffset = collider.Center * world.Scale;
                body.IsTrigger = collider.IsTrigger;
            }
            else if (registry.Has<SphereColliderComponent>(entity))
            {
                const SphereColliderComponent& collider = registry.Get<SphereColliderComponent>(entity);
                body.Shape = ColliderShape::Sphere;
                body.Radius = std::max(collider.Radius * std::max({ scale.x, scale.y, scale.z }), 0.0001f);
                body.CenterOffset = collider.Center * world.Scale;
                body.IsTrigger = collider.IsTrigger;
            }
            else
            {
                ASTER_LOG_WARN("Physics", "Entity '%s' has a rigid body but no collider",
                    registry.Has<NameComponent>(entity) ? registry.Get<NameComponent>(entity).Name.c_str() : "unknown");
                continue;
            }

            body.Position = world.Translation + body.CenterOffset;
            m_Bodies.push_back(body);
        }
    }

    void PhysicsWorld::Step(float deltaTime)
    {
        m_CollisionEvents.clear();
        m_Grounded.clear();
        if (deltaTime <= 0.0f)
        {
            return;
        }

        const float clampedDelta = std::min(deltaTime, 0.05f);

        for (Body& body : m_Bodies)
        {
            if (body.Type == BodyType::Static)
            {
                continue;
            }

            if (body.Type == BodyType::Dynamic && body.UseGravity)
            {
                body.Velocity += m_Gravity * clampedDelta;
            }
            body.Velocity *= 1.0f / (1.0f + body.LinearDamping * clampedDelta);
            body.Position += body.Velocity * clampedDelta;
        }

        for (int iteration = 0; iteration < 4; ++iteration)
        {
            for (size_t first = 0; first < m_Bodies.size(); ++first)
            {
                for (size_t second = first + 1; second < m_Bodies.size(); ++second)
                {
                    Body& a = m_Bodies[first];
                    Body& b = m_Bodies[second];
                    if (a.Type == BodyType::Static && b.Type == BodyType::Static)
                    {
                        continue;
                    }

                    Contact contact;
                    if (!Intersect(a, b, contact))
                    {
                        continue;
                    }

                    if (iteration == 0)
                    {
                        m_CollisionEvents.push_back(CollisionEvent{ a.Owner, b.Owner, contact.Normal,
                            contact.Penetration, contact.Trigger });
                    }

                    if (contact.Trigger)
                    {
                        continue;
                    }

                    if (contact.Normal.y > 0.5f)
                    {
                        m_Grounded.insert(contact.B == &b ? b.Owner : a.Owner);
                    }
                    if (contact.Normal.y < -0.5f)
                    {
                        m_Grounded.insert(contact.A == &a ? a.Owner : b.Owner);
                    }

                    Resolve(contact);
                }
            }
        }

        // Keep bodies from falling through the world forever.
        for (Body& body : m_Bodies)
        {
            if (body.Type == BodyType::Dynamic && body.Position.y < -1000.0f)
            {
                body.Position.y = -1000.0f;
                body.Velocity = glm::vec3(0.0f);
            }
        }
    }

    void PhysicsWorld::SyncToScene(Scene& scene)
    {
        for (const Body& body : m_Bodies)
        {
            if (!scene.IsAlive(body.Owner))
            {
                continue;
            }
            Transform world = scene.GetWorldTransform(body.Owner);
            world.Translation = body.Position - body.CenterOffset;
            scene.SetWorldTransform(body.Owner, world);
        }
    }

    bool PhysicsWorld::Intersect(const Body& a, const Body& b, Contact& contact) const
    {
        contact.A = const_cast<Body*>(&a);
        contact.B = const_cast<Body*>(&b);
        contact.Trigger = a.IsTrigger || b.IsTrigger;

        if (a.Shape == ColliderShape::Sphere && b.Shape == ColliderShape::Sphere)
        {
            const glm::vec3 delta = b.Position - a.Position;
            const float distanceSquared = glm::dot(delta, delta);
            const float radius = a.Radius + b.Radius;
            if (distanceSquared >= radius * radius)
            {
                return false;
            }
            const float distance = std::sqrt(std::max(distanceSquared, PhysicsEpsilon));
            contact.Normal = distance > PhysicsEpsilon ? delta / distance : glm::vec3(0.0f, 1.0f, 0.0f);
            contact.Penetration = radius - distance;
            contact.Point = a.Position + contact.Normal * a.Radius;
            return true;
        }

        if (a.Shape == ColliderShape::Box && b.Shape == ColliderShape::Box)
        {
            const glm::vec3 delta = b.Position - a.Position;
            const glm::vec3 overlap = a.HalfExtents + b.HalfExtents - glm::abs(delta);
            if (overlap.x <= 0.0f || overlap.y <= 0.0f || overlap.z <= 0.0f)
            {
                return false;
            }

            if (overlap.x <= overlap.y && overlap.x <= overlap.z)
            {
                contact.Normal = glm::vec3(delta.x < 0.0f ? -1.0f : 1.0f, 0.0f, 0.0f);
                contact.Penetration = overlap.x;
            }
            else if (overlap.y <= overlap.z)
            {
                contact.Normal = glm::vec3(0.0f, delta.y < 0.0f ? -1.0f : 1.0f, 0.0f);
                contact.Penetration = overlap.y;
            }
            else
            {
                contact.Normal = glm::vec3(0.0f, 0.0f, delta.z < 0.0f ? -1.0f : 1.0f);
                contact.Penetration = overlap.z;
            }
            contact.Point = a.Position + delta * 0.5f;
            return true;
        }

        const Body& sphere = a.Shape == ColliderShape::Sphere ? a : b;
        const Body& box = a.Shape == ColliderShape::Box ? a : b;
        const glm::vec3 closest = glm::clamp(sphere.Position, BoxMin(box), BoxMax(box));
        glm::vec3 delta = sphere.Position - closest;
        float distanceSquared = glm::dot(delta, delta);
        const bool sphereIsA = a.Shape == ColliderShape::Sphere;

        if (distanceSquared > sphere.Radius * sphere.Radius)
        {
            return false;
        }

        float distance = std::sqrt(std::max(distanceSquared, PhysicsEpsilon));
        glm::vec3 normal;
        if (distance > PhysicsEpsilon)
        {
            normal = delta / distance;
            contact.Penetration = sphere.Radius - distance;
        }
        else
        {
            // Sphere center is inside the box: push out through the nearest face.
            const glm::vec3 centerDelta = sphere.Position - box.Position;
            const glm::vec3 faceDistance = glm::abs(centerDelta) - box.HalfExtents;
            const glm::vec3 penetration = -faceDistance;
            if (penetration.x <= penetration.y && penetration.x <= penetration.z)
            {
                normal = glm::vec3(centerDelta.x < 0.0f ? -1.0f : 1.0f, 0.0f, 0.0f);
                contact.Penetration = sphere.Radius + penetration.x;
            }
            else if (penetration.y <= penetration.z)
            {
                normal = glm::vec3(0.0f, centerDelta.y < 0.0f ? -1.0f : 1.0f, 0.0f);
                contact.Penetration = sphere.Radius + penetration.y;
            }
            else
            {
                normal = glm::vec3(0.0f, 0.0f, centerDelta.z < 0.0f ? -1.0f : 1.0f);
                contact.Penetration = sphere.Radius + penetration.z;
            }
        }

        contact.Normal = sphereIsA ? -normal : normal;
        contact.Point = closest;
        return contact.Penetration > 0.0f;
    }

    void PhysicsWorld::Resolve(Contact& contact)
    {
        Body& a = *contact.A;
        Body& b = *contact.B;
        const float inverseMassSum = a.InverseMass + b.InverseMass;
        if (inverseMassSum <= 0.0f)
        {
            return;
        }

        glm::vec3 relativeVelocity = b.Velocity - a.Velocity;
        const float velocityAlongNormal = glm::dot(relativeVelocity, contact.Normal);
        const float restitution = velocityAlongNormal < -1.0f ? std::min(a.Restitution, b.Restitution) : 0.0f;

        if (velocityAlongNormal < 0.0f)
        {
            const float impulseMagnitude = -(1.0f + restitution) * velocityAlongNormal / inverseMassSum;
            const glm::vec3 impulse = contact.Normal * impulseMagnitude;
            a.Velocity -= impulse * a.InverseMass;
            b.Velocity += impulse * b.InverseMass;

            relativeVelocity = b.Velocity - a.Velocity;
            const glm::vec3 tangent = relativeVelocity - contact.Normal * glm::dot(relativeVelocity, contact.Normal);
            const float tangentLength = glm::length(tangent);
            if (tangentLength > PhysicsEpsilon)
            {
                const glm::vec3 tangentDirection = tangent / tangentLength;
                float frictionImpulse = -glm::dot(relativeVelocity, tangentDirection) / inverseMassSum;
                const float friction = std::sqrt(a.Friction * b.Friction);
                frictionImpulse = std::clamp(frictionImpulse, -impulseMagnitude * friction,
                    impulseMagnitude * friction);
                a.Velocity -= tangentDirection * frictionImpulse * a.InverseMass;
                b.Velocity += tangentDirection * frictionImpulse * b.InverseMass;
            }
        }

        constexpr float slop = 0.0005f;
        constexpr float correctionPercent = 0.8f;
        const float correctionMagnitude =
            std::max(contact.Penetration - slop, 0.0f) / inverseMassSum * correctionPercent;
        const glm::vec3 correction = contact.Normal * correctionMagnitude;
        a.Position -= correction * a.InverseMass;
        b.Position += correction * b.InverseMass;
    }

    std::optional<RaycastHit> PhysicsWorld::Raycast(const Ray& ray, float maxDistance) const
    {
        const Ray normalized = ray.Normalized();
        std::optional<RaycastHit> best;
        float bestDistance = maxDistance;

        for (const Body& body : m_Bodies)
        {
            float distance = 0.0f;
            glm::vec3 normal(0.0f, 1.0f, 0.0f);
            if (body.Shape == ColliderShape::Sphere)
            {
                const std::optional<float> hit = IntersectRaySphere(normalized, body.Position, body.Radius,
                    bestDistance);
                if (!hit.has_value())
                {
                    continue;
                }
                distance = *hit;
                normal = SafeNormalize(normalized.At(distance) - body.Position, -normalized.Direction);
            }
            else
            {
                const Aabb bounds{ BoxMin(body), BoxMax(body) };
                const std::optional<float> hit = IntersectRayAabb(normalized, bounds, bestDistance);
                if (!hit.has_value())
                {
                    continue;
                }
                distance = *hit;
                const glm::vec3 point = normalized.At(distance);
                const glm::vec3 centerDelta = point - body.Position;
                const glm::vec3 relative = glm::abs(centerDelta) / body.HalfExtents;
                if (relative.x >= relative.y && relative.x >= relative.z)
                {
                    normal = glm::vec3(centerDelta.x < 0.0f ? -1.0f : 1.0f, 0.0f, 0.0f);
                }
                else if (relative.y >= relative.z)
                {
                    normal = glm::vec3(0.0f, centerDelta.y < 0.0f ? -1.0f : 1.0f, 0.0f);
                }
                else
                {
                    normal = glm::vec3(0.0f, 0.0f, centerDelta.z < 0.0f ? -1.0f : 1.0f);
                }
            }

            if (distance <= bestDistance)
            {
                bestDistance = distance;
                RaycastHit result;
                result.HitEntity = body.Owner;
                result.Distance = distance;
                result.Point = normalized.At(distance);
                result.Normal = normal;
                best = result;
            }
        }
        return best;
    }

    bool PhysicsWorld::IsGrounded(Entity entity) const
    {
        return m_Grounded.contains(entity);
    }
}
