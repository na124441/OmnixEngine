#include "Input/InputManager.h"
#include "Input/KeyboardInput.h"
#include "Input/MouseInput.h"
#include "Input/GamepadInput.h"
#include <iostream>
#include <algorithm>
#include <memory>  // ✅ ADD THIS

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
    LoadBindingsFromConfig("");
}

void InputManager::LoadBindingsFromConfig(const std::string& configPath) {
    bindings = {
        InputBinding("Jump", DeviceType::Keyboard, InputEvent::Type::KeyDown, 32),
        InputBinding("MoveLeft", DeviceType::Keyboard, InputEvent::Type::KeyDown, 65),
        InputBinding("MoveRight", DeviceType::Keyboard, InputEvent::Type::KeyDown, 68),
        InputBinding("MoveUp", DeviceType::Keyboard, InputEvent::Type::KeyDown, 87),
        InputBinding("MoveDown", DeviceType::Keyboard, InputEvent::Type::KeyDown, 83),
        InputBinding("Fire", DeviceType::Mouse, InputEvent::Type::MouseButtonDown, 0),
        InputBinding("Pause", DeviceType::Keyboard, InputEvent::Type::KeyDown, 27),
        InputBinding("Interact", DeviceType::Keyboard, InputEvent::Type::KeyDown, 69), // E key
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
                                 int code) {
    RemoveBinding(actionName);
    AddBinding(InputBinding(actionName, device, eventType, code));
}

void InputManager::SubscribeToAction(const std::string& actionName, ActionCallback callback) {
    actionCallbacks[actionName].push_back(callback);
}

void InputManager::UnsubscribeFromAction(const std::string& actionName) {
    actionCallbacks.erase(actionName);
}

void InputManager::SetActionStateForTest(const std::string& actionName, bool pressed) {
    actionState[actionName] = pressed;
    actionStateHeld[actionName] = false;
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
