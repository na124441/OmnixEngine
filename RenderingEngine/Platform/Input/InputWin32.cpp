/*******************************************************************************************************************
 * @file  InputWin32.cpp
 * @brief Win32-specific implementation of the input system.
 *
 *        This file implements the actual OS-level input handling for Windows.
 *        It translates Windows messages into engine-compatible InputEvents
 *        and updates the internal device states.
 *
 *        The implementation works with the existing InputManager system by
 *        providing concrete implementations of the abstract InputDevice classes.
 *
 *        © 2024 Your Engine Project – all rights reserved.
 *******************************************************************************************************************/

#include "Input.h"
#include <windows.h>
#include <iostream>
#include <algorithm>

namespace eng::platform {

    // ------------------------------------------------------------------------------------------------
    // Keyboard Input Implementation
    // ------------------------------------------------------------------------------------------------

    void KeyboardInput::UpdateState() {
        // Copy current to previous
        std::copy(std::begin(keyStates), std::end(keyStates), std::begin(prevKeyStates));

        // Update current state from Windows (you would typically get this from WM_KEYDOWN/WM_KEYUP)
        // For now, we'll simulate this - in reality this would be called from the Window proc
        // This is just a placeholder - the real implementation would be event-driven
    }

    void KeyboardInput::GenerateEvents(std::vector<InputEvent>& eventBuffer) {
        for (size_t i = 0; i < KEY_COUNT; ++i) {
            if (keyStates[i] && !prevKeyStates[i]) {
                eventBuffer.emplace_back(InputEvent::KeyDown, deviceId, static_cast<int32_t>(i));
            }
            else if (!keyStates[i] && prevKeyStates[i]) {
                eventBuffer.emplace_back(InputEvent::KeyUp, deviceId, static_cast<int32_t>(i));
            }
        }
    }

    bool KeyboardInput::IsKeyDown(int32_t keyCode) const {
        if (keyCode >= 0 && keyCode < static_cast<int32_t>(KEY_COUNT)) {
            return keyStates[keyCode];
        }
        return false;
    }

    void KeyboardInput::SetKeyState(int32_t keyCode, bool pressed) {
        if (keyCode >= 0 && keyCode < static_cast<int32_t>(KEY_COUNT)) {
            keyStates[keyCode] = pressed;
        }
    }

    // ------------------------------------------------------------------------------------------------
    // Mouse Input Implementation
    // ------------------------------------------------------------------------------------------------

    void MouseInput::UpdateState() {
        previousState = currentState;

        // In a real implementation, this would be updated from Windows messages
        // For now, we just preserve the state for event generation
    }

    void MouseInput::GenerateEvents(std::vector<InputEvent>& eventBuffer) {
        // Generate mouse move event if position changed
        if (currentState.x != previousState.x || currentState.y != previousState.y) {
            eventBuffer.emplace_back(InputEvent::MouseMove, deviceId, 0, 0.0f,
                currentState.x, currentState.y);
        }

        // Generate button events
        for (int i = 0; i < 5; ++i) {
            if (currentState.buttons[i] && !previousState.buttons[i]) {
                eventBuffer.emplace_back(InputEvent::MouseButtonDown, deviceId, i);
            }
            else if (!currentState.buttons[i] && previousState.buttons[i]) {
                eventBuffer.emplace_back(InputEvent::MouseButtonUp, deviceId, i);
            }
        }

        // Generate wheel event if scrolled
        if (currentState.wheelDelta != 0.0f) {
            eventBuffer.emplace_back(InputEvent::MouseWheel, deviceId, 0, currentState.wheelDelta);
            currentState.wheelDelta = 0.0f; // Reset after processing
        }
    }

    void MouseInput::SetPosition(int32_t x, int32_t y) {
        currentState.x = x;
        currentState.y = y;
    }

    void MouseInput::SetButtonState(int32_t button, bool pressed) {
        if (button >= 0 && button < 5) {
            currentState.buttons[button] = pressed;
        }
    }

