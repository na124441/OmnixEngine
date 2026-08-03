#pragma once

#include "Runtime/ConfigSystem.h"
#include "Runtime/PluginManager.h"
#include "Runtime/RuntimeConsole.h"
#include "Runtime/TimeManager.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace eng::runtime {

    // -------------------------------------------------------------------------
    // 1. Localization Subsystem
    // -------------------------------------------------------------------------
    class LocalizationSystem {
    public:
        void SetLanguage(const std::string& languageCode) {
            m_CurrentLanguage = languageCode;
        }

        const std::string& GetCurrentLanguage() const {
            return m_CurrentLanguage;
        }

        void LoadStringTable(const std::string& languageCode, const std::unordered_map<std::string, std::string>& table) {
            m_Tables[languageCode] = table;
        }

        std::string GetLocalizedString(const std::string& key, const std::string& defaultVal = "") const {
            auto tableIt = m_Tables.find(m_CurrentLanguage);
            if (tableIt != m_Tables.end()) {
                auto strIt = tableIt->second.find(key);
                if (strIt != tableIt->second.end()) {
                    return strIt->second;
                }
            }
            return defaultVal.empty() ? key : defaultVal;
        }

    private:
        std::string m_CurrentLanguage = "en-US";
        std::unordered_map<std::string, std::unordered_map<std::string, std::string>> m_Tables;
    };

    // -------------------------------------------------------------------------
    // 2. Crash Handler Subsystem
    // -------------------------------------------------------------------------
    struct CrashDumpInfo {
        std::string dumpFilePath;
        std::string exceptionReason;
        uint32_t threadId = 0;
        bool dumpGenerated = false;
    };

    class CrashHandler {
    public:
        using CrashCallback = std::function<void(const CrashDumpInfo&)>;

        static void SetCrashCallback(CrashCallback callback) {
            GetCallback() = callback;
        }

        static CrashDumpInfo GenerateMinidump(const std::string& dumpPath, const std::string& reason) {
            CrashDumpInfo info;
            info.dumpFilePath = dumpPath;
            info.exceptionReason = reason;
            info.threadId = 1234;
            info.dumpGenerated = true;

            if (GetCallback()) {
                GetCallback()(info);
            }
            return info;
        }

    private:
        static CrashCallback& GetCallback() {
            static CrashCallback s_Callback;
            return s_Callback;
        }
    };

    // -------------------------------------------------------------------------
    // 3. Dynamic Version & Feature Flags Subsystem
    // -------------------------------------------------------------------------
    class FeatureFlagSystem {
    public:
        void SetFeatureFlag(const std::string& flagName, bool enabled) {
            m_Flags[flagName] = enabled;
        }

        bool IsFeatureEnabled(const std::string& flagName, bool defaultVal = false) const {
            auto it = m_Flags.find(flagName);
            return (it != m_Flags.end()) ? it->second : defaultVal;
        }

        void ToggleFeature(const std::string& flagName) {
            m_Flags[flagName] = !IsFeatureEnabled(flagName);
        }

        size_t GetFlagCount() const { return m_Flags.size(); }

    private:
        std::unordered_map<std::string, bool> m_Flags;
    };

    // -------------------------------------------------------------------------
    // 4. Save System Verification Wrapper
    // -------------------------------------------------------------------------
    struct SaveDataHeader {
        uint32_t version = 1;
        uint64_t crc64Checksum = 0;
        std::string playerName = "Hero";
        int playerHealth = 100;
        int activeCheckpointId = 1;
    };

    class SaveSystem {
    public:
        static uint64_t ComputeChecksum(const std::string& dataStr) {
            uint64_t crc = 0xFFFFFFFFFFFFFFFFULL;
            for (char c : dataStr) {
                crc ^= static_cast<uint64_t>(static_cast<unsigned char>(c));
                for (int i = 0; i < 8; ++i) {
                    if (crc & 1) crc = (crc >> 1) ^ 0xC96C57305C450923ULL;
                    else crc >>= 1;
                }
            }
            return ~crc;
        }
    };

} // namespace eng::runtime
