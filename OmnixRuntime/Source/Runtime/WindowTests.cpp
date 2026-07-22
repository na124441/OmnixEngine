#include "Runtime/WindowTests.h"
#include "Core/Platform/Window.h"
#include "Core/Logger.h"
#include <cassert>

namespace eng::runtime {

    void RunWindowTests() {
        CORE_LOG_INFO("=== Running Kernel Window Management Tests ===");

        // Test 1: Successful Window Creation & Basic Properties
        {
            CORE_LOG_INFO("[Test] Creating test window...");
            std::unique_ptr<eng::platform::Window> window;
            auto result = eng::platform::Window::Create(
                "Omnix Window Unit Test",
                800,
                600,
                eng::platform::WindowMode::Windowed,
                window
            );

            assert(result.IsSuccess() && "Failed to create platform window!");
            assert(window != nullptr && "Window pointer is null after creation!");

            // Check attributes
            assert(window->GetWidth() == 800 && "Window width mismatch!");
            assert(window->GetHeight() == 600 && "Window height mismatch!");
            assert(window->GetMode() == eng::platform::WindowMode::Windowed && "Window mode mismatch!");

            CORE_LOG_INFO("[Test] Window creation verified successfully.");

            // Test 2: Setters & Cursor modifications
            CORE_LOG_INFO("[Test] Testing window state mutations...");
            window->SetTitle("Updated Test Title");
            window->SetCursorMode(eng::platform::CursorMode::Normal);
            assert(window->GetCursorMode() == eng::platform::CursorMode::Normal && "Cursor mode setter failed!");

            // Test 3: Event Polling
            auto pollResult = window->PollEvents();
            assert(pollResult.IsSuccess() && "Window PollEvents failed on active window!");

            // Test 4: Native Handle Exposure
            void* handle = window->GetNativeHandle();
            assert(handle != nullptr && "GetNativeHandle returned null!");

            // Destruction (automatic via scope unique_ptr)
            CORE_LOG_INFO("[Test] Destructing window...");
            window.reset();
            CORE_LOG_INFO("[Test] Window state mutation and destruction verified.");
        }

        CORE_LOG_INFO("=== All Window Management Tests Passed Successfully ===");
    }
}
