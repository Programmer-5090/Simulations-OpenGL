#ifndef INPUT_H
#define INPUT_H

#include <GLFW/glfw3.h>

#include <array>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "globals.h"

class Input {
public:
    explicit Input(GLFWwindow* window = nullptr);
    ~Input() = default;

    void setWindow(GLFWwindow* window);

    void reset();
    void update();

    bool isKeyDown(int key) const;
    bool isKeyUp(int key) const;
    bool isKeyHeld(int key) const;

    const std::map<std::string, bool>& getMouseButtons() const;
    std::pair<int, int> getMousePos() const;

    bool shouldQuit() const { return quit; }

    void select(ID id);
    void deselect();
    bool hasSelection() const { return selectedID >= 0; }
    ID getSelectedID() const { return selectedID; }

private:
    GLFWwindow* window{nullptr};
    bool quit{false};
    std::pair<int, int> mousePos{0, 0};
    std::map<std::string, bool> mouseButtons;
    std::vector<int> keysHeld;
    std::vector<int> keysDown;
    std::vector<int> keysUp;
    std::array<bool, GLFW_KEY_LAST + 1> keyState{};

    bool isSelecting{false};
    ID selectedID{-1};
};

#endif // INPUT_H