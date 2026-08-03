#include "Core/Platform/Input.h"

namespace eng::platform {

    Input& Input::Instance() {
        static Input instance;
        return instance;
    }

    bool Input::IsKeyDown(Key key) const {
        size_t index = static_cast<size_t>(key);
        if (index >= static_cast<size_t>(Key::Count)) return false;
        return (keyboard_.bits[index / 8] & (1 << (index % 8))) != 0;
    }

    bool Input::WasKeyPressed(Key key) const {
        size_t index = static_cast<size_t>(key);
        if (index >= static_cast<size_t>(Key::Count)) return false;
        bool cur = (keyboard_.bits[index / 8] & (1 << (index % 8))) != 0;
        bool prev = (keyboardPrev_.bits[index / 8] & (1 << (index % 8))) != 0;
        return cur && !prev;
    }

    bool Input::WasKeyReleased(Key key) const {
        size_t index = static_cast<size_t>(key);
        if (index >= static_cast<size_t>(Key::Count)) return false;
        bool cur = (keyboard_.bits[index / 8] & (1 << (index % 8))) != 0;
        bool prev = (keyboardPrev_.bits[index / 8] & (1 << (index % 8))) != 0;
        return !cur && prev;
    }

    bool Input::IsMouseButtonDown(MouseButton button) const {
        size_t index = static_cast<size_t>(button);
        if (index >= static_cast<size_t>(MouseButton::Count)) return false;
        return mouse_.buttons[index];
    }

    bool Input::WasMouseButtonPressed(MouseButton button) const {
        size_t index = static_cast<size_t>(button);
        if (index >= static_cast<size_t>(MouseButton::Count)) return false;
        return mouse_.buttons[index] && !mousePrev_.buttons[index];
    }

    bool Input::WasMouseButtonReleased(MouseButton button) const {
        size_t index = static_cast<size_t>(button);
        if (index >= static_cast<size_t>(MouseButton::Count)) return false;
        return !mouse_.buttons[index] && mousePrev_.buttons[index];
    }

    const MouseState& Input::GetMouseState() const {
        return mouse_;
    }

    const KeyboardState& Input::GetKeyboardState() const {
        return keyboard_;
    }

    bool Input::IsGamepadConnected(uint32_t index) const {
        if (index >= 4) return false;
        return gamepadConnected_[index];
    }

    const GamepadState& Input::GetGamepadState(uint32_t index) const {
        static GamepadState emptyState{};
        if (index >= 4) return emptyState;
        return gamepads_[index];
    }

    void Input::SetKeyState(Key key, bool down) {
        size_t index = static_cast<size_t>(key);
        if (index >= static_cast<size_t>(Key::Count)) return;
        if (down) {
            keyboard_.bits[index / 8] |= (1 << (index % 8));
        } else {
            keyboard_.bits[index / 8] &= ~(1 << (index % 8));
        }
    }

    void Input::SetMouseButtonState(MouseButton button, bool down) {
        size_t index = static_cast<size_t>(button);
        if (index >= static_cast<size_t>(MouseButton::Count)) return;
        mouse_.buttons[index] = down;
    }

    void Input::SetMousePosition(int32_t x, int32_t y) {
        mouse_.x = x;
        mouse_.y = y;
    }

    void Input::AddMouseDelta(int32_t dx, int32_t dy) {
        mouse_.deltaX += dx;
        mouse_.deltaY += dy;
    }

    void Input::AddWheelDelta(float delta) {
        mouse_.wheelDelta += delta;
    }

    void Input::SetGamepadState(uint32_t index, const GamepadState& state) {
        if (index >= 4) return;
        gamepads_[index] = state;
    }

    void Input::SetGamepadConnected(uint32_t index, bool connected) {
        if (index >= 4) return;
        gamepadConnected_[index] = connected;
    }

    void Input::EndFrame() {
        keyboardPrev_ = keyboard_;
        mousePrev_ = mouse_;
        
        mouse_.deltaX = 0;
        mouse_.deltaY = 0;
        mouse_.wheelDelta = 0.0f;
    }

} // namespace eng::platform
