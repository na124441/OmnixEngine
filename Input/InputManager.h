#pragma once

#include <memory>
#include <vector>
#include <map>
#include <string>
#include <functional>

#include "InputDevice.h"
#include "InputEvent.h"      // ✅ ADD THIS
#include "InputBinding.h"
// Remove duplicate struct InputBinding - it's in InputBinding.h

class KeyboardInput;
class MouseInput;
class GamepadInput;

class InputManager {
private:
    std::vector<std::unique_ptr<InputDevice>> devices;
    KeyboardInput* keyboardDevice = nullptr;
    MouseInput* mouseDevice = nullptr;
    GamepadInput* gamepadDevice = nullptr;

    std::vector<InputBinding> bindings;
    std::vector<InputEvent> eventBuffer;
    std::map<std::string, bool> actionState;
    std::map<std::string, bool> actionStateHeld;

    using ActionCallback = std::function<void(const std::string&)>;
    std::map<std::string, std::vector<ActionCallback>> actionCallbacks;

    DeviceType GetDeviceTypeFromEvent(uint32_t deviceID) const;

public:
    InputManager();
    ~InputManager();

    void Initialize();
    void LoadBindingsFromConfig(const std::string& configPath);
    void Update();

    bool IsActionPressed(const std::string& actionName) const;
    bool IsActionHeld(const std::string& actionName) const;

    void AddBinding(const InputBinding& binding);
    void RemoveBinding(const std::string& actionName);
    void ClearBindings();
    void RebindAction(const std::string& actionName,
                      DeviceType device,
                      InputEvent::Type eventType,
                      int code);

    void SubscribeToAction(const std::string& actionName, ActionCallback callback);
    void UnsubscribeFromAction(const std::string& actionName);

    void SetActionStateForTest(const std::string& actionName, bool pressed);

    KeyboardInput* GetKeyboardDevice() { return keyboardDevice; }
    MouseInput* GetMouseDevice() { return mouseDevice; }
    GamepadInput* GetGamepadDevice() { return gamepadDevice; }

    void PrintBindings() const;
    void PrintActionState() const;
    std::string GetActionState(const std::string& actionName) const;

private:
    void PollAllDevices();
    void ProcessEvents();
    void DispatchActions();
    void ClearOneFrameActionStates();
};
