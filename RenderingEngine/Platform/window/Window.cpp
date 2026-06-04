#include "Window.h"
#include "WindowWin32.h"
#include <memory>

namespace eng::platform {

    eng::core::Result Window::Create(const std::string& title,
        uint32_t width,
        uint32_t height,
        WindowMode mode,
        std::unique_ptr<Window>& outWindow)
    {
#ifdef _WIN32
        auto window = std::make_unique<WindowWin32>();
        auto result = window->Initialize(title, width, height, mode);
        if (result.IsSuccess()) {
            outWindow = std::move(window);
        }
        return result;
#else
        return eng::core::Result(eng::core::ResultCode::Failure);
#endif
    }

} // namespace eng::platform
