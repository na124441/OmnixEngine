#pragma once

#include <string>
#include <vector>
#include <cstdint>

enum class DeviceType {
    Keyboard,
    Mouse,
    Gamepad,
    Unknown
};

// REMOVE the forward declaration - we'll include InputEvent.h where needed
// struct InputEvent;

class InputDevice {
protected:
    uint32_t deviceID;
    DeviceType type;
public:
    InputDevice(uint32_t id, DeviceType devType)
        : deviceID(id), type(devType) {}

    virtual ~InputDevice() = default;

    virtual void UpdateState() = 0;
    virtual void GenerateEvents(std::vector<struct InputEvent>& outEvents) = 0;  // Use struct forward decl
    virtual std::string GetDeviceName() const { return "InputDevice"; }

    DeviceType GetDeviceType() const { return type; }
    uint32_t GetDeviceID() const { return deviceID; }
};
