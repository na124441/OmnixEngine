#include "Runtime/FileWatcher.h"
#include "Core/Logging/Logger.h"

namespace eng::runtime {

    void FileWatcher::WatchDirectory(const std::filesystem::path& path)
    {
        if (std::filesystem::exists(path)) {
            m_WatchedDirs.push_back(path);

            // Populate initial write times of existing files
            std::error_code ec;
            for (const auto& entry : std::filesystem::recursive_directory_iterator(path, ec)) {
                if (ec) continue;
                if (entry.is_regular_file()) {
                    std::error_code timeEc;
                    auto writeTime = std::filesystem::last_write_time(entry.path(), timeEc);
                    if (!timeEc) {
                        m_FileTimes[entry.path().string()] = writeTime;
                    }
                }
            }
        }
    }

    void FileWatcher::PollChanges()
    {
        bool isFirstScan = m_FileTimes.empty() && m_WatchedDirs.size() > 0;

        // 1. Scan watched directories
        for (const auto& dir : m_WatchedDirs) {
            if (!std::filesystem::exists(dir)) continue;

            std::error_code dirEc;
            for (const auto& entry : std::filesystem::recursive_directory_iterator(dir, dirEc)) {
                if (dirEc) continue;

                if (entry.is_regular_file()) {
                    std::string pathStr = entry.path().string();
                    std::error_code timeEc;
                    auto writeTime = std::filesystem::last_write_time(entry.path(), timeEc);
                    if (timeEc) continue;

                    auto it = m_FileTimes.find(pathStr);
                    if (it == m_FileTimes.end()) {
                        m_FileTimes[pathStr] = writeTime;
                        if (!isFirstScan) {
                            m_PendingChanges[pathStr] = { std::chrono::steady_clock::now(), false };
                        }
                    } else if (it->second != writeTime) {
                        m_FileTimes[pathStr] = writeTime;
                        m_PendingChanges[pathStr] = { std::chrono::steady_clock::now(), false };
                    }
                }
            }
        }

        // 2. Process debounced changes
        auto now = std::chrono::steady_clock::now();
        for (auto it = m_PendingChanges.begin(); it != m_PendingChanges.end(); ) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second.lastChangeTime).count();
            if (elapsed >= 300) {
                if (m_Callback) {
                    m_Callback(std::filesystem::path(it->first));
                }
                it = m_PendingChanges.erase(it);
            } else {
                ++it;
            }
        }
    }

} // namespace eng::runtime
