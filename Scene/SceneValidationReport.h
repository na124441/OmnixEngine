#pragma once
#include <string>
#include <vector>

enum class SceneValidationSeverity {
    Info,
    Warning,
    Error,
    Fatal
};

struct SceneValidationIssue {
    SceneValidationSeverity severity;
    std::string code;
    std::string message;
    std::string path;
    std::string entityName;
};

struct SceneValidationReport {
    bool valid = true;
    std::vector<SceneValidationIssue> issues;

    bool HasErrors() const {
        for (const auto& issue : issues) {
            if (issue.severity == SceneValidationSeverity::Error || issue.severity == SceneValidationSeverity::Fatal) {
                return true;
            }
        }
        return false;
    }

    bool HasFatalErrors() const {
        for (const auto& issue : issues) {
            if (issue.severity == SceneValidationSeverity::Fatal) {
                return true;
            }
        }
        return false;
    }

    void AddInfo(const std::string& code, const std::string& message, const std::string& path = "", const std::string& entity = "") {
        issues.push_back({SceneValidationSeverity::Info, code, message, path, entity});
    }

    void AddWarning(const std::string& code, const std::string& message, const std::string& path = "", const std::string& entity = "") {
        issues.push_back({SceneValidationSeverity::Warning, code, message, path, entity});
    }

    void AddError(const std::string& code, const std::string& message, const std::string& path = "", const std::string& entity = "") {
        issues.push_back({SceneValidationSeverity::Error, code, message, path, entity});
        valid = false;
    }

    void AddFatal(const std::string& code, const std::string& message, const std::string& path = "", const std::string& entity = "") {
        issues.push_back({SceneValidationSeverity::Fatal, code, message, path, entity});
        valid = false;
    }

    std::string ToString() const {
        std::string result;
        for (const auto& issue : issues) {
            std::string sevStr;
            switch (issue.severity) {
                case SceneValidationSeverity::Info: sevStr = "[Info]"; break;
                case SceneValidationSeverity::Warning: sevStr = "[Warning]"; break;
                case SceneValidationSeverity::Error: sevStr = "[Error]"; break;
                case SceneValidationSeverity::Fatal: sevStr = "[Fatal]"; break;
            }
            result += sevStr + " " + issue.code;
            if (!issue.entityName.empty()) {
                result += " (Entity: " + issue.entityName + ")";
            }
            if (!issue.path.empty()) {
                result += " at " + issue.path;
            }
            result += ": " + issue.message + "\n";
        }
        return result;
    }
};
