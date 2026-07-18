// ============================================================================
// InputEventTypes.h - Input-Related Event Classes
// ============================================================================

#pragma once

#include "EventManagement/GameEvent.h"

namespace Omnix {

// ============================================================================
// BASE INPUT EVENT CLASS
// ============================================================================

class InputEvent : public GameEvent {
protected:
    explicit InputEvent(uint8_t priority = 128)
        : GameEvent(priority) {}
};

// ============================================================================
// KEY PRESS EVENT
// ============================================================================

class KeyPressEvent : public InputEvent {
public:
    KeyPressEvent(int keyCode, bool repeat = false, uint8_t priority = 128)
        : InputEvent(priority), keyCode(keyCode), repeat(repeat) {}

    DEFINE_EVENT_TYPE(KeyPressEvent, EventType::INPUT_KEY_PRESS, "KeyPress")

    int getKeyCode() const { return keyCode; }
    bool isRepeat() const { return repeat; }

private:
    int keyCode;
    bool repeat;
};

// ============================================================================
// KEY RELEASE EVENT
// ============================================================================

class KeyReleaseEvent : public InputEvent {
public:
    explicit KeyReleaseEvent(int keyCode, uint8_t priority = 128)
        : InputEvent(priority), keyCode(keyCode) {}

    DEFINE_EVENT_TYPE(KeyReleaseEvent, EventType::INPUT_KEY_RELEASE, "KeyRelease")

    int getKeyCode() const { return keyCode; }

private:
    int keyCode;
};

// ============================================================================
// MOUSE MOVE EVENT
// ============================================================================

class MouseMoveEvent : public InputEvent {
public:
    MouseMoveEvent(int x, int y, int deltaX = 0, int deltaY = 0,
                   uint8_t priority = 128)
        : InputEvent(priority), x(x), y(y), deltaX(deltaX), deltaY(deltaY) {}

    DEFINE_EVENT_TYPE(MouseMoveEvent, EventType::INPUT_MOUSE_MOVE, "MouseMove")

    int getX() const { return x; }
    int getY() const { return y; }
    int getDeltaX() const { return deltaX; }
    int getDeltaY() const { return deltaY; }

private:
    int x, y;
    int deltaX, deltaY;
};

// ============================================================================
// MOUSE CLICK EVENT
// ============================================================================

class MouseClickEvent : public InputEvent {
public:
    enum class Button : uint8_t {
        LEFT = 0,
        RIGHT = 1,
        MIDDLE = 2,
        UNKNOWN = 255
    };

    MouseClickEvent(Button button, bool pressed, int x, int y,
                    uint8_t priority = 128)
        : InputEvent(priority), button(button), pressed(pressed), x(x), y(y) {}

    DEFINE_EVENT_TYPE(MouseClickEvent, EventType::INPUT_MOUSE_CLICK, "MouseClick")

    Button getButton() const { return button; }
    bool isPressed() const { return pressed; }
    int getX() const { return x; }
    int getY() const { return y; }

private:
    Button button;
    bool pressed;
    int x, y;
};

} // namespace Omnix
