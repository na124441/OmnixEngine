#pragma once

#include <string>
#include <cstdint>
#include "InputDevice.h"
#include "InputEvent.h"  // ✅ MUST INCLUDE FULL DEFINITION

struct InputBinding {
    std::string actionName;
    DeviceType deviceType;
    InputEvent::Type eventType;
    int code;

    InputBinding() = default;

    InputBinding(const std::string& action,
                 DeviceType device,
                 InputEvent::Type event,
                 int eventCode)
        : actionName(action), deviceType(device), eventType(event), code(eventCode) {}

    bool Matches(const InputEvent& event) const {
        if (event.eventType != eventType) return false;

        if (code >= 0) {
            switch (event.eventType) {
                case InputEvent::Type::KeyDown:
                case InputEvent::Type::KeyUp:
                    return event.payload.keyboard.keyCode == code;
                case InputEvent::Type::MouseButtonDown:
                case InputEvent::Type::MouseButtonUp:
                    return event.payload.mouseButton.button == code;
                case InputEvent::Type::GamepadButtonDown:
                case InputEvent::Type::GamepadButtonUp:
                    return event.payload.gamepadButton.button == code;
                default:
                    return true;
            }
        }
        return true;
    }

    std::string ToString() const {
        return "Binding[" + actionName + "] -> " + std::to_string(static_cast<int>(eventType)) +
               " (code: " + std::to_string(code) + ")";
    }
};
