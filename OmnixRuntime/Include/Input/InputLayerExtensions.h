#pragma once

#include "Input/InputManager.h"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <algorithm>
#include <cmath>

namespace eng::input {

    // -------------------------------------------------------------------------
    // 1. Keyboard Subsystem
    // -------------------------------------------------------------------------
    struct KeyboardDeviceState {
        bool keyCurrent[512] = { false };
        bool keyPrevious[512] = { false };

        void SetKeyState(int keyCode, bool pressed) {
            if (keyCode >= 0 && keyCode < 512) {
                keyCurrent[keyCode] = pressed;
            }
        }

        void UpdateFrame() {
            for (int i = 0; i < 512; ++i) {
                keyPrevious[i] = keyCurrent[i];
            }
        }

        bool IsKeyDown(int keyCode) const {
            return (keyCode >= 0 && keyCode < 512) ? keyCurrent[keyCode] : false;
        }

        bool IsKeyPressed(int keyCode) const {
            return (keyCode >= 0 && keyCode < 512) ? (keyCurrent[keyCode] && !keyPrevious[keyCode]) : false;
        }

        bool IsKeyReleased(int keyCode) const {
            return (keyCode >= 0 && keyCode < 512) ? (!keyCurrent[keyCode] && keyPrevious[keyCode]) : false;
        }
    };

    // -------------------------------------------------------------------------
    // 2. Mouse Subsystem
    // -------------------------------------------------------------------------
    struct MouseDeviceState {
        glm::vec2 position{ 0.0f, 0.0f };
        glm::vec2 previousPosition{ 0.0f, 0.0f };
        glm::vec2 delta{ 0.0f, 0.0f };
        float scrollWheelX = 0.0f;
        float scrollWheelY = 0.0f;
        bool buttonCurrent[8] = { false };
        bool buttonPrevious[8] = { false };

        void SetPosition(float x, float y) {
            position = { x, y };
        }

        void SetButtonState(int buttonCode, bool pressed) {
            if (buttonCode >= 0 && buttonCode < 8) {
                buttonCurrent[buttonCode] = pressed;
            }
        }

        void AddScrollDelta(float dx, float dy) {
            scrollWheelX += dx;
            scrollWheelY += dy;
        }

        void UpdateFrame() {
            delta = position - previousPosition;
            previousPosition = position;
            for (int i = 0; i < 8; ++i) {
                buttonPrevious[i] = buttonCurrent[i];
            }
            scrollWheelX = 0.0f;
            scrollWheelY = 0.0f;
        }

        bool IsButtonDown(int buttonCode) const {
            return (buttonCode >= 0 && buttonCode < 8) ? buttonCurrent[buttonCode] : false;
        }

        bool IsButtonPressed(int buttonCode) const {
            return (buttonCode >= 0 && buttonCode < 8) ? (buttonCurrent[buttonCode] && !buttonPrevious[buttonCode]) : false;
        }
    };

    // -------------------------------------------------------------------------
    // 3. Gamepad Subsystem (Up to 4 Controllers)
    // -------------------------------------------------------------------------
    struct GamepadState {
        bool connected = false;
        std::string name = "Xbox Controller";
        glm::vec2 leftStick{ 0.0f, 0.0f };
        glm::vec2 rightStick{ 0.0f, 0.0f };
        float leftTrigger = 0.0f;
        float rightTrigger = 0.0f;
        uint32_t buttonBitmask = 0;
        uint32_t prevButtonBitmask = 0;

        bool IsButtonDown(uint32_t buttonMask) const {
            return connected && ((buttonBitmask & buttonMask) != 0);
        }

        bool IsButtonPressed(uint32_t buttonMask) const {
            return connected && ((buttonBitmask & buttonMask) != 0) && ((prevButtonBitmask & buttonMask) == 0);
        }
    };

    class GamepadSubsystem {
    public:
        GamepadSubsystem() {
            for (int i = 0; i < 4; ++i) {
                m_Gamepads[i].connected = (i == 0); // Controller 0 connected by default
            }
        }

        GamepadState& GetGamepad(uint32_t index) {
            uint32_t idx = std::min(index, 3u);
            return m_Gamepads[idx];
        }

        const GamepadState& GetGamepad(uint32_t index) const {
            uint32_t idx = std::min(index, 3u);
            return m_Gamepads[idx];
        }

        void UpdateFrame() {
            for (int i = 0; i < 4; ++i) {
                m_Gamepads[i].prevButtonBitmask = m_Gamepads[i].buttonBitmask;
            }
        }

    private:
        GamepadState m_Gamepads[4];
    };

    // -------------------------------------------------------------------------
    // 4. Input Mapping Subsystem
    // -------------------------------------------------------------------------
    struct ActionBinding {
        std::string actionName;
        int keyCode = -1;
        int mouseButton = -1;
        uint32_t gamepadButtonMask = 0;
    };

    struct AxisBinding {
        std::string axisName;
        int positiveKey = -1;
        int negativeKey = -1;
        int axisIndex = -1; // 0: LeftStickX, 1: LeftStickY, 2: RightStickX, 3: RightStickY
        float scale = 1.0f;
        float deadzone = 0.15f;
    };

    class InputMappingTable {
    public:
        void BindAction(const ActionBinding& binding) {
            m_ActionBindings[binding.actionName].push_back(binding);
        }

        void BindAxis(const AxisBinding& binding) {
            m_AxisBindings[binding.axisName].push_back(binding);
        }

