#pragma once
#include <cstdint>

namespace eng::platform {

    enum class Key : uint16_t {
        Unknown = 0,
        A, B, C, D, E, F, G, H, I, J, K, L, M,
        N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
        Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
        Escape, Space, Enter, Tab, Backspace,
        LeftShift, RightShift, LeftCtrl, RightCtrl,
        LeftAlt, RightAlt,
        ArrowUp, ArrowDown, ArrowLeft, ArrowRight,
        Count
    };

    enum class MouseButton : uint8_t {
        Left = 0,
        Right,
        Middle,
        XButton1,
        XButton2,
        Count
    };

    enum class GamepadButton : uint16_t {
        A, B, X, Y,
        DPadUp, DPadDown, DPadLeft, DPadRight,
        LeftShoulder, RightShoulder,
        Start, Back,
        LeftStick, RightStick,
        Count
    };

    struct KeyboardState {
        uint8_t bits[static_cast<size_t>(Key::Count) / 8 + 1] = {};
    };

    struct MouseState {
        int32_t x = 0, y = 0;
        int32_t deltaX = 0, deltaY = 0;
        bool    buttons[static_cast<size_t>(MouseButton::Count)] = {};
        float   wheelDelta = 0.0f;
    };

    struct GamepadState {
        float   axes[6] = {};
        bool    buttons[static_cast<size_t>(GamepadButton::Count)] = {};
    };

    class Input {
    public:
        [[nodiscard]] static Input& Instance();

        [[nodiscard]] bool IsKeyDown(Key key) const;
        [[nodiscard]] bool WasKeyPressed(Key key) const;
        [[nodiscard]] bool WasKeyReleased(Key key) const;

        [[nodiscard]] bool IsMouseButtonDown(MouseButton button) const;
        [[nodiscard]] bool WasMouseButtonPressed(MouseButton button) const;
        [[nodiscard]] bool WasMouseButtonReleased(MouseButton button) const;

        [[nodiscard]] const MouseState& GetMouseState() const;
        [[nodiscard]] const KeyboardState& GetKeyboardState() const;

        [[nodiscard]] bool IsGamepadConnected(uint32_t index) const;
        [[nodiscard]] const GamepadState& GetGamepadState(uint32_t index) const;

        void SetKeyState(Key key, bool down);
        void SetMouseButtonState(MouseButton button, bool down);
        void SetMousePosition(int32_t x, int32_t y);
        void AddMouseDelta(int32_t dx, int32_t dy);
        void AddWheelDelta(float delta);

        void SetGamepadState(uint32_t index, const GamepadState& state);
        void SetGamepadConnected(uint32_t index, bool connected);

        void EndFrame();

    private:
        Input() = default;
        ~Input() = default;

        KeyboardState keyboard_;
        MouseState    mouse_;
        GamepadState  gamepads_[4];
        bool          gamepadConnected_[4]{};

        KeyboardState keyboardPrev_;
        MouseState    mousePrev_;
    };

} // namespace eng::platform
