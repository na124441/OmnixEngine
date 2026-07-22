#include "Core/Platform/Clipboard.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <cstring>

namespace eng::platform {

    bool Clipboard::SetText(const std::string& text) noexcept {
        if (!OpenClipboard(nullptr)) {
            return false;
        }

        if (!EmptyClipboard()) {
            CloseClipboard();
            return false;
        }

        size_t size = text.size() + 1;
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, size);
        if (!hMem) {
            CloseClipboard();
            return false;
        }

        char* pMem = static_cast<char*>(GlobalLock(hMem));
        if (pMem) {
            std::memcpy(pMem, text.c_str(), size);
            GlobalUnlock(hMem);
        } else {
            GlobalFree(hMem);
            CloseClipboard();
            return false;
        }

        if (!SetClipboardData(CF_TEXT, hMem)) {
            GlobalFree(hMem);
            CloseClipboard();
            return false;
        }

        CloseClipboard();
        return true;
    }

    std::string Clipboard::GetText() noexcept {
        if (!OpenClipboard(nullptr)) {
            return "";
        }

        HANDLE hData = GetClipboardData(CF_TEXT);
        if (!hData) {
            CloseClipboard();
            return "";
        }

        char* pMem = static_cast<char*>(GlobalLock(hData));
        std::string result;
        if (pMem) {
            result = pMem;
            GlobalUnlock(hData);
        }

        CloseClipboard();
        return result;
    }

    bool Clipboard::HasText() noexcept {
        return IsClipboardFormatAvailable(CF_TEXT) != 0;
    }

} // namespace eng::platform

#else // Non-Windows Stub

namespace eng::platform {

    bool Clipboard::SetText(const std::string& /*text*/) noexcept {
        return false;
    }

    std::string Clipboard::GetText() noexcept {
        return "";
    }

    bool Clipboard::HasText() noexcept {
        return false;
    }

} // namespace eng::platform

#endif
