#pragma once

#include <cstdint>
#include <chrono>

enum class DeviceType;

struct InputEvent {
    enum class Type {
        // Keyboard events
        KeyDown,
        KeyUp,

        // Mouse events
        MouseMove,
        MouseButtonDown,
        MouseButtonUp,
        MouseScroll,

        // Gamepad events
        GamepadButtonDown,
        GamepadButtonUp,
        GamepadAxis,
        GamepadTrigger,

        // Sentinel
        Unknown
    };

    Type eventType = Type::Unknown;
    uint32_t deviceID = 0;
    uint64_t timestamp = 0;  // Milliseconds since epoch or frame start

    // Payload union for event-specific data
    union {
        struct {
            int keyCode;
        } keyboard;

        struct {
            float deltaX, deltaY;
            float posX, posY;
        } mouseMove;

        struct {
            int button;  // 0=Left, 1=Middle, 2=Right
        } mouseButton;

        struct {
            float scrollDelta;
        } mouseScroll;

        struct {
            int button;  // 0-15 for gamepad buttons
        } gamepadButton;

        struct {
            float leftStickX, leftStickY;
            float rightStickX, rightStickY;
            float leftTrigger, rightTrigger;
        } gamepadAxis;

        struct {
            float leftTrigger;
            float rightTrigger;
        } gamepadTrigger;
    } payload;

    // Factory functions for creating events
    static InputEvent CreateKeyDownEvent(uint32_t deviceID, int keyCode) {
        InputEvent e;
        e.eventType = Type::KeyDown;
        e.deviceID = deviceID;
        e.timestamp = GetCurrentTimestamp();
        e.payload.keyboard.keyCode = keyCode;
        return e;
    }

    static InputEvent CreateKeyUpEvent(uint32_t deviceID, int keyCode) {
        InputEvent e;
        e.eventType = Type::KeyUp;
        e.deviceID = deviceID;
        e.timestamp = GetCurrentTimestamp();
        e.payload.keyboard.keyCode = keyCode;
        return e;
    }

    static InputEvent CreateMouseMoveEvent(uint32_t deviceID, float deltaX, float deltaY, float posX, float posY) {
        InputEvent e;
        e.eventType = Type::MouseMove;
        e.deviceID = deviceID;
        e.timestamp = GetCurrentTimestamp();
        e.payload.mouseMove.deltaX = deltaX;
        e.payload.mouseMove.deltaY = deltaY;
        e.payload.mouseMove.posX = posX;
        e.payload.mouseMove.posY = posY;
        return e;
    }

    static InputEvent CreateMouseButtonDownEvent(uint32_t deviceID, int button) {
        InputEvent e;
        e.eventType = Type::MouseButtonDown;
        e.deviceID = deviceID;
        e.timestamp = GetCurrentTimestamp();
        e.payload.mouseButton.button = button;
        return e;
    }

    static InputEvent CreateMouseButtonUpEvent(uint32_t deviceID, int button) {
        InputEvent e;
        e.eventType = Type::MouseButtonUp;
        e.deviceID = deviceID;
        e.timestamp = GetCurrentTimestamp();
        e.payload.mouseButton.button = button;
        return e;
    }

    static InputEvent CreateMouseScrollEvent(uint32_t deviceID, float scrollDelta) {
        InputEvent e;
        e.eventType = Type::MouseScroll;
        e.deviceID = deviceID;
        e.timestamp = GetCurrentTimestamp();
        e.payload.mouseScroll.scrollDelta = scrollDelta;
        return e;
    }

    static InputEvent CreateGamepadButtonDownEvent(uint32_t deviceID, int button) {
        InputEvent e;
        e.eventType = Type::GamepadButtonDown;
        e.deviceID = deviceID;
        e.timestamp = GetCurrentTimestamp();
        e.payload.gamepadButton.button = button;
        return e;
    }

    static InputEvent CreateGamepadButtonUpEvent(uint32_t deviceID, int button) {
        InputEvent e;
        e.eventType = Type::GamepadButtonUp;
        e.deviceID = deviceID;
        e.timestamp = GetCurrentTimestamp();
        e.payload.gamepadButton.button = button;
        return e;
    }

    static InputEvent CreateGamepadAxisEvent(uint32_t deviceID,
                                             float leftStickX, float leftStickY,
                                             float rightStickX, float rightStickY,
                                             float leftTrigger, float rightTrigger) {
        InputEvent e;
        e.eventType = Type::GamepadAxis;
        e.deviceID = deviceID;
        e.timestamp = GetCurrentTimestamp();
        e.payload.gamepadAxis.leftStickX = leftStickX;
        e.payload.gamepadAxis.leftStickY = leftStickY;
        e.payload.gamepadAxis.rightStickX = rightStickX;
        e.payload.gamepadAxis.rightStickY = rightStickY;
        e.payload.gamepadAxis.leftTrigger = leftTrigger;
        e.payload.gamepadAxis.rightTrigger = rightTrigger;
        return e;
    }

    static InputEvent CreateGamepadTriggerEvent(uint32_t deviceID,
                                                float leftTrigger, float rightTrigger) {
        InputEvent e;
        e.eventType = Type::GamepadTrigger;
        e.deviceID = deviceID;
        e.timestamp = GetCurrentTimestamp();
        e.payload.gamepadTrigger.leftTrigger = leftTrigger;
        e.payload.gamepadTrigger.rightTrigger = rightTrigger;
        return e;
    }

private:
    static uint64_t GetCurrentTimestamp() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()
        ).count();
    }
};
