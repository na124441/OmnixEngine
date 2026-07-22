#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <shellapi.h>
#include <string>
#include <cstdint>
#include <memory>
#include "Core/Platform/Window.h"
#include "Core/Platform/Input.h"
#include "Editor/AssetImportService.h"
#include "RenderingEngine/Core/types/Result.h"
#include "RenderingEngine/Core/Log/log.h"

struct IUnknown;

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace eng::platform {

    class WindowWin32 : public Window {
    public:
        WindowWin32() = default;
        ~WindowWin32() override;

        [[nodiscard]] eng::core::Result Initialize(const std::string& title,
            uint32_t width,
            uint32_t height,
            WindowMode mode) noexcept;

        // Window interface implementation
        void SetTitle(const std::string& newTitle) override;
        void Resize(uint32_t width, uint32_t height) override;
        void SetMode(WindowMode mode) override;

        uint32_t GetWidth() const override { return m_Width; }
        uint32_t GetHeight() const override { return m_Height; }
        WindowMode GetMode() const override { return m_Mode; }

        void SetCursorMode(CursorMode mode) override;
        CursorMode GetCursorMode() const override { return m_CursorMode; }

        eng::core::Result PollEvents() override;
        void* GetNativeHandle() const override;

    private:
        bool RegisterWindowClass() noexcept;
        bool CreateWin32Window() noexcept;
        void ProcessMessages() noexcept;
        void AdjustWindowRect(RECT& rect) const noexcept;
        void UpdateWindowStyle() noexcept;

        static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

        // Window state
        HWND m_hWnd = nullptr;
        HINSTANCE m_hInstance = nullptr;
        std::string m_Title;
        uint32_t m_Width = 0;
        uint32_t m_Height = 0;
        WindowMode m_Mode = WindowMode::Windowed;
        CursorMode m_CursorMode = CursorMode::Normal;
        bool m_ShouldClose = false;

        // Class registration tracking
        static bool s_ClassRegistered;
        static const wchar_t* const s_WindowClassName;
    };

    inline bool WindowWin32::s_ClassRegistered = false;
    inline const wchar_t* const WindowWin32::s_WindowClassName = L"EngineWindowClass";

    namespace detail {
        inline WindowWin32* g_CurrentWindow = nullptr;

        inline void SetCurrentWindow(WindowWin32* window) noexcept {
            g_CurrentWindow = window;
        }

        inline WindowWin32* GetCurrentWindow() noexcept {
            return g_CurrentWindow;
        }
    }

} // namespace eng::platform

// ------------------------------------------------------------------------------------------------
// Inline Implementation
// ------------------------------------------------------------------------------------------------

namespace eng::platform {

    // Destructor
    inline WindowWin32::~WindowWin32()
    {
        if (m_hWnd) {
            DestroyWindow(m_hWnd);
            m_hWnd = nullptr;
        }

        if (s_ClassRegistered) {
            UnregisterClassW(s_WindowClassName, GetModuleHandle(nullptr));
            s_ClassRegistered = false;
        }

        detail::SetCurrentWindow(nullptr);
    }

    // Initialization
    inline eng::core::Result WindowWin32::Initialize(const std::string& title,
        uint32_t width,
        uint32_t height,
        WindowMode mode) noexcept
    {
        m_Title = title;
        m_Width = width;
        m_Height = height;
        m_Mode = mode;
        m_hInstance = GetModuleHandle(nullptr);

        if (!RegisterWindowClass()) {
            ENG_LOG_ERROR("Failed to register window class");
            return eng::core::Result(eng::core::ResultCode::Failure);
        }

        if (!CreateWin32Window()) {
            ENG_LOG_ERROR("Failed to create window");
            return eng::core::Result(eng::core::ResultCode::Failure);
        }

        ShowWindow(m_hWnd, SW_SHOW);
        UpdateWindow(m_hWnd);

        ENG_LOG_INFO("Successfully created Win32 window: {}x{} '{}'", width, height, title);
        return eng::core::Result();
    }

