#include "Core/Platform/PlatformFileSystem.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace eng::platform {

    // --- PlatformFile Win32 Implementation ---

    PlatformFile::~PlatformFile() noexcept {
        Close();
    }

    PlatformFile::PlatformFile(PlatformFile&& other) noexcept
        : m_Handle(other.m_Handle) {
        other.m_Handle = nullptr;
    }

    PlatformFile& PlatformFile::operator=(PlatformFile&& other) noexcept {
        if (this != &other) {
            Close();
            m_Handle = other.m_Handle;
            other.m_Handle = nullptr;
        }
        return *this;
    }

    eng::core::ResultCode PlatformFile::Open(const std::string& path, FileMode mode, FileShare share, PlatformFile& outFile) noexcept {
        outFile.Close();

        std::string resolved = PlatformFileSystem::ResolveVirtualPath(path);
        int wideCharCount = MultiByteToWideChar(CP_UTF8, 0, resolved.c_str(), -1, nullptr, 0);
        std::wstring widePath(wideCharCount, 0);
        if (wideCharCount > 0) {
            MultiByteToWideChar(CP_UTF8, 0, resolved.c_str(), -1, &widePath[0], wideCharCount);
            if (!widePath.empty() && widePath.back() == L'\0') {
                widePath.pop_back();
            }
        }

        DWORD desiredAccess = 0;
        DWORD creationDisposition = 0;
        switch (mode) {
            case FileMode::Read:
                desiredAccess = GENERIC_READ;
                creationDisposition = OPEN_EXISTING;
                break;
            case FileMode::Write:
                desiredAccess = GENERIC_WRITE;
                creationDisposition = CREATE_ALWAYS;
                break;
            case FileMode::Append:
                desiredAccess = FILE_APPEND_DATA;
                creationDisposition = OPEN_ALWAYS;
                break;
        }

        DWORD shareMode = 0;
        switch (share) {
            case FileShare::None:  shareMode = 0; break;
            case FileShare::Read:  shareMode = FILE_SHARE_READ; break;
            case FileShare::Write: shareMode = FILE_SHARE_WRITE; break;
            case FileShare::All:   shareMode = FILE_SHARE_READ | FILE_SHARE_WRITE; break;
        }

        HANDLE handle = CreateFileW(
            widePath.c_str(),
            desiredAccess,
            shareMode,
            nullptr,
            creationDisposition,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        );

        if (handle == INVALID_HANDLE_VALUE) {
            return eng::core::ResultCode::Failure;
        }

        outFile.m_Handle = handle;
        return eng::core::ResultCode::Success;
    }

    void PlatformFile::Close() noexcept {
        if (m_Handle) {
            CloseHandle(m_Handle);
            m_Handle = nullptr;
        }
    }

    bool PlatformFile::IsOpen() const noexcept {
        return m_Handle != nullptr;
    }

    uint64_t PlatformFile::GetSize() const noexcept {
        if (!m_Handle) return 0;
        LARGE_INTEGER liSize;
        if (GetFileSizeEx(m_Handle, &liSize)) {
            return liSize.QuadPart;
        }
        return 0;
    }

    eng::core::ResultCode PlatformFile::Read(void* buffer, uint64_t bytesToRead, uint64_t& bytesRead) noexcept {
        if (!m_Handle) return eng::core::ResultCode::NotInitialized;

        DWORD read = 0;
        BOOL result = ReadFile(m_Handle, buffer, static_cast<DWORD>(bytesToRead), &read, nullptr);
        bytesRead = read;

        if (!result) {
            return eng::core::ResultCode::Failure;
        }
        return eng::core::ResultCode::Success;
    }

    eng::core::ResultCode PlatformFile::Write(const void* data, uint64_t bytesToWrite, uint64_t& bytesWritten) noexcept {
        if (!m_Handle) return eng::core::ResultCode::NotInitialized;

        DWORD written = 0;
        BOOL result = WriteFile(m_Handle, data, static_cast<DWORD>(bytesToWrite), &written, nullptr);
        bytesWritten = written;

        if (!result) {
            return eng::core::ResultCode::Failure;
        }
        return eng::core::ResultCode::Success;
    }

    eng::core::ResultCode PlatformFile::Seek(int64_t offset, uint64_t& newPosition) noexcept {
        if (!m_Handle) return eng::core::ResultCode::NotInitialized;

        LARGE_INTEGER liOffset;
        liOffset.QuadPart = offset;
        LARGE_INTEGER liNewPos;

        BOOL result = SetFilePointerEx(m_Handle, liOffset, &liNewPos, FILE_BEGIN);
        newPosition = liNewPos.QuadPart;

        if (!result) {
            return eng::core::ResultCode::Failure;
        }
        return eng::core::ResultCode::Success;
    }

    // --- PlatformFileSystem Win32 Implementation ---

    std::string PlatformFileSystem::GetExecutableDirectory() noexcept {
        wchar_t wpath[MAX_PATH];
        DWORD length = GetModuleFileNameW(nullptr, wpath, MAX_PATH);
        if (length == 0 || length >= MAX_PATH) return "";

        std::wstring ws(wpath, length);
        size_t lastSlash = ws.find_last_of(L"\\/");
        if (lastSlash != std::wstring::npos) {
            ws = ws.substr(0, lastSlash);
        }

        int utf8Len = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (utf8Len > 0) {
            std::string path(utf8Len, '\0');
            WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, &path[0], utf8Len, nullptr, nullptr);
            if (!path.empty() && path.back() == '\0') {
                path.pop_back();
            }
            return NormalizePath(path);
        }
        return "";
    }

    std::string PlatformFileSystem::GetAppDirectory() noexcept {
        wchar_t wpath[MAX_PATH];
        DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", wpath, MAX_PATH);
        if (length == 0 || length >= MAX_PATH) return "";

        std::wstring ws(wpath, length);
        int utf8Len = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (utf8Len > 0) {
            std::string path(utf8Len, '\0');
            WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, &path[0], utf8Len, nullptr, nullptr);
            if (!path.empty() && path.back() == '\0') {
                path.pop_back();
            }
            return NormalizePath(path);
        }
        return "";
    }

    std::string PlatformFileSystem::GetTempDirectory() noexcept {
        wchar_t wpath[MAX_PATH];
        DWORD length = GetTempPathW(MAX_PATH, wpath);
        if (length == 0 || length >= MAX_PATH) return "";

        std::wstring ws(wpath, length);
        if (!ws.empty() && ws.back() == L'\\') {
            ws.pop_back(); // strip trailing slash
        }

        int utf8Len = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (utf8Len > 0) {
            std::string path(utf8Len, '\0');
            WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, &path[0], utf8Len, nullptr, nullptr);
            if (!path.empty() && path.back() == '\0') {
                path.pop_back();
            }
            return NormalizePath(path);
        }
        return "";
    }

} // namespace eng::platform

