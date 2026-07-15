#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <functional>
#include <mutex>

namespace eng::runtime {

    using ConfigChangedCallback = std::function<void(const std::string& key, const std::string& value)>;

    /**
     * @class ConfigSystem
     * @brief Implements layered configuration (defaults -> project -> user -> CLI args) with hot-reload change notifications.
     */
    class ConfigSystem {
    public:
        ConfigSystem() = default;
        ~ConfigSystem() = default;

        ConfigSystem(const ConfigSystem&) = delete;
        ConfigSystem& operator=(const ConfigSystem&) = delete;

        /**
         * @brief Initialize by loading config files and parsing command-line parameters.
         */
        bool Initialize(int argc, char* argv[],
                        const std::string& projectConfigPath = "Config/engine.json",
                        const std::string& userConfigPath = "Config/user.json");

        void Shutdown();

        /**
         * @brief Set a default value layer fallback.
         */
        void SetDefault(const std::string& key, const std::string& value);

        // Typed Getters (T1.1.6)
        [[nodiscard]] std::string GetString(const std::string& key, const std::string& defaultValue = "") const;
        [[nodiscard]] int GetInt(const std::string& key, int defaultValue = 0) const;
        [[nodiscard]] float GetFloat(const std::string& key, float defaultValue = 0.0f) const;
        [[nodiscard]] bool GetBool(const std::string& key, bool defaultValue = false) const;

        // Setters (Modifies the active user layer)
        void SetString(const std::string& key, const std::string& value);
        void SetInt(const std::string& key, int value);
        void SetFloat(const std::string& key, float value);
        void SetBool(const std::string& key, bool value);

        /**
         * @brief Polls file modification timestamps to trigger config reload callbacks (T1.1.8).
         */
        void CheckForHotReload();

        /**
         * @brief Register a callback to be notified when a specific key changes.
         */
        void RegisterCallback(const std::string& key, ConfigChangedCallback cb);

        /**
         * @brief Saves user overrides back to user.json.
         */
        bool SaveUserConfig();

    private:
        void LoadJsonLayer(const std::string& path, std::unordered_map<std::string, std::string>& layer);
        void ParseCommandLine(int argc, char* argv[]);
        void NotifyChanges(const std::unordered_map<std::string, std::string>& oldMerged);
        void MergeConfig();
        [[nodiscard]] uint64_t GetFileLastWriteTime(const std::string& path) const;

        mutable std::mutex m_Mutex;
        std::string m_ProjectConfigPath;
        std::string m_UserConfigPath;
        uint64_t m_LastProjectWriteTime = 0;
        uint64_t m_LastUserWriteTime = 0;

        // Config Layers (T1.1.7)
        std::unordered_map<std::string, std::string> m_Defaults;
        std::unordered_map<std::string, std::string> m_ProjectConfig;
        std::unordered_map<std::string, std::string> m_UserOverrides;
        std::unordered_map<std::string, std::string> m_CommandLineArgs;

        // Merged Cache
        std::unordered_map<std::string, std::string> m_MergedConfig;

        // Change notification registry
        std::unordered_map<std::string, std::vector<ConfigChangedCallback>> m_Callbacks;
    };

} // namespace eng::runtime
