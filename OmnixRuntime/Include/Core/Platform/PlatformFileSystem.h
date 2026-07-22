#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "Core/Error/ResultCode.h"

namespace eng::platform {

    enum class FileMode {
        Read,
        Write,
        Append
    };

    enum class FileShare {
        None,
        Read,
        Write,
        All
    };

    class PlatformFile {
    public:
        PlatformFile() noexcept = default;
        ~PlatformFile() noexcept;

        PlatformFile(const PlatformFile&) = delete;
        PlatformFile& operator=(const PlatformFile&) = delete;

        PlatformFile(PlatformFile&& other) noexcept;
        PlatformFile& operator=(PlatformFile&& other) noexcept;

        static eng::core::ResultCode Open(const std::string& path, FileMode mode, FileShare share, PlatformFile& outFile) noexcept;
        void Close() noexcept;

        [[nodiscard]] bool IsOpen() const noexcept;
        [[nodiscard]] uint64_t GetSize() const noexcept;

        eng::core::ResultCode Read(void* buffer, uint64_t bytesToRead, uint64_t& bytesRead) noexcept;
        eng::core::ResultCode Write(const void* data, uint64_t bytesToWrite, uint64_t& bytesWritten) noexcept;

        eng::core::ResultCode Seek(int64_t offset, uint64_t& newPosition) noexcept;

    private:
        void* m_Handle = nullptr;
    };

    class PlatformFileSystem {
    public:
        [[nodiscard]] static std::string NormalizePath(const std::string& path) noexcept;
        [[nodiscard]] static std::string ResolveVirtualPath(const std::string& path) noexcept;
        static void RegisterProtocol(const std::string& protocol, const std::string& absolutePath) noexcept;

        [[nodiscard]] static std::string GetExecutableDirectory() noexcept;
        [[nodiscard]] static std::string GetAppDirectory() noexcept;
        [[nodiscard]] static std::string GetTempDirectory() noexcept;
    };

} // namespace eng::platform