#else

#include <cstdio>

namespace eng::platform {

    // --- PlatformFile POSIX Fallback ---

    PlatformFile::~PlatformFile() noexcept {
        Close();
    }

    PlatformFile::PlatformFile(PlatformFile&& other) noexcept
        : m_Handle(other.m_Handle) {
        other.m_Handle = nullptr;
    }

    PlatformFile& PlatformFile::operator=(PlatformFile&& other) noexcept {
        if (this != &other) {
            Close();
            m_Handle = other.m_Handle;
            other.m_Handle = nullptr;
        }
        return *this;
    }

    eng::core::ResultCode PlatformFile::Open(const std::string& path, FileMode mode, FileShare /*share*/, PlatformFile& outFile) noexcept {
        outFile.Close();

        std::string resolved = PlatformFileSystem::ResolveVirtualPath(path);
        const char* cmode = "rb";
        switch (mode) {
            case FileMode::Read:   cmode = "rb"; break;
            case FileMode::Write:  cmode = "wb"; break;
            case FileMode::Append: cmode = "ab"; break;
        }

        FILE* f = std::fopen(resolved.c_str(), cmode);
        if (!f) {
            return eng::core::ResultCode::Failure;
        }

        outFile.m_Handle = f;
        return eng::core::ResultCode::Success;
    }

    void PlatformFile::Close() noexcept {
        if (m_Handle) {
            std::fclose(static_cast<FILE*>(m_Handle));
            m_Handle = nullptr;
        }
    }

    bool PlatformFile::IsOpen() const noexcept {
        return m_Handle != nullptr;
    }

