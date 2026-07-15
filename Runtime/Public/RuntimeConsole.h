#pragma once

#include "ThirdParty/imgui/imgui.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <mutex>

namespace eng::runtime {

    class CVarSystem;

    using CommandCallback = std::function<void(const std::vector<std::string>& args)>;

    struct ConsoleCommand {
        std::string name;
        CommandCallback callback;
        std::string description;
    };

    /**
     * @class RuntimeConsole
     * @brief Console overlay UI, command/CVar execution, auto-completion, and command history (T1.1.16, T1.1.17).
     */
    class RuntimeConsole {
    public:
        RuntimeConsole() = default;
        ~RuntimeConsole() = default;

        RuntimeConsole(const RuntimeConsole&) = delete;
        RuntimeConsole& operator=(const RuntimeConsole&) = delete;

        /**
         * @brief Initialize with a pointer to the CVar system to support CVar query and update commands.
         */
        void Initialize(CVarSystem* cvarSystem);
        void Shutdown();

        /**
         * @brief Register a custom command in the console.
         */
        void RegisterCommand(const std::string& name, CommandCallback callback, const std::string& desc = "");

        /**
         * @brief Parse and execute a command string.
         */
        void ExecuteCommand(const std::string& cmdLine);

        /**
         * @brief Render the ImGui overlay console.
         * @param pOpen Controls overlay visibility (e.g. toggled via ~ key).
         */
        void RenderUI(bool* pOpen);

        [[nodiscard]] const std::vector<std::string>& GetLogs() const { return m_Logs; }
        [[nodiscard]] const std::vector<std::string>& GetHistory() const { return m_History; }

    private:
        void AddLog(const std::string& log);
        std::vector<std::string> GetAutoCompleteSuggestions(const std::string& prefix) const;
        int HandleInputCallback(ImGuiInputTextCallbackData* data);

        mutable std::recursive_mutex m_Mutex;
        CVarSystem* m_CVarSystem = nullptr;

        std::unordered_map<std::string, ConsoleCommand> m_Commands;
        std::vector<std::string> m_Logs;
        std::vector<std::string> m_History;
        int m_HistoryIndex = -1;

        char m_InputBuf[256] = "";
        bool m_ScrollToBottom = false;
        std::vector<std::string> m_ActiveSuggestions;
    };

} // namespace eng::runtime
