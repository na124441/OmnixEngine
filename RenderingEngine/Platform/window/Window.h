#pragma once
#include <cstdint>
#include <string>
#include <functional>
#include "core/types/Result.h"
#include "core/types/Handle.h"
#include "core/log/Log.h"          // optional, for Init/Shutdown messages

namespace eng::platform {

    enum class WindowMode {
        Windowed,
        BorderlessWindowed,
        Fullscreen
    };

    enum class CursorMode {
        Normal,          // visible, OS controls
        Hidden,          // invisible but may move
        Disabled         // confined + hidden (e.g., FPS camera)
    };

    /**
     * @brief Opaque handle that the RHI uses to create a VkSurfaceKHR (or equivalent).
     * Platform‑specific implementation returns a native window handle.
     */
    using NativeWindowHandle = void*;   // HWND on Windows, Window* on macOS, etc.

    /**
     * @class Window
     * @brief Public, *non‑copyable* window abstraction.
     *
     * Lifetime:
     *   - Constructed via static `Create` function.
     *   - Destroyed through `Destroy` (or by letting the unique_ptr go out of scope).
     *
     * Thread‑safety:
     *   - All public methods must be called from the *owner thread* (normally the main thread).
     *   - Internally the OS may invoke callbacks on a separate thread; these are queued
     *     and processed by `PollEvents` on the owner thread.
     */
    class Window {
    public:
        // --------------------------------------------------------------------
        // Construction / destruction
        // --------------------------------------------------------------------
        Window() = default;
        virtual ~Window() = default;

        Window(const Window&) = delete;
        Window& operator=(const Window&) = delete;

        /**
         * @brief Factory – creates the concrete OS window.
         *
         * @param title      Window caption.
         * @param width      Initial client width (pixels).
         * @param height     Initial client height (pixels).
         * @param mode       Desired mode (windowed / fullscreen …).
         * @param outWindow  Receives a unique_ptr to the concrete Window implementation.
         *
         * @return Result::Success on success, otherwise an error code.
         *
         * @note The function may fail if the OS cannot create a surface, if the
         *       requested video mode is unavailable, or if required extensions are missing.
         */
        static eng::core::Result Create(const std::string& title,
            uint32_t width,
            uint32_t height,
            WindowMode mode,
            std::unique_ptr<Window>& outWindow);

        // --------------------------------------------------------------------
        // Basic window operations
        // --------------------------------------------------------------------
        virtual void SetTitle(const std::string& newTitle) = 0;
        virtual void Resize(uint32_t width, uint32_t height) = 0;
        virtual void SetMode(WindowMode mode) = 0;

        virtual uint32_t GetWidth()  const = 0;
        virtual uint32_t GetHeight() const = 0;
        virtual WindowMode GetMode() const = 0;

        // --------------------------------------------------------------------
        // Cursor handling
        // --------------------------------------------------------------------
        virtual void SetCursorMode(CursorMode mode) = 0;
        virtual CursorMode GetCursorMode() const = 0;

        // --------------------------------------------------------------------
        // Event pump
        // --------------------------------------------------------------------
        /**
         * @brief Process OS messages, update internal input state, and push
         *        platform‑agnostic input events into the Input subsystem.
         *
         * @return Result::Success  – normal case.
         *         Result::Failure  – OS reported an unrecoverable error (e.g. WM_CLOSE).
         */
        virtual eng::core::Result PollEvents() = 0;

        // --------------------------------------------------------------------
        // Vulkan (or other API) surface creation glue
        // --------------------------------------------------------------------
        /**
         * @brief Return the native window handle that the RHI uses to create a
         *        `VkSurfaceKHR` (or an equivalent surface type on other APIs).
         *
         * This is the only place where a concrete OS type leaks out of the platform
         * layer – the RHI knows how to interpret the opaque `void*`.
         */
        virtual NativeWindowHandle GetNativeHandle() const = 0;
    };

} // namespace eng::platform
