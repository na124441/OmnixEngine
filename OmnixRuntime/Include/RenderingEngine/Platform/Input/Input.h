#pragma once
#include <cstdint>
#include "core/types/Result.h"

namespace eng::platform {

    /* ----------------------------------------------------------------------
     * Enumerations – platform‑agnostic key / mouse / gamepad codes.
     * ---------------------------------------------------------------------- */
    enum class Key : uint16_t {
        Unknown = 0,
        A, B, C, D, E, F, G, H, I, J, K, L, M,
        N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
        Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
        Escape, Space, Enter, Tab, Backspace,
        LeftShift, RightShift, LeftCtrl, RightCtrl,
        LeftAlt, RightAlt,
        ArrowUp, ArrowDown, ArrowLeft, ArrowRight,
        // … add as needed, keep sorted for binary search tables
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

    /* ----------------------------------------------------------------------
     * Input state – snapshot of the current frame.
     * ---------------------------------------------------------------------- */
    struct KeyboardState {
        // 1 bit per key – true if down
        uint8_t bits[static_cast<size_t>(Key::Count) / 8 + 1];
    };

    struct MouseState {
        int32_t x = 0, y = 0;                // current cursor position (client coords)
        int32_t deltaX = 0, deltaY = 0;      // movement this frame
        bool    buttons[static_cast<size_t>(MouseButton::Count)] = {};
        float   wheelDelta = 0.0f;           // vertical scroll this frame
    };

    struct GamepadState {
        float   axes[6] = {};                // LStickX,Y, RStickX,Y, LT, RT (range [-1,1] or [0,1])
        bool    buttons[static_cast<size_t>(GamepadButton::Count)] = {};
    };

    /**
     * @class Input
     *
     * A **singleton‑style** API that stores the latest state of all input devices.
     *
     * The OS‑specific implementation updates this class from the Window’s
     * `PollEvents()` call. The game code reads from it during its Update().
     *
     * Thread safety: all mutating calls happen on the owner thread (the same thread
     * that polls events). Reads are also expected on that thread, but they are
     * trivially thread‑safe (plain data members). If a background thread ever
     * needs to read input, it must first copy the current snapshot.
     */
    class Input {
    public:
        // --------------------------------------------------------------------
        // Lifetime (creation is performed by the concrete OS implementation)
        // --------------------------------------------------------------------
        static Input& Instance();               // global accessor (Meyers singleton)

        // --------------------------------------------------------------------
        // Public query API – called by the game simulation
        // --------------------------------------------------------------------
        bool IsKeyDown(Key key) const;
        bool WasKeyPressed(Key key) const;     // true only on the frame the key transitioned
        bool WasKeyReleased(Key key) const;

        bool IsMouseButtonDown(MouseButton button) const;
        bool WasMouseButtonPressed(MouseButton button) const;
        bool WasMouseButtonReleased(MouseButton button) const;

        const MouseState& GetMouseState() const;
        const KeyboardState& GetKeyboardState() const;

        // Gamepad API – optional (implementation may be empty on platforms without gamepad support)
        bool IsGamepadConnected(uint32_t index) const;
        const GamepadState& GetGamepadState(uint32_t index) const;

        // --------------------------------------------------------------------
        // Internal – only platform implementations should call these
        // --------------------------------------------------------------------
        // Called by the OS message pump to update the raw state.
        void SetKeyState(Key key, bool down);
        void SetMouseButtonState(MouseButton button, bool down);
        void SetMousePosition(int32_t x, int32_t y);
        void AddMouseDelta(int32_t dx, int32_t dy);
        void AddWheelDelta(float delta);

        void SetGamepadState(uint32_t index, const GamepadState& state);
        void SetGamepadConnected(uint32_t index, bool connected);

        // Called once per frame after the game has read the state.
        void EndFrame();    // resets “pressed/released” flags, clears deltas

    private:
        Input() = default;  // only Instance() can construct it
        ~Input() = default;

        // Internal storage (packed to reduce cache misses)
        KeyboardState keyboard_;
        MouseState    mouse_;
        GamepadState  gamepads_[4];   // support up to 4 controllers
        bool          gamepadConnected_[4]{};

        // Helper for transition detection (previous frame ↔ current frame)
        KeyboardState keyboardPrev_;
        MouseState    mousePrev_;
    };

} // namespace eng::platform