    void MouseInput::AddMouseDelta(int32_t dx, int32_t dy) {
        currentState.deltaX += dx;
        currentState.deltaY += dy;
    }

    void MouseInput::AddWheelDelta(float delta) {
        currentState.wheelDelta += delta;
    }

    // ------------------------------------------------------------------------------------------------
    // Gamepad Input Implementation
    // ------------------------------------------------------------------------------------------------

    void GamepadInput::UpdateState() {
        previousState = currentState;

        // In a real implementation, this would poll XInput or DirectInput
        // For now, we just preserve the state for event generation
    }

    void GamepadInput::GenerateEvents(std::vector<InputEvent>& eventBuffer) {
        // Generate button events
        for (int i = 0; i < 16; ++i) {
            if (currentState.buttons[i] && !previousState.buttons[i]) {
                eventBuffer.emplace_back(InputEvent::GamepadButtonDown, deviceId, i);
            }
            else if (!currentState.buttons[i] && previousState.buttons[i]) {
                eventBuffer.emplace_back(InputEvent::GamepadButtonUp, deviceId, i);
            }
        }

        // Generate axis events (optional - could also just expose state directly)
        for (int i = 0; i < 6; ++i) {
            if (currentState.axes[i] != previousState.axes[i]) {
                eventBuffer.emplace_back(InputEvent::GamepadAxis, deviceId, i, currentState.axes[i]);
            }
        }
    }

    void GamepadInput::SetButtonState(int32_t button, bool pressed) {
        if (button >= 0 && button < 16) {
            currentState.buttons[button] = pressed;
        }
    }

    void GamepadInput::SetAxisState(int32_t axis, float value) {
        if (axis >= 0 && axis < 6) {
            currentState.axes[axis] = value;
        }
    }

    // ------------------------------------------------------------------------------------------------
    // InputManager Implementation
    // ------------------------------------------------------------------------------------------------

    namespace {
        InputManager g_InputManager;
    }

    InputManager& GetInputManager() {
        return g_InputManager;
    }

    InputManager::InputManager() = default;
    InputManager::~InputManager() = default;

    void InputManager::Initialize() {
        auto keyboard = std::make_unique<KeyboardInput>(0);
        auto mouse = std::make_unique<MouseInput>(1);
        auto gamepad = std::make_unique<GamepadInput>(2);

        keyboardDevice = keyboard.get();
        mouseDevice = mouse.get();
        gamepadDevice = gamepad.get();

        devices.push_back(std::move(keyboard));
        devices.push_back(std::move(mouse));
        devices.push_back(std::move(gamepad));

        std::cout << "[InputManager] Initialized with 3 devices" << std::endl;
    }

    void InputManager::LoadBindingsFromConfig(const std::string& configPath) {
        // If configPath is provided, load from file
        if (!configPath.empty()) {
            // TODO: Implement config file loading
            std::cout << "[InputManager] Loading bindings from: " << configPath << std::endl;
        }

        // Default bindings
        bindings = {
            InputBinding("Jump", DeviceType::Keyboard, InputEvent::Type::KeyDown, 32),        // Space
            InputBinding("MoveLeft", DeviceType::Keyboard, InputEvent::Type::KeyDown, 65),    // A
            InputBinding("MoveRight", DeviceType::Keyboard, InputEvent::Type::KeyDown, 68),   // D
            InputBinding("MoveUp", DeviceType::Keyboard, InputEvent::Type::KeyDown, 87),     // W
            InputBinding("MoveDown", DeviceType::Keyboard, InputEvent::Type::KeyDown, 83),    // S
            InputBinding("Fire", DeviceType::Mouse, InputEvent::Type::MouseButtonDown, 0),    // Left mouse
            InputBinding("Pause", DeviceType::Keyboard, InputEvent::Type::KeyDown, 27),       // Escape
        };
        std::cout << "[InputManager] Loaded " << bindings.size() << " bindings" << std::endl;
    }

