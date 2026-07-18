/*******************************************************************************************************************
 * @file  WindowWin32.h
 * @brief Win32 implementation of the platform window interface.
 *
 *        Provides:
 *          • Win32 window creation and management
 *          • Message processing loop
 *          • Integration with input system
 *          • Cursor control and window mode management
 *          • Native window handle for Vulkan surface creation
 *
 *        Usage:
 *            WindowWin32 window;
 *            window.Initialize("My Game", 1920, 1080, WindowMode::Windowed);
 *            while (window.PollEvents().IsSuccess()) {
 *                // Process frame...
 *            }
 *
 *        © 2024 Your Engine Project – all rights reserved.
 *******************************************************************************************************************/

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <shellapi.h>
#include <string>
#include <cstdint>
#include "platform/window/Window.h"
#include "Editor/AssetImportService.h"
#include "core/types/Result.h"
#include "core/log/Log.h"

 // Forward declarations
struct IUnknown;  // Needed for some Windows headers

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace eng::platform {

    /**
     * @class WindowWin32
     *
     * Win32-specific implementation of the Window interface.
     *
     * This class handles:
     *   - Creating and managing a Win32 window
     *   - Processing Windows messages
     *   - Integrating with the input system
     *   - Managing window modes (windowed, borderless, fullscreen)
     *   - Providing the native window handle for Vulkan
     */
    class WindowWin32 : public Window {
    public:
        WindowWin32() = default;
        ~WindowWin32() override;

        /**
         * @brief Initialize the Win32 window.
         *
         * @param title Window title
         * @param width Initial window width
         * @param height Initial window height
         * @param mode Initial window mode
         * @return Result::Success on success, Result::Failure on error
         */
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
        /**
         * @brief Register the window class if not already registered.
         * @return true on success, false on failure
         */
        bool RegisterWindowClass() noexcept;

        /**
         * @brief Create the actual Win32 window.
         * @return true on success, false on failure
         */
        bool CreateWin32Window() noexcept;

        /**
         * @brief Process all pending Windows messages.
         */
        void ProcessMessages() noexcept;

        /**
         * @brief Adjust window size to account for borders and title bar.
         */
        void AdjustWindowRect(RECT& rect) const noexcept;

        /**
         * @brief Update window style based on current mode.
         */
        void UpdateWindowStyle() noexcept;

        /**
         * @brief Window procedure callback for message handling.
         */
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

    // ------------------------------------------------------------------------------------------------
    // Global Window Instance Tracking (for message routing)
    // ------------------------------------------------------------------------------------------------

    namespace detail {
        // Store pointer to current window instance for message routing
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

    // Static members
    bool WindowWin32::s_ClassRegistered = false;
    const wchar_t* const WindowWin32::s_WindowClassName = L"EngineWindowClass";

    // ------------------------------------------------------------------------------------------------
    // Destructor
    // ------------------------------------------------------------------------------------------------

    WindowWin32::~WindowWin32()
    {
        if (m_hWnd) {
            DestroyWindow(m_hWnd);
            m_hWnd = nullptr;
        }

        // Unregister class if we were the one who registered it
        if (s_ClassRegistered) {
            UnregisterClassW(s_WindowClassName, GetModuleHandle(nullptr));
            s_ClassRegistered = false;
        }

        detail::SetCurrentWindow(nullptr);
    }

    // ------------------------------------------------------------------------------------------------
    // Initialization
    // ------------------------------------------------------------------------------------------------

    eng::core::Result WindowWin32::Initialize(const std::string& title,
        uint32_t width,
        uint32_t height,
        WindowMode mode) noexcept
    {
        m_Title = title;
        m_Width = width;
        m_Height = height;
        m_Mode = mode;
        m_hInstance = GetModuleHandle(nullptr);

        // Register window class
        if (!RegisterWindowClass()) {
            ENG_LOG_ERROR("Failed to register window class");
            return eng::core::Result(eng::core::ResultCode::Failure);
        }

        // Create the window
        if (!CreateWin32Window()) {
            ENG_LOG_ERROR("Failed to create window");
            return eng::core::Result(eng::core::ResultCode::Failure);
        }

        // Show and update the window
        ShowWindow(m_hWnd, SW_SHOW);
        UpdateWindow(m_hWnd);

        ENG_LOG_INFO("Successfully created Win32 window: {}x{} '{}'", width, height, title);
        return eng::core::Result(); // Success
    }

    // ------------------------------------------------------------------------------------------------
    // Window Interface Implementation
    // ------------------------------------------------------------------------------------------------

    void WindowWin32::SetTitle(const std::string& newTitle)
    {
        if (m_hWnd) {
            m_Title = newTitle;

            // Convert UTF-8 to wide string
            int wideCharCount = MultiByteToWideChar(CP_UTF8, 0, newTitle.c_str(), -1, nullptr, 0);
            if (wideCharCount > 0) {
                std::wstring wideTitle(wideCharCount, 0);
                MultiByteToWideChar(CP_UTF8, 0, newTitle.c_str(), -1, &wideTitle[0], wideCharCount);
                SetWindowTextW(m_hWnd, wideTitle.c_str());
            }
        }
    }

    void WindowWin32::Resize(uint32_t width, uint32_t height)
    {
        if (m_hWnd && width > 0 && height > 0) {
            m_Width = width;
            m_Height = height;

            if (m_Mode != WindowMode::Fullscreen) {
                // Adjust window size to include borders
                RECT rect = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
                AdjustWindowRect(rect);

                int windowWidth = rect.right - rect.left;
                int windowHeight = rect.bottom - rect.top;

                SetWindowPos(m_hWnd, HWND_TOP, 0, 0, windowWidth, windowHeight,
                    SWP_NOZORDER | SWP_NOMOVE);
            }
        }
    }

    void WindowWin32::SetMode(WindowMode mode)
    {
        if (m_Mode == mode) return;

        m_Mode = mode;
        UpdateWindowStyle();

        if (mode == WindowMode::Fullscreen) {
            // Enter fullscreen
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
            // Exit fullscreen
            if (m_Mode == WindowMode::Windowed) {
                ChangeDisplaySettings(nullptr, 0);
            }
            UpdateWindowStyle();
            Resize(m_Width, m_Height); // Restore window size and position
        }
    }

    void WindowWin32::SetCursorMode(CursorMode mode)
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

    eng::core::Result WindowWin32::PollEvents()
    {
        ProcessMessages();
        if (m_ShouldClose) {
            return eng::core::Result(eng::core::ResultCode::Failure);
        }

        return eng::core::Result(); // Success
    }

    void* WindowWin32::GetNativeHandle() const
    {
        return reinterpret_cast<void*>(m_hWnd);
    }

    // ------------------------------------------------------------------------------------------------
    // Private Methods
    // ------------------------------------------------------------------------------------------------

    bool WindowWin32::RegisterWindowClass() noexcept
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

    bool WindowWin32::CreateWin32Window() noexcept
    {
        // Convert title to wide string
        int wideCharCount = MultiByteToWideChar(CP_UTF8, 0, m_Title.c_str(), -1, nullptr, 0);
        std::wstring wideTitle(wideCharCount, 0);
        if (wideCharCount > 0) {
            MultiByteToWideChar(CP_UTF8, 0, m_Title.c_str(), -1, &wideTitle[0], wideCharCount);
        }

        // Set window style based on mode
        DWORD style = WS_OVERLAPPEDWINDOW;
        DWORD exStyle = 0;

        if (m_Mode == WindowMode::BorderlessWindowed) {
            style = WS_POPUP;
        }
        else if (m_Mode == WindowMode::Fullscreen) {
            style = WS_POPUP;
            exStyle = WS_EX_TOPMOST;
        }

        // Calculate window size
        RECT rect = { 0, 0, static_cast<LONG>(m_Width), static_cast<LONG>(m_Height) };
        AdjustWindowRect(rect);

        int windowWidth = rect.right - rect.left;
        int windowHeight = rect.bottom - rect.top;

        // Create the window
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

        // Associate this window instance with the HWND for message routing
        SetWindowLongPtr(m_hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
        detail::SetCurrentWindow(this);
        DragAcceptFiles(m_hWnd, TRUE);

        return true;
    }

    void WindowWin32::ProcessMessages() noexcept
    {
        MSG msg = { 0 };

        // Process all pending messages
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

    void WindowWin32::AdjustWindowRect(RECT& rect) const noexcept
    {
        DWORD style = GetWindowLong(m_hWnd, GWL_STYLE);
        DWORD exStyle = GetWindowLong(m_hWnd, GWL_EXSTYLE);

        AdjustWindowRectEx(&rect, style, FALSE, exStyle);
    }

    void WindowWin32::UpdateWindowStyle() noexcept
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

    // Declared globally above

    LRESULT CALLBACK WindowWin32::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam)) {
            return 1;
        }

        // Get the window instance
        WindowWin32* window = reinterpret_cast<WindowWin32*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

        // If we don't have a window instance yet, use the global one
        if (!window) {
            window = detail::GetCurrentWindow();
            if (window && window->m_hWnd == nullptr) {
                window->m_hWnd = hwnd;
                SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
            }
        }

        if (window) {
            switch (msg) {
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
                        
                        // Convert wide string to UTF-8
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
                ENG_LOG_INFO("Win32 window received WM_DESTROY.");
                PostQuitMessage(0);
                return 0;

            case WM_SIZE:
                if (wParam != SIZE_MINIMIZED) {
                    window->m_Width = LOWORD(lParam);
                    window->m_Height = HIWORD(lParam);
                }
                return 0;

            case WM_KEYDOWN:
            case WM_KEYUP:
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
            case WM_RBUTTONDOWN:
            case WM_RBUTTONUP:
            case WM_MBUTTONDOWN:
            case WM_MBUTTONUP:
            case WM_MOUSEMOVE:
            case WM_MOUSEWHEEL:
                // These messages are handled by the input system
                // Forward them to the global input manager if needed
                break;

            default:
                break;
            }
        }

        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

} // namespace eng::platform
