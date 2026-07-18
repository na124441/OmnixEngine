#pragma once

#include "Input/InputDevice.h"
#include "Input/InputEvent.h"  // ✅ ADD THIS
#include <cmath>

class GamepadInput : public InputDevice {
private:
    static constexpr int NUM_BUTTONS = 16;
    static constexpr float DEADZONE = 0.15f;

    float leftStickX = 0.0f, leftStickY = 0.0f;
    float rightStickX = 0.0f, rightStickY = 0.0f;
    float leftTrigger = 0.0f, rightTrigger = 0.0f;

    float prevLeftStickX = 0.0f, prevLeftStickY = 0.0f;
    float prevRightStickX = 0.0f, prevRightStickY = 0.0f;
    float prevLeftTrigger = 0.0f, prevRightTrigger = 0.0f;

    bool buttonCurrent[NUM_BUTTONS] = {};
    bool buttonPrevious[NUM_BUTTONS] = {};
    bool isConnected = false;

public:
    GamepadInput(uint32_t id)
        : InputDevice(id, DeviceType::Gamepad)
    {
        std::fill(buttonCurrent, buttonCurrent + NUM_BUTTONS, false);
        std::fill(buttonPrevious, buttonPrevious + NUM_BUTTONS, false);
    }

    void UpdateState() override {}

    void GenerateEvents(std::vector<InputEvent>& outEvents) override {
        if (!isConnected) return;

        for (int i = 0; i < NUM_BUTTONS; ++i) {
            if (!buttonPrevious[i] && buttonCurrent[i]) {
                outEvents.push_back(InputEvent::CreateGamepadButtonDownEvent(deviceID, i));
            }
            else if (buttonPrevious[i] && !buttonCurrent[i]) {
                outEvents.push_back(InputEvent::CreateGamepadButtonUpEvent(deviceID, i));
            }
            buttonPrevious[i] = buttonCurrent[i];
        }

        bool leftStickMoved = (std::fabs(leftStickX - prevLeftStickX) > DEADZONE) ||
                              (std::fabs(leftStickY - prevLeftStickY) > DEADZONE);
        bool rightStickMoved = (std::fabs(rightStickX - prevRightStickX) > DEADZONE) ||
                               (std::fabs(rightStickY - prevRightStickY) > DEADZONE);

        if (leftStickMoved || rightStickMoved) {
            outEvents.push_back(InputEvent::CreateGamepadAxisEvent(
                deviceID, leftStickX, leftStickY, rightStickX, rightStickY, leftTrigger, rightTrigger
            ));
            prevLeftStickX = leftStickX;
            prevLeftStickY = leftStickY;
            prevRightStickX = rightStickX;
            prevRightStickY = rightStickY;
        }

        if (std::fabs(leftTrigger - prevLeftTrigger) > 0.05f ||
            std::fabs(rightTrigger - prevRightTrigger) > 0.05f) {
            outEvents.push_back(InputEvent::CreateGamepadTriggerEvent(deviceID, leftTrigger, rightTrigger));
            prevLeftTrigger = leftTrigger;
            prevRightTrigger = rightTrigger;
        }
    }

    std::string GetDeviceName() const override { return "Gamepad"; }
    bool IsConnected() const { return isConnected; }
};
