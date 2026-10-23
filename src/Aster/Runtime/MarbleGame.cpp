#include "Aster/Runtime/MarbleGame.h"

#include "Aster/Core/Log.h"
#include "Aster/Physics/PhysicsWorld.h"
#include "Aster/Scene/Scene.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace Aster
{
    namespace
    {
        constexpr float kMoveSpeed = 6.0f;
        constexpr float kJumpSpeed = 6.5f;
        constexpr float kCoinRadius = 1.0f;
        constexpr float kGoalRadius = 1.5f;
        constexpr float kFallLimit = -8.0f;
        constexpr glm::vec3 kCameraOffset{ 0.0f, 5.5f, 8.0f };

        bool StartsWith(const std::string& value, const std::string& prefix)
        {
            return value.size() >= prefix.size() &&
                value.compare(0, prefix.size(), prefix) == 0;
        }
    }

    bool MarbleGame::Initialize(Scene& scene, PhysicsWorld& physics)
    {
        m_Scene = &scene;
        m_Physics = &physics;
        m_Coins.clear();
        m_Collected.clear();
        m_Score = 0;
        m_Elapsed = 0.0f;
        m_Complete = false;
        m_JumpLatch = false;
        m_RestartLatch = false;
        m_Message.clear();

        m_Player = scene.FindByName("Marble");
        m_Goal = scene.FindByName("Goal");
        m_Camera = scene.FindByName("Main Camera");
        if (m_Camera == NullEntity)
        {
            m_Camera = scene.GetPrimaryCamera();
        }
        if (m_Player == NullEntity || m_Goal == NullEntity)
        {
            m_Active = false;
            return false;
        }

        m_StartPosition = glm::vec3(scene.GetWorldMatrix(m_Player)[3]);
        for (Entity entity : scene.GetEntities())
        {
            if (!scene.GetRegistry().Has<NameComponent>(entity))
            {
                continue;
            }
            const std::string& name = scene.GetRegistry().Get<NameComponent>(entity).Name;
            if (StartsWith(name, "Coin"))
            {
                m_Coins.push_back(entity);
                m_Collected.push_back(false);
            }
        }

        m_Active = true;
        ASTER_LOG_INFO("Demo", "Marble Run ready: %zu coin(s), goal at (%.1f, %.1f, %.1f)",
            m_Coins.size(), glm::vec3(scene.GetWorldMatrix(m_Goal)[3]).x,
            glm::vec3(scene.GetWorldMatrix(m_Goal)[3]).y, glm::vec3(scene.GetWorldMatrix(m_Goal)[3]).z);
        UpdateCamera();
        return true;
    }

    void MarbleGame::Update(float deltaTime, const MarbleInput& input)
    {
        if (!m_Active || m_Scene == nullptr || m_Physics == nullptr || !m_Scene->IsAlive(m_Player))
        {
            return;
        }

        if (input.Restart && !m_RestartLatch)
        {
            m_RestartLatch = true;
            Reset();
            return;
        }
        m_RestartLatch = input.Restart;

        const float dt = std::clamp(deltaTime, 0.0f, 0.1f);
        if (!m_Complete)
        {
            m_Elapsed += dt;

            glm::vec3 velocity = m_Physics->GetLinearVelocity(m_Player);
            const float forward = std::clamp(input.Forward, -1.0f, 1.0f);
            const float right = std::clamp(input.Right, -1.0f, 1.0f);
            glm::vec3 movement(right, 0.0f, -forward);
            if (glm::length(movement) > 1.0f)
            {
                movement = glm::normalize(movement);
            }
            velocity.x = movement.x * kMoveSpeed;
            velocity.z = movement.z * kMoveSpeed;

            if (input.Jump && !m_JumpLatch && m_Physics->IsGrounded(m_Player))
            {
                velocity.y = kJumpSpeed;
            }
            m_JumpLatch = input.Jump;
            m_Physics->SetLinearVelocity(m_Player, velocity);

            const glm::vec3 playerPosition = glm::vec3(m_Scene->GetWorldMatrix(m_Player)[3]);
            for (size_t index = 0; index < m_Coins.size(); ++index)
            {
                if (m_Collected[index] || !m_Scene->IsAlive(m_Coins[index]))
                {
                    continue;
                }
                const glm::vec3 coinPosition = glm::vec3(m_Scene->GetWorldMatrix(m_Coins[index])[3]);
                if (glm::length(coinPosition - playerPosition) <= kCoinRadius)
                {
                    m_Collected[index] = true;
                    m_Score++;
                    SetCoinVisible(m_Coins[index], false);
                    ASTER_LOG_INFO("Demo", "Coin collected (%d/%zu)", m_Score, m_Coins.size());
                }
            }

            if (glm::length(glm::vec3(m_Scene->GetWorldMatrix(m_Goal)[3]) - playerPosition) <= kGoalRadius)
            {
                m_Complete = true;
                m_Message = "GOAL! Press R to run it again";
                ASTER_LOG_INFO("Demo", "Marble Run complete in %.1f seconds with %d/%zu coins",
                    m_Elapsed, m_Score, m_Coins.size());
            }

            if (playerPosition.y < kFallLimit)
            {
                ResetPlayer();
                m_Message = "Back to the start";
                ASTER_LOG_WARN("Demo", "Marble fell out of the level; resetting to the start");
            }
        }

        UpdateCamera();
    }

    void MarbleGame::Reset()
    {
        if (!m_Active || m_Scene == nullptr)
        {
            return;
        }

        m_Score = 0;
        m_Elapsed = 0.0f;
        m_Complete = false;
        m_JumpLatch = false;
        m_Message.clear();
        for (size_t index = 0; index < m_Coins.size(); ++index)
        {
            m_Collected[index] = false;
            SetCoinVisible(m_Coins[index], true);
        }
        ResetPlayer();
        ASTER_LOG_INFO("Demo", "Marble Run reset");
    }

    glm::vec3 MarbleGame::GetPlayerPosition() const
    {
        if (m_Scene == nullptr || m_Player == NullEntity || !m_Scene->IsAlive(m_Player))
        {
            return m_StartPosition;
        }
        return glm::vec3(m_Scene->GetWorldMatrix(m_Player)[3]);
    }

    void MarbleGame::ResetPlayer()
    {
        if (m_Scene == nullptr || m_Physics == nullptr || !m_Scene->IsAlive(m_Player))
        {
            return;
        }

        Registry& registry = m_Scene->GetRegistry();
        if (registry.Has<TransformComponent>(m_Player))
        {
            TransformComponent& transform = registry.Get<TransformComponent>(m_Player);
            transform.Local.Translation = m_StartPosition;
            transform.Local.Rotation = glm::vec3(0.0f);
        }
        m_Physics->Rebuild(*m_Scene);
        m_Physics->SetLinearVelocity(m_Player, glm::vec3(0.0f));
    }

    void MarbleGame::UpdateCamera()
    {
        if (m_Scene == nullptr || !m_Scene->IsAlive(m_Camera))
        {
            return;
        }

        Registry& registry = m_Scene->GetRegistry();
        if (!registry.Has<TransformComponent>(m_Camera))
        {
            return;
        }
        TransformComponent& transform = registry.Get<TransformComponent>(m_Camera);
        transform.Local.Translation = GetPlayerPosition() + kCameraOffset;
        transform.Local.Rotation = glm::vec3(-20.0f, 0.0f, 0.0f);
    }

    void MarbleGame::SetCoinVisible(Entity coin, bool visible)
    {
        if (m_Scene == nullptr || !m_Scene->IsAlive(coin))
        {
            return;
        }
        Registry& registry = m_Scene->GetRegistry();
        if (registry.Has<MeshRendererComponent>(coin))
        {
            registry.Get<MeshRendererComponent>(coin).Visible = visible;
        }
    }
}