    void InputManager::Update() {
        eventBuffer.clear();
        PollAllDevices();
        ProcessEvents();
        DispatchActions();
        ClearOneFrameActionStates();
    }

    void InputManager::PollAllDevices() {
        for (auto& device : devices) {
            device->UpdateState();
            device->GenerateEvents(eventBuffer);
        }
    }

    void InputManager::ProcessEvents() {
        for (const InputEvent& event : eventBuffer) {
            DeviceType eventDeviceType = GetDeviceTypeFromEvent(event.deviceID);

            for (const InputBinding& binding : bindings) {
                if (binding.deviceType != eventDeviceType) continue;
                if (binding.Matches(event)) {
                    actionState[binding.actionName] = true;
                }
            }
        }
    }

    void InputManager::DispatchActions() {
        for (const auto& [actionName, fired] : actionState) {
            if (fired && actionCallbacks.find(actionName) != actionCallbacks.end()) {
                for (const auto& callback : actionCallbacks[actionName]) {
                    callback(actionName);
                }
            }
        }
    }

    void InputManager::ClearOneFrameActionStates() {
        actionStateHeld = actionState;
        for (auto& [actionName, fired] : actionState) {
            fired = false;
        }
    }

    bool InputManager::IsActionPressed(const std::string& actionName) const {
        auto it = actionState.find(actionName);
        if (it != actionState.end()) {
            auto heldIt = actionStateHeld.find(actionName);
            bool wasHeld = (heldIt != actionStateHeld.end()) && heldIt->second;
            return it->second && !wasHeld;
        }
        return false;
    }

    bool InputManager::IsActionHeld(const std::string& actionName) const {
        auto it = actionStateHeld.find(actionName);
        return (it != actionStateHeld.end()) && it->second;
    }

    void InputManager::AddBinding(const InputBinding& binding) {
        bindings.push_back(binding);
    }

    void InputManager::RemoveBinding(const std::string& actionName) {
        auto it = std::remove_if(bindings.begin(), bindings.end(),
            [&actionName](const InputBinding& b) { return b.actionName == actionName; });
        bindings.erase(it, bindings.end());
    }

    void InputManager::ClearBindings() {
        bindings.clear();
    }

    void InputManager::RebindAction(const std::string& actionName,
        DeviceType device,
        InputEvent::Type eventType,
        int32_t code) {
        RemoveBinding(actionName);
        AddBinding(InputBinding(actionName, device, eventType, code));
    }

    void InputManager::SubscribeToAction(const std::string& actionName, ActionCallback callback) {
        actionCallbacks[actionName].push_back(callback);
    }

    void InputManager::UnsubscribeFromAction(const std::string& actionName) {
        actionCallbacks.erase(actionName);
    }

    void InputManager::PrintBindings() const {
        std::cout << "\n========== Input Bindings ==========" << std::endl;
        for (const auto& binding : bindings) {
            std::cout << binding.ToString() << std::endl;
        }
        std::cout << "==================================\n" << std::endl;
    }

    void InputManager::PrintActionState() const {
        std::cout << "\n========== Action State ==========" << std::endl;
        for (const auto& [action, state] : actionState) {
            std::cout << action << ": " << (state ? "ACTIVE" : "inactive") << std::endl;
        }
        std::cout << "================================\n" << std::endl;
    }

    std::string InputManager::GetActionState(const std::string& actionName) const {
        if (actionState.find(actionName) != actionState.end()) {
            return actionState.at(actionName) ? "PRESSED" : "released";
        }
        return "NOT_FOUND";
    }

    DeviceType InputManager::GetDeviceTypeFromEvent(uint32_t deviceID) const {
        if (deviceID == 0) return DeviceType::Keyboard;
        if (deviceID == 1) return DeviceType::Mouse;
        if (deviceID == 2) return DeviceType::Gamepad;
        return DeviceType::Unknown;
    }

} // namespace eng::platform
