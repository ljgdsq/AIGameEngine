#pragma once

#include <glm/glm.hpp>

#include <array>
#include <string>

struct GLFWwindow;

namespace Aster
{
    struct WindowDesc
    {
        int Width = 1600;
        int Height = 900;
        std::string Title = "Aster Engine";
        bool Resizable = true;
    };

    class Window
    {
    public:
        Window() = default;
        ~Window();

        Window(const Window&) = delete;
        Window& operator=(const Window&) = delete;

        [[nodiscard]] bool Initialize(const WindowDesc& desc, std::string* error = nullptr);
        void Shutdown();

        [[nodiscard]] bool ShouldClose() const;
        void RequestClose();
        void PollEvents();
        void WaitEvents();

        [[nodiscard]] int GetWidth() const { return m_Width; }
        [[nodiscard]] int GetHeight() const { return m_Height; }
        [[nodiscard]] bool WasResized() const { return m_Resized; }
        void ClearResized() { m_Resized = false; }
        [[nodiscard]] float GetAspectRatio() const;

        [[nodiscard]] GLFWwindow* GetHandle() const { return m_Window; }
        [[nodiscard]] void* GetWin32Handle() const;

        [[nodiscard]] bool IsKeyDown(int key) const;
        [[nodiscard]] bool IsMouseButtonDown(int button) const;
        [[nodiscard]] glm::vec2 GetMousePosition() const { return m_MousePosition; }
        [[nodiscard]] glm::vec2 GetMouseDelta() const { return m_MouseDelta; }
        [[nodiscard]] float GetScrollDelta() const { return m_ScrollDelta; }
        [[nodiscard]] double GetTime() const;

        void SetCursorCaptured(bool captured);
        [[nodiscard]] bool IsCursorCaptured() const { return m_CursorCaptured; }
        void SetTitle(const std::string& title);

    private:
        static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
        static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
        static void CursorPositionCallback(GLFWwindow* window, double x, double y);
        static void ScrollCallback(GLFWwindow* window, double xOffset, double yOffset);
        static void FramebufferSizeCallback(GLFWwindow* window, int width, int height);

        GLFWwindow* m_Window = nullptr;
        int m_Width = 0;
        int m_Height = 0;
        bool m_Resized = false;
        bool m_CursorCaptured = false;
        bool m_FirstMouse = true;
        std::array<bool, 512> m_Keys{};
        std::array<bool, 8> m_MouseButtons{};
        glm::vec2 m_MousePosition{ 0.0f };
        glm::vec2 m_LastMousePosition{ 0.0f };
        glm::vec2 m_MouseDelta{ 0.0f };
        float m_ScrollDelta = 0.0f;
    };
}