        bool EvaluateAction(
            const std::string& name,
            const KeyboardDeviceState& kb,
            const MouseDeviceState& mouse,
            const GamepadState& pad
        ) const {
            auto it = m_ActionBindings.find(name);
            if (it == m_ActionBindings.end()) return false;

            for (const auto& bind : it->second) {
                if (bind.keyCode >= 0 && kb.IsKeyDown(bind.keyCode)) return true;
                if (bind.mouseButton >= 0 && mouse.IsButtonDown(bind.mouseButton)) return true;
                if (bind.gamepadButtonMask != 0 && pad.IsButtonDown(bind.gamepadButtonMask)) return true;
            }
            return false;
        }

        float EvaluateAxis(
            const std::string& name,
            const KeyboardDeviceState& kb,
            const GamepadState& pad
        ) const {
            auto it = m_AxisBindings.find(name);
            if (it == m_AxisBindings.end()) return 0.0f;

            float val = 0.0f;
            for (const auto& bind : it->second) {
                if (bind.positiveKey >= 0 && kb.IsKeyDown(bind.positiveKey)) val += bind.scale;
                if (bind.negativeKey >= 0 && kb.IsKeyDown(bind.negativeKey)) val -= bind.scale;

                if (bind.axisIndex >= 0 && pad.connected) {
                    float rawAxis = 0.0f;
                    if (bind.axisIndex == 0) rawAxis = pad.leftStick.x;
                    else if (bind.axisIndex == 1) rawAxis = pad.leftStick.y;
                    else if (bind.axisIndex == 2) rawAxis = pad.rightStick.x;
                    else if (bind.axisIndex == 3) rawAxis = pad.rightStick.y;

                    if (std::abs(rawAxis) > bind.deadzone) {
                        val += rawAxis * bind.scale;
                    }
                }
            }
            return glm::clamp(val, -1.0f, 1.0f);
        }

    private:
        std::unordered_map<std::string, std::vector<ActionBinding>> m_ActionBindings;
        std::unordered_map<std::string, std::vector<AxisBinding>> m_AxisBindings;
    };

    // -------------------------------------------------------------------------
    // 5. Input Contexts / Actions / Axes Subsystem
    // -------------------------------------------------------------------------
    class InputContext {
    public:
        InputContext(const std::string& name, int priority = 0)
            : m_Name(name), m_Priority(priority) {}

        const std::string& GetName() const { return m_Name; }
        int GetPriority() const { return m_Priority; }
        InputMappingTable& GetMappings() { return m_Mappings; }
        const InputMappingTable& GetMappings() const { return m_Mappings; }

    private:
        std::string m_Name;
        int m_Priority = 0;
        InputMappingTable m_Mappings;
    };

    class InputContextStack {
    public:
        void PushContext(std::shared_ptr<InputContext> context) {
            m_Contexts.push_back(context);
            std::sort(m_Contexts.begin(), m_Contexts.end(), [](const auto& a, const auto& b) {
                return a->GetPriority() > b->GetPriority();
            });
        }

        void PopContext(const std::string& name) {
            m_Contexts.erase(std::remove_if(m_Contexts.begin(), m_Contexts.end(), [&](const auto& ctx) {
                return ctx->GetName() == name;
            }), m_Contexts.end());
        }

        const std::vector<std::shared_ptr<InputContext>>& GetStack() const {
            return m_Contexts;
        }

    private:
        std::vector<std::shared_ptr<InputContext>> m_Contexts;
    };

    // -------------------------------------------------------------------------
    // 6. Hot Plugging & Haptics Subsystem
    // -------------------------------------------------------------------------
    struct ControllerConnectionEvent {
        uint32_t controllerIndex = 0;
        bool connected = false;
        std::string deviceName;
    };

    struct RumbleState {
        float lowFrequency = 0.0f;  // Heavy motor
        float highFrequency = 0.0f; // Light motor
        float durationSeconds = 0.0f;
    };

    class InputHapticsSystem {
    public:
        using HotPlugCallback = std::function<void(const ControllerConnectionEvent&)>;

        void SetHotPlugCallback(HotPlugCallback cb) {
            m_Callback = cb;
        }

        void TriggerConnectionEvent(uint32_t controllerIdx, bool connected, const std::string& name) {
            ControllerConnectionEvent evt{ controllerIdx, connected, name };
            if (m_Callback) m_Callback(evt);
        }

        void SetRumble(uint32_t controllerIdx, float lowFreq, float highFreq, float durationSec) {
            if (controllerIdx < 4) {
                m_Rumble[controllerIdx] = { lowFreq, highFreq, durationSec };
            }
        }

        const RumbleState& GetRumble(uint32_t controllerIdx) const {
            static RumbleState empty{};
            return (controllerIdx < 4) ? m_Rumble[controllerIdx] : empty;
        }

        void Update(float dt) {
            for (int i = 0; i < 4; ++i) {
                if (m_Rumble[i].durationSeconds > 0.0f) {
                    m_Rumble[i].durationSeconds = std::max(0.0f, m_Rumble[i].durationSeconds - dt);
                    if (m_Rumble[i].durationSeconds == 0.0f) {
                        m_Rumble[i].lowFrequency = 0.0f;
                        m_Rumble[i].highFrequency = 0.0f;
                    }
                }
            }
        }

    private:
        HotPlugCallback m_Callback;
        RumbleState m_Rumble[4];
    };

} // namespace eng::input