    // Window Interface Implementation
    inline void WindowWin32::SetTitle(const std::string& newTitle)
    {
        if (m_hWnd) {
            m_Title = newTitle;
            int wideCharCount = MultiByteToWideChar(CP_UTF8, 0, newTitle.c_str(), -1, nullptr, 0);
            if (wideCharCount > 0) {
                std::wstring wideTitle(wideCharCount, 0);
                MultiByteToWideChar(CP_UTF8, 0, newTitle.c_str(), -1, &wideTitle[0], wideCharCount);
                SetWindowTextW(m_hWnd, wideTitle.c_str());
            }
        }
    }

    inline void WindowWin32::Resize(uint32_t width, uint32_t height)
    {
        if (m_hWnd && width > 0 && height > 0) {
            m_Width = width;
            m_Height = height;

            if (m_Mode != WindowMode::Fullscreen) {
                RECT rect = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
                AdjustWindowRect(rect);

                int windowWidth = rect.right - rect.left;
                int windowHeight = rect.bottom - rect.top;

                SetWindowPos(m_hWnd, HWND_TOP, 0, 0, windowWidth, windowHeight,
                    SWP_NOZORDER | SWP_NOMOVE);
            }
        }
    }

    inline void WindowWin32::SetMode(WindowMode mode)
    {
        if (m_Mode == mode) return;

        m_Mode = mode;
        UpdateWindowStyle();

        if (mode == WindowMode::Fullscreen) {
            DEVMODE dmScreenSettings = { 0 };
            dmScreenSettings.dmSize = sizeof(dmScreenSettings);
            dmScreenSettings.dmPelsWidth = static_cast<DWORD>(m_Width);
            dmScreenSettings.dmPelsHeight = static_cast<DWORD>(m_Height);
            dmScreenSettings.dmBitsPerPel = 32;
            dmScreenSettings.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;

            if (ChangeDisplaySettings(&dmScreenSettings, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL) {
                ENG_LOG_WARN("Failed to enter fullscreen mode");
                m_Mode = WindowMode::Windowed;
                UpdateWindowStyle();
                return;
            }

            SetWindowPos(m_hWnd, HWND_TOP, 0, 0, m_Width, m_Height, SWP_NOZORDER | SWP_FRAMECHANGED);
        }
        else {
            if (m_Mode == WindowMode::Windowed) {
                ChangeDisplaySettings(nullptr, 0);
            }
            UpdateWindowStyle();
            Resize(m_Width, m_Height);
        }
    }

    inline void WindowWin32::SetCursorMode(CursorMode mode)
    {
        if (m_CursorMode == mode) return;

        m_CursorMode = mode;

        switch (mode) {
        case CursorMode::Normal:
            ShowCursor(TRUE);
            ClipCursor(nullptr);
            break;

        case CursorMode::Hidden:
            ShowCursor(FALSE);
            break;

        case CursorMode::Disabled:
            ShowCursor(FALSE);
            if (m_hWnd) {
                RECT rect;
                GetClientRect(m_hWnd, &rect);
                ClientToScreen(m_hWnd, reinterpret_cast<POINT*>(&rect.left));
                ClientToScreen(m_hWnd, reinterpret_cast<POINT*>(&rect.right));
                ClipCursor(&rect);
            }
            break;
        }
    }

    inline eng::core::Result WindowWin32::PollEvents()
    {
        ProcessMessages();
        if (m_ShouldClose) {
            return eng::core::Result(eng::core::ResultCode::Failure);
        }
        return eng::core::Result();
    }

    inline void* WindowWin32::GetNativeHandle() const
    {
        return reinterpret_cast<void*>(m_hWnd);
    }

    // Private Methods
    inline bool WindowWin32::RegisterWindowClass() noexcept
    {
        if (s_ClassRegistered) {
            return true;
        }

        WNDCLASSEXW wc = { 0 };
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
        wc.lpfnWndProc = WndProc;
        wc.hInstance = m_hInstance;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.lpszClassName = s_WindowClassName;
        wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
        wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);

        if (!RegisterClassExW(&wc)) {
            ENG_LOG_ERROR("Failed to register window class: {}", GetLastError());
            return false;
        }

        s_ClassRegistered = true;
        return true;
    }