    uint64_t PlatformFile::GetSize() const noexcept {
        if (!m_Handle) return 0;
        FILE* f = static_cast<FILE*>(m_Handle);
        long current = std::ftell(f);
        if (std::fseek(f, 0, SEEK_END) != 0) return 0;
        long size = std::ftell(f);
        std::fseek(f, current, SEEK_SET);
        return size >= 0 ? static_cast<uint64_t>(size) : 0;
    }

    eng::core::ResultCode PlatformFile::Read(void* buffer, uint64_t bytesToRead, uint64_t& bytesRead) noexcept {
        if (!m_Handle) return eng::core::ResultCode::NotInitialized;
        FILE* f = static_cast<FILE*>(m_Handle);
        size_t read = std::fread(buffer, 1, bytesToRead, f);
        bytesRead = read;
        return eng::core::ResultCode::Success;
    }

    eng::core::ResultCode PlatformFile::Write(const void* data, uint64_t bytesToWrite, uint64_t& bytesWritten) noexcept {
        if (!m_Handle) return eng::core::ResultCode::NotInitialized;
        FILE* f = static_cast<FILE*>(m_Handle);
        size_t written = std::fwrite(data, 1, bytesToWrite, f);
        bytesWritten = written;
        return eng::core::ResultCode::Success;
    }

    eng::core::ResultCode PlatformFile::Seek(int64_t offset, uint64_t& newPosition) noexcept {
        if (!m_Handle) return eng::core::ResultCode::NotInitialized;
        FILE* f = static_cast<FILE*>(m_Handle);
        if (std::fseek(f, offset, SEEK_SET) != 0) {
            return eng::core::ResultCode::Failure;
        }
        long pos = std::ftell(f);
        newPosition = pos >= 0 ? static_cast<uint64_t>(pos) : 0;
        return eng::core::ResultCode::Success;
    }

    // --- PlatformFileSystem POSIX Fallback ---

    std::string PlatformFileSystem::GetExecutableDirectory() noexcept {
        return "/tmp";
    }

    std::string PlatformFileSystem::GetAppDirectory() noexcept {
        return "/tmp";
    }

    std::string PlatformFileSystem::GetTempDirectory() noexcept {
        return "/tmp";
    }

} // namespace eng::platform

#endif

namespace eng::platform {

    // --- Shared Path Translation Implementations ---

    namespace {
        struct ProtocolMapping {
            std::string protocol;
            std::string path;
        };
        std::vector<ProtocolMapping> g_Protocols;
    }

    std::string PlatformFileSystem::NormalizePath(const std::string& path) noexcept {
        std::string result = path;
        for (char& c : result) {
            if (c == '\\') c = '/';
        }
        
        std::string normalized;
        normalized.reserve(result.size());
        
        bool lastWasSlash = false;
        for (size_t i = 0; i < result.size(); ++i) {
            char c = result[i];
            if (c == '/') {
                if (i > 0 && result[i - 1] == ':') {
                    normalized.push_back(c);
                    lastWasSlash = false;
                    continue;
                }
                if (!lastWasSlash) {
                    normalized.push_back(c);
                }
                lastWasSlash = true;
            } else {
                normalized.push_back(c);
                lastWasSlash = false;
            }
        }
        return normalized;
    }

    void PlatformFileSystem::RegisterProtocol(const std::string& protocol, const std::string& absolutePath) noexcept {
        std::string prefix = protocol;
        if (prefix.find("://") == std::string::npos) {
            prefix += "://";
        }
        std::string normalizedPath = NormalizePath(absolutePath);
        if (!normalizedPath.empty() && normalizedPath.back() != '/') {
            normalizedPath += '/';
        }

        for (auto& mapping : g_Protocols) {
            if (mapping.protocol == prefix) {
                mapping.path = normalizedPath;
                return;
            }
        }
        g_Protocols.push_back({ prefix, normalizedPath });
    }

    std::string PlatformFileSystem::ResolveVirtualPath(const std::string& path) noexcept {
        std::string normalized = NormalizePath(path);
        for (const auto& mapping : g_Protocols) {
            if (normalized.rfind(mapping.protocol, 0) == 0) {
                std::string remainder = normalized.substr(mapping.protocol.size());
                if (!remainder.empty() && remainder.front() == '/') {
                    remainder = remainder.substr(1);
                }
                return mapping.path + remainder;
            }
        }
        return normalized;
    }

} // namespace eng::platform
