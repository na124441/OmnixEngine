#pragma once
#include <filesystem>
#include <unordered_map>
#include <vector>
#include <functional>
#include <chrono>
#include <string>

namespace eng::runtime {

    class FileWatcher
    {
    public:
        using FileCallback = std::function<void(const std::filesystem::path& path)>;

        FileWatcher() = default;
        ~FileWatcher() = default;

        void WatchDirectory(const std::filesystem::path& path);
        void SetCallback(FileCallback callback) { m_Callback = callback; }

        /**
         * @brief Scans watched directories, detects write time changes, debouncing them
         * to trigger the callback when modifications stabilize.
         */
        void PollChanges();

    private:
        struct PendingChange {
            std::chrono::steady_clock::time_point lastChangeTime;
            bool dispatched = false;
        };

        FileCallback m_Callback;
        std::vector<std::filesystem::path> m_WatchedDirs;
        std::unordered_map<std::string, std::filesystem::file_time_type> m_FileTimes;
        std::unordered_map<std::string, PendingChange> m_PendingChanges;
    };

} // namespace eng::runtime
