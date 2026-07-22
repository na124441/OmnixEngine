#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include <system_error>
#include "Core/Error/Result.h"

namespace eng::core::fs {

    // Safe, exception-free filesystem operations returning custom Result
    bool Exists(const std::filesystem::path& path) noexcept;
    bool CreateDir(const std::filesystem::path& path) noexcept;
    bool RemoveFile(const std::filesystem::path& path) noexcept;

    Expected<uint64_t, std::error_code> GetSize(const std::filesystem::path& path) noexcept;
    Expected<std::string, std::error_code> ReadText(const std::filesystem::path& path) noexcept;
    Expected<std::vector<uint8_t>, std::error_code> ReadBinary(const std::filesystem::path& path) noexcept;

    Expected<void, std::error_code> WriteText(const std::filesystem::path& path, const std::string& content) noexcept;
    Expected<void, std::error_code> WriteBinary(const std::filesystem::path& path, const std::vector<uint8_t>& data) noexcept;

} // namespace eng::core::fs
