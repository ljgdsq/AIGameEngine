#pragma once

#include "Aster/ECS/Entity.h"

#include <glm/glm.hpp>

#include <string>
#include <vector>

namespace Aster
{
    class PhysicsWorld;
    class Scene;

    struct MarbleInput
    {
        float Forward = 0.0f;
        float Right = 0.0f;
        bool Jump = false;
        bool Restart = false;
    };

    // Small native gameplay sample used by the "Marble Run" demo project. It drives a dynamic sphere
    // through the engine's physics world, follows it with the primary camera, collects coins and reports
    // HUD state. The logic is deliberately independent of the window and renderer so it remains testable.
    class MarbleGame
    {
    public:
        [[nodiscard]] bool Initialize(Scene& scene, PhysicsWorld& physics);
        void Update(float deltaTime, const MarbleInput& input);
        void Reset();

        [[nodiscard]] bool IsActive() const { return m_Active; }
        [[nodiscard]] bool IsComplete() const { return m_Complete; }
        [[nodiscard]] int GetScore() const { return m_Score; }
        [[nodiscard]] int GetTotalCoins() const { return static_cast<int>(m_Coins.size()); }
        [[nodiscard]] float GetElapsedTime() const { return m_Elapsed; }
        [[nodiscard]] glm::vec3 GetPlayerPosition() const;
        [[nodiscard]] const std::string& GetStatusMessage() const { return m_Message; }

    private:
        void UpdateCamera();
        void ResetPlayer();
        void SetCoinVisible(Entity coin, bool visible);

        Scene* m_Scene = nullptr;
        PhysicsWorld* m_Physics = nullptr;
        Entity m_Player = NullEntity;
        Entity m_Goal = NullEntity;
        Entity m_Camera = NullEntity;
        std::vector<Entity> m_Coins;
        std::vector<bool> m_Collected;
        glm::vec3 m_StartPosition{ 0.0f };
        float m_Elapsed = 0.0f;
        int m_Score = 0;
        bool m_Complete = false;
        bool m_JumpLatch = false;
        bool m_RestartLatch = false;
        bool m_Active = false;
        std::string m_Message;
    };
}