    inline bool WindowWin32::CreateWin32Window() noexcept
    {
        int wideCharCount = MultiByteToWideChar(CP_UTF8, 0, m_Title.c_str(), -1, nullptr, 0);
        std::wstring wideTitle(wideCharCount, 0);
        if (wideCharCount > 0) {
            MultiByteToWideChar(CP_UTF8, 0, m_Title.c_str(), -1, &wideTitle[0], wideCharCount);
        }

        DWORD style = WS_OVERLAPPEDWINDOW;
        DWORD exStyle = 0;

        if (m_Mode == WindowMode::BorderlessWindowed) {
            style = WS_POPUP;
        }
        else if (m_Mode == WindowMode::Fullscreen) {
            style = WS_POPUP;
            exStyle = WS_EX_TOPMOST;
        }

        RECT rect = { 0, 0, static_cast<LONG>(m_Width), static_cast<LONG>(m_Height) };
        AdjustWindowRect(rect);

        int windowWidth = rect.right - rect.left;
        int windowHeight = rect.bottom - rect.top;

        m_hWnd = CreateWindowExW(
            exStyle,
            s_WindowClassName,
            wideTitle.c_str(),
            style,
            CW_USEDEFAULT, CW_USEDEFAULT,
            windowWidth, windowHeight,
            nullptr,
            nullptr,
            m_hInstance,
            nullptr
        );

        if (!m_hWnd) {
            ENG_LOG_ERROR("Failed to create window: {}", GetLastError());
            return false;
        }

        SetWindowLongPtr(m_hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
        detail::SetCurrentWindow(this);
        DragAcceptFiles(m_hWnd, TRUE);

        return true;
    }

    inline void WindowWin32::ProcessMessages() noexcept
    {
        MSG msg = { 0 };
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                ENG_LOG_INFO("Win32 window received WM_QUIT.");
                m_ShouldClose = true;
                continue;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    inline void WindowWin32::AdjustWindowRect(RECT& rect) const noexcept
    {
        DWORD style = GetWindowLong(m_hWnd, GWL_STYLE);
        DWORD exStyle = GetWindowLong(m_hWnd, GWL_EXSTYLE);
        AdjustWindowRectEx(&rect, style, FALSE, exStyle);
    }

    inline void WindowWin32::UpdateWindowStyle() noexcept
    {
        if (!m_hWnd) return;

        DWORD style = 0;
        DWORD exStyle = 0;

        switch (m_Mode) {
        case WindowMode::Windowed:
            style = WS_OVERLAPPEDWINDOW;
            break;
        case WindowMode::BorderlessWindowed:
            style = WS_POPUP;
            break;
        case WindowMode::Fullscreen:
            style = WS_POPUP;
            exStyle = WS_EX_TOPMOST;
            break;
        }

        SetWindowLong(m_hWnd, GWL_STYLE, style);
        SetWindowLong(m_hWnd, GWL_EXSTYLE, exStyle);
        SetWindowPos(m_hWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);
    }

    inline Key MapWin32Key(WPARAM wParam) noexcept {
        if (wParam >= 'A' && wParam <= 'Z') {
            return static_cast<Key>(static_cast<uint16_t>(Key::A) + (wParam - 'A'));
        }
        if (wParam >= '0' && wParam <= '9') {
            return static_cast<Key>(static_cast<uint16_t>(Key::Num0) + (wParam - '0'));
        }
        switch (wParam) {
            case VK_ESCAPE: return Key::Escape;
            case VK_SPACE:  return Key::Space;
            case VK_RETURN: return Key::Enter;
            case VK_TAB:    return Key::Tab;
            case VK_BACK:   return Key::Backspace;
            case VK_SHIFT:  return Key::LeftShift;
            case VK_CONTROL:return Key::LeftCtrl;
            case VK_MENU:   return Key::LeftAlt;
            case VK_UP:     return Key::ArrowUp;
            case VK_DOWN:   return Key::ArrowDown;
            case VK_LEFT:   return Key::ArrowLeft;
            case VK_RIGHT:  return Key::ArrowRight;
            default:        return Key::Unknown;
        }
    }

    inline LRESULT CALLBACK WindowWin32::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam)) {
            return 1;
        }

