#include "Runtime/Public/ConfigSystem.h"
#include "Core/Logging/Logger.h"
#include "ThirdParty/rapidjson-master/include/rapidjson/document.h"
#include "ThirdParty/rapidjson-master/include/rapidjson/writer.h"
#include "ThirdParty/rapidjson-master/include/rapidjson/stringbuffer.h"
#include "ThirdParty/rapidjson-master/include/rapidjson/error/en.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <chrono>

namespace eng::runtime {

    bool ConfigSystem::Initialize(int argc, char* argv[],
                                  const std::string& projectConfigPath,
                                  const std::string& userConfigPath) {
        std::lock_guard<std::mutex> lock(m_Mutex);

        m_ProjectConfigPath = projectConfigPath;
        m_UserConfigPath = userConfigPath;

        // Ensure Config directory exists
        std::filesystem::create_directories("Config");

        // Load files
        LoadJsonLayer(m_ProjectConfigPath, m_ProjectConfig);
        LoadJsonLayer(m_UserConfigPath, m_UserOverrides);

        // Parse command line overrides
        ParseCommandLine(argc, argv);

        m_LastProjectWriteTime = GetFileLastWriteTime(m_ProjectConfigPath);
        m_LastUserWriteTime = GetFileLastWriteTime(m_UserConfigPath);

        MergeConfig();
        return true;
    }

    void ConfigSystem::Shutdown() {
        std::lock_guard<std::mutex> lock(m_Mutex);
        SaveUserConfig();
    }

