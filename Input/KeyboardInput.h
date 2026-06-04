#pragma once

#include "InputDevice.h"
#include "InputEvent.h"  // ✅ ADD THIS
#include <array>

class KeyboardInput : public InputDevice {
private:
    static constexpr int NUM_KEYS = 256;
    std::array<bool, NUM_KEYS> keyCurrent{};
    std::array<bool, NUM_KEYS> keyPrevious{};

public:
    KeyboardInput(uint32_t id)
        : InputDevice(id, DeviceType::Keyboard) {}

    void UpdateState() override {}

    void GenerateEvents(std::vector<InputEvent>& outEvents) override {
        for (int i = 0; i < NUM_KEYS; ++i) {
            if (!keyPrevious[i] && keyCurrent[i]) {
                outEvents.push_back(InputEvent::CreateKeyDownEvent(deviceID, i));
            }
            else if (keyPrevious[i] && !keyCurrent[i]) {
                outEvents.push_back(InputEvent::CreateKeyUpEvent(deviceID, i));
            }
            keyPrevious[i] = keyCurrent[i];
        }
    }

    std::string GetDeviceName() const override { return "Keyboard"; }
};
