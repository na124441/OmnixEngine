#pragma once
#include <cstdint>
#include <string>
#include <memory>
#include "RenderingEngine/Core/types/Result.h"

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

    using NativeWindowHandle = void*;   // HWND on Windows, Window* on macOS, etc.

    class Window {
    public:
        Window() = default;
        virtual ~Window() = default;

        Window(const Window&) = delete;
        Window& operator=(const Window&) = delete;

        static eng::core::Result Create(const std::string& title,
            uint32_t width,
            uint32_t height,
            WindowMode mode,
            std::unique_ptr<Window>& outWindow);

        virtual void SetTitle(const std::string& newTitle) = 0;
        virtual void Resize(uint32_t width, uint32_t height) = 0;
        virtual void SetMode(WindowMode mode) = 0;

        virtual uint32_t GetWidth()  const = 0;
        virtual uint32_t GetHeight() const = 0;
        virtual WindowMode GetMode() const = 0;

        virtual void SetCursorMode(CursorMode mode) = 0;
        virtual CursorMode GetCursorMode() const = 0;

        virtual eng::core::Result PollEvents() = 0;
        virtual NativeWindowHandle GetNativeHandle() const = 0;
    };

} // namespace eng::platform
