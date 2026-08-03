#pragma once
#include "Runtime/AssetRegistry.h"
#include <string>
#include <vector>
#include <mutex>

namespace eng::runtime {

enum class ImportLogSeverity {
    Info,
    Warning,
    Error
};

struct ImportLogEntry {
    ImportLogSeverity severity;
    std::string message;
    std::string timestamp;
};

class AssetImportService {
public:
    // Imports a model (.obj) from sourcePath:
    // - copies to Assets/Models/
    // - registers in AssetRegistry
    // - saves AssetRegistry.json
    // Returns the relative path of the imported asset, or empty string on failure.
    static std::string ImportModel(const std::string& sourcePath, AssetRegistry* registry);

    // Logging helpers
    static void LogInfo(const std::string& msg);
    static void LogWarning(const std::string& msg);
    static void LogError(const std::string& msg);
    static void ClearLogs();
    static const std::vector<ImportLogEntry>& GetLogEntries();

    // Drag and drop queue helpers
    static void AddDroppedFile(const std::string& path);
    static bool HasDroppedFiles();
    static std::string PopDroppedFile();

private:
    static std::vector<ImportLogEntry> s_LogEntries;
    static std::vector<std::string> s_DroppedFiles;
    static std::mutex s_LogMutex;
    static std::mutex s_DropMutex;
};

} // namespace eng::runtime
