#pragma once

#include "Input/InputDevice.h"
#include "Input/InputEvent.h"  // ✅ ADD THIS
#include <array>

#include "ThirdParty/imgui/imgui.h"

inline ImGuiKey GetImGuiKeyFromKeyCode(int keyCode) {
    if (keyCode >= 65 && keyCode <= 90) { // A to Z
        return static_cast<ImGuiKey>(ImGuiKey_A + (keyCode - 65));
    }
    if (keyCode >= 48 && keyCode <= 57) { // 0 to 9
        return static_cast<ImGuiKey>(ImGuiKey_0 + (keyCode - 48));
    }
    switch (keyCode) {
        case 32: return ImGuiKey_Space;
        case 27: return ImGuiKey_Escape;
        case 13: return ImGuiKey_Enter;
        case 16: return ImGuiKey_LeftShift;
        case 17: return ImGuiKey_LeftCtrl;
        case 18: return ImGuiKey_LeftAlt;
        default: return ImGuiKey_None;
    }
}

class KeyboardInput : public InputDevice {
private:
    static constexpr int NUM_KEYS = 256;
    std::array<bool, NUM_KEYS> keyCurrent{};
    std::array<bool, NUM_KEYS> keyPrevious{};

public:
    KeyboardInput(uint32_t id)
        : InputDevice(id, DeviceType::Keyboard) {}

    void UpdateState() override {
        if (ImGui::GetCurrentContext() != nullptr) {
            for (int i = 0; i < NUM_KEYS; ++i) {
                ImGuiKey imguiKey = GetImGuiKeyFromKeyCode(i);
                if (imguiKey != ImGuiKey_None) {
                    keyCurrent[i] = ImGui::IsKeyDown(imguiKey);
                }
            }
        }
    }

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
