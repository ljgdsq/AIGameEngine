#include "TestFramework.h"

#include "Aster/Platform/Window.h"

#include <GLFW/glfw3.h>

using namespace Aster;

ASTER_TEST(Window, KeyNamesMapToGlfwCodes)
{
    ASTER_CHECK_EQ(Window::KeyFromName("W"), GLFW_KEY_W);
    ASTER_CHECK_EQ(Window::KeyFromName("w"), GLFW_KEY_W);
    ASTER_CHECK_EQ(Window::KeyFromName("A"), GLFW_KEY_A);
    ASTER_CHECK_EQ(Window::KeyFromName("7"), GLFW_KEY_7);
    ASTER_CHECK_EQ(Window::KeyFromName("Space"), GLFW_KEY_SPACE);
    ASTER_CHECK_EQ(Window::KeyFromName("space"), GLFW_KEY_SPACE);
    ASTER_CHECK_EQ(Window::KeyFromName("LeftShift"), GLFW_KEY_LEFT_SHIFT);
    ASTER_CHECK_EQ(Window::KeyFromName("Shift"), GLFW_KEY_LEFT_SHIFT);
    ASTER_CHECK_EQ(Window::KeyFromName("LeftControl"), GLFW_KEY_LEFT_CONTROL);
    ASTER_CHECK_EQ(Window::KeyFromName("Up"), GLFW_KEY_UP);
    ASTER_CHECK_EQ(Window::KeyFromName("PageDown"), GLFW_KEY_PAGE_DOWN);
    ASTER_CHECK_EQ(Window::KeyFromName("F5"), GLFW_KEY_F5);
    ASTER_CHECK_EQ(Window::KeyFromName("F12"), GLFW_KEY_F12);

    ASTER_CHECK_EQ(Window::KeyFromName(""), -1);
    ASTER_CHECK_EQ(Window::KeyFromName("NotAKey"), -1);
    ASTER_CHECK_EQ(Window::KeyFromName("F99"), -1);
}
