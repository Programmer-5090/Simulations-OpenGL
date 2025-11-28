#include "input.h"

#include <algorithm>
#include <array>

namespace {
constexpr std::array<std::pair<const char*, int>, 3> kMouseButtonMap = {{
    {"left", GLFW_MOUSE_BUTTON_LEFT},
    {"middle", GLFW_MOUSE_BUTTON_MIDDLE},
    {"right", GLFW_MOUSE_BUTTON_RIGHT},
}};
}

Input::Input(GLFWwindow* windowPtr)
    : window(windowPtr) {
    mouseButtons["left"] = false;
    mouseButtons["middle"] = false;
    mouseButtons["right"] = false;
    keyState.fill(false);
}

void Input::setWindow(GLFWwindow* windowPtr) {
    window = windowPtr;
}

void Input::reset() {
    keysDown.clear();
    keysUp.clear();
    keysHeld.clear();
}

void Input::update() {
    if (!window) {
        return;
    }

    reset();
    glfwPollEvents();

    quit = glfwWindowShouldClose(window) != 0;

    for (int key = 0; key <= GLFW_KEY_LAST; ++key) {
        int state = glfwGetKey(window, key);
        bool isPressed = (state == GLFW_PRESS || state == GLFW_REPEAT);
        if (isPressed) {
            keysHeld.push_back(key);
            if (!keyState[key]) {
                keysDown.push_back(key);
            }
        } else if (keyState[key]) {
            keysUp.push_back(key);
        }
        keyState[key] = isPressed;
    }

    double xpos = 0.0;
    double ypos = 0.0;
    glfwGetCursorPos(window, &xpos, &ypos);
    mousePos = { static_cast<int>(xpos), static_cast<int>(ypos) };

    for (const auto& [name, button] : kMouseButtonMap) {
        mouseButtons[name] = glfwGetMouseButton(window, button) == GLFW_PRESS;
    }
}

bool Input::isKeyDown(int key) const {
    return std::find(keysDown.begin(), keysDown.end(), key) != keysDown.end();
}

bool Input::isKeyUp(int key) const {
    return std::find(keysUp.begin(), keysUp.end(), key) != keysUp.end();
}

bool Input::isKeyHeld(int key) const {
    return std::find(keysHeld.begin(), keysHeld.end(), key) != keysHeld.end();
}

const std::map<std::string, bool>& Input::getMouseButtons() const {
    return mouseButtons;
}

std::pair<int, int> Input::getMousePos() const {
    return mousePos;
}

void Input::select(ID id) {
    if (id >= 0) {
        selectedID = id;
        isSelecting = true;
    }
}

void Input::deselect() {
    selectedID = -1;
    isSelecting = false;
}
