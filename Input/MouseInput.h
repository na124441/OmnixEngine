#pragma once

#include "InputDevice.h"
#include "InputEvent.h"  // ✅ ADD THIS
#include <cmath>

class MouseInput : public InputDevice {
private:
    float posX = 0.0f;
    float posY = 0.0f;
    float deltaX = 0.0f;
    float deltaY = 0.0f;
    bool buttonCurrent[3] = {false, false, false};
    bool buttonPrevious[3] = {false, false, false};
    float scrollDelta = 0.0f;
    float prevPosX = 0.0f;
    float prevPosY = 0.0f;

public:
    MouseInput(uint32_t id)
        : InputDevice(id, DeviceType::Mouse) {}

    void UpdateState() override {}

    void GenerateEvents(std::vector<InputEvent>& outEvents) override {
        deltaX = posX - prevPosX;
        deltaY = posY - prevPosY;

        if (std::fabs(deltaX) > 0.001f || std::fabs(deltaY) > 0.001f) {
            outEvents.push_back(
                InputEvent::CreateMouseMoveEvent(deviceID, deltaX, deltaY, posX, posY)
            );
        }

        for (int i = 0; i < 3; ++i) {
            if (!buttonPrevious[i] && buttonCurrent[i]) {
                outEvents.push_back(InputEvent::CreateMouseButtonDownEvent(deviceID, i));
            }
            else if (buttonPrevious[i] && !buttonCurrent[i]) {
                outEvents.push_back(InputEvent::CreateMouseButtonUpEvent(deviceID, i));
            }
            buttonPrevious[i] = buttonCurrent[i];
        }

        if (std::fabs(scrollDelta) > 0.001f) {
            outEvents.push_back(InputEvent::CreateMouseScrollEvent(deviceID, scrollDelta));
            scrollDelta = 0.0f;
        }

        prevPosX = posX;
        prevPosY = posY;
    }

    std::string GetDeviceName() const override { return "Mouse"; }
};
