#include "Aster/Platform/Window.h"

#include "Aster/Core/Log.h"

#include <GLFW/glfw3.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <GLFW/glfw3native.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>

namespace Aster
{
    namespace
    {
        int s_WindowCount = 0;
    }

    int Window::KeyFromName(std::string_view name)
    {
        std::string key;
        key.reserve(name.size());
        for (char character : name)
        {
            if (character != ' ' && character != '_' && character != '-')
            {
                key.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(character))));
            }
        }
        if (key.empty())
        {
            return -1;
        }
        if (key.size() == 1)
        {
            if (key[0] >= 'A' && key[0] <= 'Z')
            {
                return GLFW_KEY_A + (key[0] - 'A');
            }
            if (key[0] >= '0' && key[0] <= '9')
            {
                return GLFW_KEY_0 + (key[0] - '0');
            }
        }

        if (key == "SPACE") return GLFW_KEY_SPACE;
        if (key == "ESCAPE" || key == "ESC") return GLFW_KEY_ESCAPE;
        if (key == "ENTER" || key == "RETURN") return GLFW_KEY_ENTER;
        if (key == "TAB") return GLFW_KEY_TAB;
        if (key == "BACKSPACE") return GLFW_KEY_BACKSPACE;
        if (key == "DELETE" || key == "DEL") return GLFW_KEY_DELETE;
        if (key == "INSERT" || key == "INS") return GLFW_KEY_INSERT;
        if (key == "HOME") return GLFW_KEY_HOME;
        if (key == "END") return GLFW_KEY_END;
        if (key == "PAGEUP" || key == "PGUP") return GLFW_KEY_PAGE_UP;
        if (key == "PAGEDOWN" || key == "PGDN") return GLFW_KEY_PAGE_DOWN;
        if (key == "UP" || key == "ARROWUP") return GLFW_KEY_UP;
        if (key == "DOWN" || key == "ARROWDOWN") return GLFW_KEY_DOWN;
        if (key == "LEFT" || key == "ARROWLEFT") return GLFW_KEY_LEFT;
        if (key == "RIGHT" || key == "ARROWRIGHT") return GLFW_KEY_RIGHT;
        if (key == "LEFTSHIFT" || key == "SHIFT") return GLFW_KEY_LEFT_SHIFT;
        if (key == "RIGHTSHIFT") return GLFW_KEY_RIGHT_SHIFT;
        if (key == "LEFTCTRL" || key == "LEFTCONTROL" || key == "CTRL" || key == "CONTROL")
        {
            return GLFW_KEY_LEFT_CONTROL;
        }
        if (key == "RIGHTCTRL" || key == "RIGHTCONTROL") return GLFW_KEY_RIGHT_CONTROL;
        if (key == "LEFTALT" || key == "ALT") return GLFW_KEY_LEFT_ALT;
        if (key == "RIGHTALT") return GLFW_KEY_RIGHT_ALT;
        if (key.size() >= 2 && key[0] == 'F')
        {
            const int index = std::atoi(key.c_str() + 1);
            if (index >= 1 && index <= 25)
            {
                return GLFW_KEY_F1 + (index - 1);
            }
        }
        return -1;
    }

    Window::~Window()
    {
        Shutdown();
    }

    bool Window::Initialize(const WindowDesc& desc, std::string* error)
    {
        if (m_Window != nullptr)
        {
            return true;
        }

        glfwSetErrorCallback([](int code, const char* description) {
            ASTER_LOG_ERROR("GLFW", "Error %d: %s", code, description);
        });

        if (s_WindowCount == 0 && glfwInit() == GLFW_FALSE)
        {
            if (error != nullptr)
            {
                *error = "Failed to initialize GLFW";
            }
            return false;
        }
        ++s_WindowCount;

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, desc.Resizable ? GLFW_TRUE : GLFW_FALSE);
        m_Window = glfwCreateWindow(desc.Width, desc.Height, desc.Title.c_str(), nullptr, nullptr);
        if (m_Window == nullptr)
        {
            --s_WindowCount;
            if (error != nullptr)
            {
                *error = "Failed to create a GLFW window";
            }
            return false;
        }

        glfwSetWindowUserPointer(m_Window, this);
        glfwSetKeyCallback(m_Window, KeyCallback);
        glfwSetMouseButtonCallback(m_Window, MouseButtonCallback);
        glfwSetCursorPosCallback(m_Window, CursorPositionCallback);
        glfwSetScrollCallback(m_Window, ScrollCallback);
        glfwSetFramebufferSizeCallback(m_Window, FramebufferSizeCallback);
        glfwGetFramebufferSize(m_Window, &m_Width, &m_Height);
        glfwSetWindowSizeLimits(m_Window, 640, 360, GLFW_DONT_CARE, GLFW_DONT_CARE);

        ASTER_LOG_INFO("Window", "Created %dx%d window '%s'", m_Width, m_Height, desc.Title.c_str());
        return true;
    }

    void Window::Shutdown()
    {
        if (m_Window != nullptr)
        {
            glfwDestroyWindow(m_Window);
            m_Window = nullptr;
        }
        if (s_WindowCount > 0)
        {
            --s_WindowCount;
            if (s_WindowCount == 0)
            {
                glfwTerminate();
            }
        }
    }

    bool Window::ShouldClose() const
    {
        return m_Window == nullptr || glfwWindowShouldClose(m_Window) == GLFW_TRUE;
    }

    void Window::RequestClose()
    {
        if (m_Window != nullptr)
        {
            glfwSetWindowShouldClose(m_Window, GLFW_TRUE);
        }
    }

    void Window::PollEvents()
    {
        m_MouseDelta = glm::vec2(0.0f);
        m_ScrollDelta = 0.0f;
        m_FirstMouse = m_FirstMouse || !m_CursorCaptured;
        glfwPollEvents();
    }

    void Window::WaitEvents()
    {
        m_MouseDelta = glm::vec2(0.0f);
        m_ScrollDelta = 0.0f;
        m_FirstMouse = m_FirstMouse || !m_CursorCaptured;
        glfwWaitEvents();
    }

    float Window::GetAspectRatio() const
    {
        return m_Height > 0 ? static_cast<float>(m_Width) / static_cast<float>(m_Height) : 1.0f;
    }

    void* Window::GetWin32Handle() const
    {
#ifdef _WIN32
        return m_Window != nullptr ? glfwGetWin32Window(m_Window) : nullptr;
#else
        return nullptr;
#endif
    }

    bool Window::IsKeyDown(int key) const
    {
        return key >= 0 && key < static_cast<int>(m_Keys.size()) && m_Keys[static_cast<size_t>(key)];
    }

    bool Window::IsMouseButtonDown(int button) const
    {
        return button >= 0 && button < static_cast<int>(m_MouseButtons.size()) &&
            m_MouseButtons[static_cast<size_t>(button)];
    }

    double Window::GetTime() const
    {
        return glfwGetTime();
    }

    void Window::SetCursorCaptured(bool captured)
    {
        if (m_Window == nullptr || m_CursorCaptured == captured)
        {
            return;
        }
        m_CursorCaptured = captured;
        m_FirstMouse = true;
        glfwSetInputMode(m_Window, GLFW_CURSOR, captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
        if (captured && glfwRawMouseMotionSupported() == GLFW_TRUE)
        {
            glfwSetInputMode(m_Window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
        }
        else
        {
            glfwSetInputMode(m_Window, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
        }
    }

    void Window::SetTitle(const std::string& title)
    {
        if (m_Window != nullptr)
        {
            glfwSetWindowTitle(m_Window, title.c_str());
        }
    }

    void Window::KeyCallback(GLFWwindow* window, int key, int, int action, int)
    {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
        if (self == nullptr || key < 0 || key >= static_cast<int>(self->m_Keys.size()))
        {
            return;
        }
        if (action == GLFW_PRESS)
        {
            self->m_Keys[static_cast<size_t>(key)] = true;
        }
        else if (action == GLFW_RELEASE)
        {
            self->m_Keys[static_cast<size_t>(key)] = false;
        }
    }

    void Window::MouseButtonCallback(GLFWwindow* window, int button, int action, int)
    {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
        if (self == nullptr || button < 0 || button >= static_cast<int>(self->m_MouseButtons.size()))
        {
            return;
        }
        self->m_MouseButtons[static_cast<size_t>(button)] = action == GLFW_PRESS;
    }

    void Window::CursorPositionCallback(GLFWwindow* window, double x, double y)
    {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
        if (self == nullptr)
        {
            return;
        }
        const glm::vec2 position(static_cast<float>(x), static_cast<float>(y));
        if (self->m_FirstMouse)
        {
            self->m_LastMousePosition = position;
            self->m_FirstMouse = false;
        }
        self->m_MouseDelta = position - self->m_LastMousePosition;
        self->m_LastMousePosition = position;
        self->m_MousePosition = position;
    }

    void Window::ScrollCallback(GLFWwindow* window, double, double yOffset)
    {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
        if (self != nullptr)
        {
            self->m_ScrollDelta += static_cast<float>(yOffset);
        }
    }

    void Window::FramebufferSizeCallback(GLFWwindow* window, int width, int height)
    {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
        if (self == nullptr)
        {
            return;
        }
        self->m_Width = std::max(width, 1);
        self->m_Height = std::max(height, 1);
        self->m_Resized = true;
    }
}