    void ConfigSystem::SetDefault(const std::string& key, const std::string& value) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        auto oldMerged = m_MergedConfig;
        m_Defaults[key] = value;
        MergeConfig();
        NotifyChanges(oldMerged);
    }

    std::string ConfigSystem::GetString(const std::string& key, const std::string& defaultValue) const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        auto it = m_MergedConfig.find(key);
        if (it != m_MergedConfig.end()) {
            return it->second;
        }
        return defaultValue;
    }

    int ConfigSystem::GetInt(const std::string& key, int defaultValue) const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        auto it = m_MergedConfig.find(key);
        if (it != m_MergedConfig.end()) {
            try {
                return std::stoi(it->second);
            } catch (...) {}
        }
        return defaultValue;
    }

    float ConfigSystem::GetFloat(const std::string& key, float defaultValue) const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        auto it = m_MergedConfig.find(key);
        if (it != m_MergedConfig.end()) {
            try {
                return std::stof(it->second);
            } catch (...) {}
        }
        return defaultValue;
    }

    bool ConfigSystem::GetBool(const std::string& key, bool defaultValue) const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        auto it = m_MergedConfig.find(key);
        if (it != m_MergedConfig.end()) {
            std::string val = it->second;
            std::transform(val.begin(), val.end(), val.begin(), ::tolower);
            return (val == "true" || val == "1" || val == "yes" || val == "on");
        }
        return defaultValue;
    }

    void ConfigSystem::SetString(const std::string& key, const std::string& value) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        auto oldMerged = m_MergedConfig;
        m_UserOverrides[key] = value;
        MergeConfig();
        NotifyChanges(oldMerged);
    }

    void ConfigSystem::SetInt(const std::string& key, int value) {
        SetString(key, std::to_string(value));
    }

    void ConfigSystem::SetFloat(const std::string& key, float value) {
        SetString(key, std::to_string(value));
    }

    void ConfigSystem::SetBool(const std::string& key, bool value) {
        SetString(key, value ? "true" : "false");
    }

    void ConfigSystem::CheckForHotReload() {
        std::lock_guard<std::mutex> lock(m_Mutex);

        uint64_t currProjectTime = GetFileLastWriteTime(m_ProjectConfigPath);
        uint64_t currUserTime = GetFileLastWriteTime(m_UserConfigPath);

        bool changed = false;

        if (currProjectTime > m_LastProjectWriteTime) {
            CORE_LOG_INFO("[ConfigSystem] Project config file changed. Hot-reloading...");
            LoadJsonLayer(m_ProjectConfigPath, m_ProjectConfig);
            m_LastProjectWriteTime = currProjectTime;
            changed = true;
        }

        if (currUserTime > m_LastUserWriteTime) {
            CORE_LOG_INFO("[ConfigSystem] User config file changed. Hot-reloading...");
            LoadJsonLayer(m_UserConfigPath, m_UserOverrides);
            m_LastUserWriteTime = currUserTime;
            changed = true;
        }

        if (changed) {
            auto oldMerged = m_MergedConfig;
            MergeConfig();
            NotifyChanges(oldMerged);
        }
    }

    void ConfigSystem::RegisterCallback(const std::string& key, ConfigChangedCallback cb) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_Callbacks[key].push_back(cb);
    }

    bool ConfigSystem::SaveUserConfig() {
        rapidjson::Document doc;
        doc.SetObject();
        auto& allocator = doc.GetAllocator();

        for (const auto& [key, value] : m_UserOverrides) {
            rapidjson::Value k(key.c_str(), allocator);
            rapidjson::Value v(value.c_str(), allocator);
            doc.AddMember(k, v, allocator);
        }

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        doc.Accept(writer);

        std::ofstream file(m_UserConfigPath);
        if (!file.is_open()) {
            CORE_LOG_ERROR("[ConfigSystem] Failed to write user config: %s", m_UserConfigPath.c_str());
            return false;
        }
        file << buffer.GetString();
        m_LastUserWriteTime = GetFileLastWriteTime(m_UserConfigPath);
        return true;
    }

    void ConfigSystem::LoadJsonLayer(const std::string& path, std::unordered_map<std::string, std::string>& layer) {
        layer.clear();
        std::ifstream file(path);
        if (!file.is_open()) return;

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string jsonStr = buffer.str();

        rapidjson::Document doc;
        if (doc.Parse(jsonStr.c_str()).HasParseError()) {
            CORE_LOG_ERROR("[ConfigSystem] JSON parse error in file %s: %s (Offset: %zu)",
                           path.c_str(), rapidjson::GetParseError_En(doc.GetParseError()), doc.GetErrorOffset());
            return;
        }

        if (doc.IsObject()) {
            for (auto it = doc.MemberBegin(); it != doc.MemberEnd(); ++it) {
                if (it->name.IsString()) {
                    std::string key = it->name.GetString();
                    std::string value;
                    if (it->value.IsString()) {
                        value = it->value.GetString();
                    } else if (it->value.IsBool()) {
                        value = it->value.GetBool() ? "true" : "false";
                    } else if (it->value.IsInt()) {
                        value = std::to_string(it->value.GetInt());
                    } else if (it->value.IsDouble()) {
                        value = std::to_string(it->value.GetDouble());
                    }
                    layer[key] = value;
                }
            }
        }
    }

    void ConfigSystem::ParseCommandLine(int argc, char* argv[]) {
        m_CommandLineArgs.clear();
        for (int i = 1; i < argc - 1; ++i) {
            std::string arg = argv[i];
            if (arg.rfind("--", 0) == 0 && arg.size() > 2) {
                std::string key = arg.substr(2);
                std::string value = argv[i + 1];
                m_CommandLineArgs[key] = value;
                i++;
            } else if (arg.rfind("-", 0) == 0 && arg.size() > 1) {
                std::string key = arg.substr(1);
                std::string value = argv[i + 1];
                m_CommandLineArgs[key] = value;
                i++;
            }
        }
    }

    void ConfigSystem::MergeConfig() {
        m_MergedConfig.clear();

        // 1. Defaults
        for (const auto& [k, v] : m_Defaults) {
            m_MergedConfig[k] = v;
        }
        // 2. Project
        for (const auto& [k, v] : m_ProjectConfig) {
            m_MergedConfig[k] = v;
        }
        // 3. User
        for (const auto& [k, v] : m_UserOverrides) {
            m_MergedConfig[k] = v;
        }
        // 4. Command Line
        for (const auto& [k, v] : m_CommandLineArgs) {
            m_MergedConfig[k] = v;
        }
    }

    void ConfigSystem::NotifyChanges(const std::unordered_map<std::string, std::string>& oldMerged) {
        for (const auto& [k, v] : m_MergedConfig) {
            auto itOld = oldMerged.find(k);
            if (itOld == oldMerged.end() || itOld->second != v) {
                auto itCallbacks = m_Callbacks.find(k);
                if (itCallbacks != m_Callbacks.end()) {
                    for (const auto& cb : itCallbacks->second) {
                        try {
                            cb(k, v);
                        } catch (...) {}
                    }
                }
            }
        }
    }

    uint64_t ConfigSystem::GetFileLastWriteTime(const std::string& path) const {
        if (!std::filesystem::exists(path)) return 0;
        try {
            auto time = std::filesystem::last_write_time(path);
            return std::chrono::duration_cast<std::chrono::seconds>(time.time_since_epoch()).count();
        } catch (...) {
            return 0;
        }
    }

} // namespace eng::runtime
