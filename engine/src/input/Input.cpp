#include <wv/input/Input.h>

#include <wv/rendering/Window.h>
#include <wv/input/OpenGLKey.h>
#include <GLFW/glfw3.h>
#include <imgui.h>

namespace WillowVox
{
    bool Input::_pressed[348];
    bool Input::_released[348];
    bool Input::_mousePressed[7];
    bool Input::_mouseReleased[7];
    double Input::_mouseX = 0;
    double Input::_mouseY = 0;
    double Input::_prevMouseX = 0;
    double Input::_prevMouseY = 0;
    double Input::_scrollX = 0;
    double Input::_scrollY = 0;
    Window* Input::_window = nullptr;

    void Input::Init()
    {
        _window = Window::GetInstance();
        glfwSetKeyCallback(static_cast<GLFWwindow*>(_window->GetNativeWindow()), [](GLFWwindow* window, int key, int scancode, int action, int mods) {
            if (action == GLFW_PRESS)
                _pressed[key] = true;
            else if (action == GLFW_RELEASE)
                _released[key] = true;
            });
        glfwSetMouseButtonCallback(static_cast<GLFWwindow*>(_window->GetNativeWindow()), [](GLFWwindow* window, int button, int action, int mods) {
            if (action == GLFW_PRESS)
                _mousePressed[button] = true;
            else if (action == GLFW_RELEASE)
                _mouseReleased[button] = true;
            });
        glfwSetCursorPosCallback(static_cast<GLFWwindow*>(_window->GetNativeWindow()), [](GLFWwindow* window, double xpos, double ypos) {
            _mouseX = xpos;
            _mouseY = ypos;
            });
        glfwSetScrollCallback(static_cast<GLFWwindow*>(_window->GetNativeWindow()), [](GLFWwindow* window, double xoffset, double yoffset) {
            _scrollX = xoffset;
            _scrollY = yoffset;
            });

        glfwGetCursorPos(static_cast<GLFWwindow*>(_window->GetNativeWindow()), &_prevMouseX, &_prevMouseY);
        glfwGetCursorPos(static_cast<GLFWwindow*>(_window->GetNativeWindow()), &_mouseX, &_mouseY);
    }

    void Input::ResetStates()
    {
        for (int i = 0; i < 348; i++)
        {
            _pressed[i] = false;
            _released[i] = false;
        }
        for (int i = 0; i < 7; i++)
        {
            _mousePressed[i] = false;
            _mouseReleased[i] = false;
        }

        _prevMouseX = _mouseX;
        _prevMouseY = _mouseY;
        _scrollX = 0;
        _scrollY = 0;
    }

    bool Input::GetKey(Key key)
    {
        return glfwGetKey(static_cast<GLFWwindow*>(_window->GetNativeWindow()), keyToOpenGL[(int)key]) == GLFW_PRESS;
    }

    bool Input::GetKeyDown(Key key)
    {
        return _pressed[keyToOpenGL[(int)key]];
    }

    bool Input::GetKeyUp(Key key)
    {
        return _released[keyToOpenGL[(int)key]];
    }

    bool Input::GetMouseButton(int button)
    {
        return glfwGetMouseButton(static_cast<GLFWwindow*>(_window->GetNativeWindow()), button) == GLFW_PRESS;
    }

    bool Input::GetMouseButtonDown(int button)
    {
        return _mousePressed[button];
    }

    bool Input::GetMouseButtonUp(int button)
    {
        return _mouseReleased[button];
    }

    glm::vec2 Input::GetMousePos()
    {
        double x, y;
        glfwGetCursorPos(static_cast<GLFWwindow*>(_window->GetNativeWindow()), &x, &y);
        return { x, y };
    }

    glm::vec2 Input::GetMouseDelta()
    {
        return { _mouseX - _prevMouseX, _mouseY - _prevMouseY };
    }

    glm::vec2 Input::GetMouseScrollDelta()
    {
        return { _scrollX, _scrollY };
    }

    void Input::SetMouseMode(MouseMode mode)
    {
        ImGuiIO& io = ImGui::GetIO();
        switch (mode)
        {
        case MouseMode::NORMAL:
            glfwSetInputMode(static_cast<GLFWwindow*>(_window->GetNativeWindow()), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
            break;
        case MouseMode::DISABLED:
            glfwSetInputMode(static_cast<GLFWwindow*>(_window->GetNativeWindow()), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
            break;
        case MouseMode::HIDDEN:
            glfwSetInputMode(static_cast<GLFWwindow*>(_window->GetNativeWindow()), GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
            io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
            break;
        }
    }
}