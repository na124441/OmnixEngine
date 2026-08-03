#include "Runtime/InputTests.h"
#include "Core/Platform/Input.h"
#include "Core/Logger.h"
#include <cassert>

namespace eng::runtime {

    void RunInputTests() {
        CORE_LOG_INFO("=== Running Kernel Input Backend Tests ===");

        auto& input = eng::platform::Input::Instance();

        // Test 1: Key Down/Up Transitions and Edge Detection
        {
            CORE_LOG_INFO("[Test] Testing Keyboard input transitions...");
            
            // Initial state: key should be up
            assert(!input.IsKeyDown(eng::platform::Key::Space) && "Space key initially down!");
            assert(!input.WasKeyPressed(eng::platform::Key::Space) && "Space key initially pressed!");

            // Frame 1: Space key is pressed
            input.SetKeyState(eng::platform::Key::Space, true);
            assert(input.IsKeyDown(eng::platform::Key::Space) && "IsKeyDown failed to report pressed state!");
            
            // End frame transition
            input.EndFrame();

            // WasKeyPressed should report true now because it transitioned from UP -> DOWN
            assert(input.IsKeyDown(eng::platform::Key::Space) && "Space key lost held state after EndFrame!");
            assert(input.WasKeyPressed(eng::platform::Key::Space) && "WasKeyPressed failed to detect UP -> DOWN edge!");
            assert(!input.WasKeyReleased(eng::platform::Key::Space) && "WasKeyReleased falsely reported true!");

            // Frame 2: Space key is released
            input.SetKeyState(eng::platform::Key::Space, false);
            assert(!input.IsKeyDown(eng::platform::Key::Space) && "IsKeyDown failed to report released state!");

            input.EndFrame();

            // WasKeyReleased should report true because it transitioned from DOWN -> UP
            assert(!input.IsKeyDown(eng::platform::Key::Space) && "Space key still held after release!");
            assert(!input.WasKeyPressed(eng::platform::Key::Space) && "WasKeyPressed falsely reported true after release!");
            assert(input.WasKeyReleased(eng::platform::Key::Space) && "WasKeyReleased failed to detect DOWN -> UP edge!");

            // Frame 3: End frame again, transition clears edge flags
            input.EndFrame();
            assert(!input.WasKeyReleased(eng::platform::Key::Space) && "WasKeyReleased edge flag not cleared!");
        }

        // Test 2: Mouse State Modifiers and Per-Frame Resetting
        {
            CORE_LOG_INFO("[Test] Testing Mouse state modifiers...");

            input.SetMousePosition(100, 200);
            input.AddMouseDelta(15, -30);
            input.AddWheelDelta(2.5f);
            input.SetMouseButtonState(eng::platform::MouseButton::Left, true);

            const auto& ms = input.GetMouseState();
            assert(ms.x == 100 && ms.y == 200 && "Mouse position mismatch!");
            assert(ms.deltaX == 15 && ms.deltaY == -30 && "Mouse delta movement mismatch!");
            assert(ms.wheelDelta == 2.5f && "Mouse wheel scroll mismatch!");
            assert(input.IsMouseButtonDown(eng::platform::MouseButton::Left) && "Left click down mismatch!");

            // End frame should reset deltas but preserve position/button state
            input.EndFrame();

            const auto& msAfter = input.GetMouseState();
            assert(msAfter.x == 100 && msAfter.y == 200 && "Mouse position lost after EndFrame!");
            assert(msAfter.deltaX == 0 && msAfter.deltaY == 0 && "Mouse delta failed to reset on EndFrame!");
            assert(msAfter.wheelDelta == 0.0f && "Mouse wheel failed to reset on EndFrame!");
            assert(input.IsMouseButtonDown(eng::platform::MouseButton::Left) && "Left click lost state on EndFrame!");
            assert(input.WasMouseButtonPressed(eng::platform::MouseButton::Left) && "WasMouseButtonPressed failed to report edge!");
            
            // Release mouse
            input.SetMouseButtonState(eng::platform::MouseButton::Left, false);
            input.EndFrame();
            assert(!input.IsMouseButtonDown(eng::platform::MouseButton::Left) && "Left click still held!");
            assert(input.WasMouseButtonReleased(eng::platform::MouseButton::Left) && "WasMouseButtonReleased failed to report edge!");
        }

        // Test 3: Gamepad States
        {
            CORE_LOG_INFO("[Test] Testing Gamepad connection and state...");

            assert(!input.IsGamepadConnected(0) && "Gamepad 0 connected by default!");

            input.SetGamepadConnected(0, true);
            assert(input.IsGamepadConnected(0) && "Gamepad connection state failed!");

            eng::platform::GamepadState state{};
            state.axes[0] = 0.5f; // LStickX
            state.buttons[static_cast<size_t>(eng::platform::GamepadButton::A)] = true;

            input.SetGamepadState(0, state);

            const auto& queried = input.GetGamepadState(0);
            assert(queried.axes[0] == 0.5f && "Gamepad axis state mismatch!");
            assert(queried.buttons[static_cast<size_t>(eng::platform::GamepadButton::A)] && "Gamepad button A state mismatch!");

            input.SetGamepadConnected(0, false);
            assert(!input.IsGamepadConnected(0) && "Gamepad disconnection failed!");
        }

        CORE_LOG_INFO("=== All Input Backend Tests Passed Successfully ===");
    }
}