        WindowWin32* window = reinterpret_cast<WindowWin32*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

        if (!window) {
            window = detail::GetCurrentWindow();
            if (window && window->m_hWnd == nullptr) {
                window->m_hWnd = hwnd;
                SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
            }
        }

        if (window) {
            switch (msg) {
            case WM_KEYDOWN:
            case WM_SYSKEYDOWN: {
                Key k = MapWin32Key(wParam);
                if (k != Key::Unknown) {
                    Input::Instance().SetKeyState(k, true);
                }
                break;
            }
            case WM_KEYUP:
            case WM_SYSKEYUP: {
                Key k = MapWin32Key(wParam);
                if (k != Key::Unknown) {
                    Input::Instance().SetKeyState(k, false);
                }
                break;
            }
            case WM_LBUTTONDOWN:
                Input::Instance().SetMouseButtonState(MouseButton::Left, true);
                break;
            case WM_LBUTTONUP:
                Input::Instance().SetMouseButtonState(MouseButton::Left, false);
                break;
            case WM_RBUTTONDOWN:
                Input::Instance().SetMouseButtonState(MouseButton::Right, true);
                break;
            case WM_RBUTTONUP:
                Input::Instance().SetMouseButtonState(MouseButton::Right, false);
                break;
            case WM_MBUTTONDOWN:
                Input::Instance().SetMouseButtonState(MouseButton::Middle, true);
                break;
            case WM_MBUTTONUP:
                Input::Instance().SetMouseButtonState(MouseButton::Middle, false);
                break;
            case WM_MOUSEMOVE: {
                int32_t x = static_cast<int16_t>(LOWORD(lParam));
                int32_t y = static_cast<int16_t>(HIWORD(lParam));
                
                auto& input = Input::Instance();
                const auto& ms = input.GetMouseState();
                int32_t dx = x - ms.x;
                int32_t dy = y - ms.y;
                input.AddMouseDelta(dx, dy);
                input.SetMousePosition(x, y);
                break;
            }
            case WM_MOUSEWHEEL: {
                float delta = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / static_cast<float>(WHEEL_DELTA);
                Input::Instance().AddWheelDelta(delta);
                break;
            }

            case WM_CLOSE:
                ENG_LOG_INFO("Win32 window received WM_CLOSE.");
                window->m_ShouldClose = true;
                return 0;

            case WM_DROPFILES: {
                HDROP hDrop = reinterpret_cast<HDROP>(wParam);
                UINT fileCount = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
                for (UINT i = 0; i < fileCount; ++i) {
                    UINT len = DragQueryFileW(hDrop, i, nullptr, 0);
                    if (len > 0) {
                        std::wstring widePath(len + 1, L'\0');
                        DragQueryFileW(hDrop, i, &widePath[0], len + 1);
                        int utf8Len = WideCharToMultiByte(CP_UTF8, 0, widePath.c_str(), -1, nullptr, 0, nullptr, nullptr);
                        if (utf8Len > 0) {
                            std::string utf8Path(utf8Len, '\0');
                            WideCharToMultiByte(CP_UTF8, 0, widePath.c_str(), -1, &utf8Path[0], utf8Len, nullptr, nullptr);
                            if (!utf8Path.empty() && utf8Path.back() == '\0') {
                                utf8Path.pop_back();
                            }
                            eng::runtime::AssetImportService::AddDroppedFile(utf8Path);
                        }
                    }
                }
                DragFinish(hDrop);
                return 0;
            }

            case WM_DESTROY:
                PostQuitMessage(0);
                return 0;

            case WM_SIZE:
                if (wParam != SIZE_MINIMIZED) {
                    window->m_Width = LOWORD(lParam);
                    window->m_Height = HIWORD(lParam);
                }
                return 0;

            default:
                break;
            }
        }

        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

} // namespace eng::platform
