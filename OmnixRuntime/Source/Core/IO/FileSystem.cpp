#include "Core/IO/FileSystem.h"
#include <fstream>

namespace eng::core::fs {

    bool Exists(const std::filesystem::path& path) noexcept {
        std::error_code ec;
        return std::filesystem::exists(path, ec);
    }

    bool CreateDir(const std::filesystem::path& path) noexcept {
        std::error_code ec;
        return std::filesystem::create_directories(path, ec);
    }

    bool RemoveFile(const std::filesystem::path& path) noexcept {
        std::error_code ec;
        return std::filesystem::remove(path, ec);
    }

    Expected<uint64_t, std::error_code> GetSize(const std::filesystem::path& path) noexcept {
        std::error_code ec;
        uint64_t size = std::filesystem::file_size(path, ec);
        if (ec) {
            return Unexpected<std::error_code>(ec);
        }
        return size;
    }

    Expected<std::string, std::error_code> ReadText(const std::filesystem::path& path) noexcept {
        if (!Exists(path)) {
            return Unexpected<std::error_code>(std::make_error_code(std::errc::no_such_file_or_directory));
        }

        std::ifstream file(path, std::ios::in | std::ios::binary);
        if (!file.is_open()) {
            return Unexpected<std::error_code>(std::make_error_code(std::errc::permission_denied));
        }

        std::string content;
        file.seekg(0, std::ios::end);
        auto size = file.tellg();
        if (size > 0) {
            content.resize(static_cast<size_t>(size));
            file.seekg(0, std::ios::beg);
            file.read(&content[0], size);
            if (file.fail()) {
                return Unexpected<std::error_code>(std::make_error_code(std::errc::io_error));
            }
        }
        return content;
    }

    Expected<std::vector<uint8_t>, std::error_code> ReadBinary(const std::filesystem::path& path) noexcept {
        if (!Exists(path)) {
            return Unexpected<std::error_code>(std::make_error_code(std::errc::no_such_file_or_directory));
        }

        std::ifstream file(path, std::ios::in | std::ios::binary);
        if (!file.is_open()) {
            return Unexpected<std::error_code>(std::make_error_code(std::errc::permission_denied));
        }

        std::vector<uint8_t> buffer;
        file.seekg(0, std::ios::end);
        auto size = file.tellg();
        if (size > 0) {
            buffer.resize(static_cast<size_t>(size));
            file.seekg(0, std::ios::beg);
            file.read(reinterpret_cast<char*>(buffer.data()), size);
            if (file.fail()) {
                return Unexpected<std::error_code>(std::make_error_code(std::errc::io_error));
            }
        }
        return buffer;
    }

    Expected<void, std::error_code> WriteText(const std::filesystem::path& path, const std::string& content) noexcept {
        std::ofstream file(path, std::ios::out | std::ios::binary);
        if (!file.is_open()) {
            return Unexpected<std::error_code>(std::make_error_code(std::errc::permission_denied));
        }

        if (!content.empty()) {
            file.write(content.data(), content.size());
            if (file.fail()) {
                return Unexpected<std::error_code>(std::make_error_code(std::errc::io_error));
            }
        }
        return Expected<void, std::error_code>();
    }

    Expected<void, std::error_code> WriteBinary(const std::filesystem::path& path, const std::vector<uint8_t>& data) noexcept {
        std::ofstream file(path, std::ios::out | std::ios::binary);
        if (!file.is_open()) {
            return Unexpected<std::error_code>(std::make_error_code(std::errc::permission_denied));
        }

        if (!data.empty()) {
            file.write(reinterpret_cast<const char*>(data.data()), data.size());
            if (file.fail()) {
                return Unexpected<std::error_code>(std::make_error_code(std::errc::io_error));
            }
        }
        return Expected<void, std::error_code>();
    }

} // namespace eng::core::fs
