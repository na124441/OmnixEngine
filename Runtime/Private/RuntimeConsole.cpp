#include "Runtime/Public/RuntimeConsole.h"
#include "Runtime/Public/CVarSystem.h"
#include "ThirdParty/imgui/imgui.h"
#include "Core/Logging/Logger.h"
#include <sstream>
#include <algorithm>

namespace eng::runtime {

    static std::vector<std::string> Tokenize(const std::string& str) {
        std::vector<std::string> tokens;
        std::string token = "";
        bool inQuotes = false;

        for (char c : str) {
            if (c == '\"') {
                inQuotes = !inQuotes;
            } else if (std::isspace(c) && !inQuotes) {
                if (!token.empty()) {
                    tokens.push_back(token);
                    token.clear();
                }
            } else {
                token += c;
            }
        }
        if (!token.empty()) {
            tokens.push_back(token);
        }
        return tokens;
    }

    void RuntimeConsole::Initialize(CVarSystem* cvarSystem) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_CVarSystem = cvarSystem;

        // Register default commands (T1.1.16)
        RegisterCommand("help", [this](const std::vector<std::string>& args) {
            std::lock_guard<std::mutex> innerLock(m_Mutex);
            AddLog("=== Available Commands ===");
            for (const auto& [name, cmd] : m_Commands) {
                AddLog("  " + name + " - " + cmd.description);
            }
            if (m_CVarSystem) {
                AddLog("=== Available CVars ===");
                for (const auto& [name, cvar] : m_CVarSystem->GetCVars()) {
                    AddLog("  " + name + " - " + cvar.description);
                }
            }
        }, "Prints all commands and CVars.");

        RegisterCommand("clear", [this](const std::vector<std::string>& args) {
            std::lock_guard<std::mutex> innerLock(m_Mutex);
            m_Logs.clear();
        }, "Clears the console log window.");

        AddLog("Omnix Runtime Console Initialized. Type 'help' for commands.");
    }

    void RuntimeConsole::Shutdown() {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_Commands.clear();
        m_Logs.clear();
        m_History.clear();
    }

    void RuntimeConsole::RegisterCommand(const std::string& name, CommandCallback callback, const std::string& desc) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_Commands[name] = ConsoleCommand{ name, callback, desc };
    }

    void RuntimeConsole::ExecuteCommand(const std::string& cmdLine) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        if (cmdLine.empty()) return;

        AddLog("> " + cmdLine);
        
        m_History.push_back(cmdLine);
        m_HistoryIndex = -1;

        std::vector<std::string> tokens = Tokenize(cmdLine);
        if (tokens.empty()) return;

        std::string cmd = tokens[0];
        std::vector<std::string> args(tokens.begin() + 1, tokens.end());

        // 1. Check Command
        auto itCmd = m_Commands.find(cmd);
        if (itCmd != m_Commands.end()) {
            // Unlock mutex to avoid deadlocks in case commands query the console state
            m_Mutex.unlock();
            try {
                itCmd->second.callback(args);
            } catch (...) {}
            m_Mutex.lock();
            return;
        }

        // 2. Check CVar (T1.1.17)
        if (m_CVarSystem) {
            const CVar* cvar = m_CVarSystem->GetCVar(cmd);
            if (cvar) {
                if (args.empty()) {
                    AddLog(cmd + " = " + m_CVarSystem->GetValueAsString(cmd));
                } else {
                    if (m_CVarSystem->SetValueFromString(cmd, args[0])) {
                        AddLog(cmd + " set to " + m_CVarSystem->GetValueAsString(cmd));
                    } else {
                        AddLog("Failed to set CVar: " + cmd);
                    }
                }
                return;
            }
        }

        AddLog("Unknown command or CVar: " + cmd);
    }

    void RuntimeConsole::AddLog(const std::string& log) {
        m_Logs.push_back(log);
        m_ScrollToBottom = true;
    }

    std::vector<std::string> RuntimeConsole::GetAutoCompleteSuggestions(const std::string& prefix) const {
        std::vector<std::string> suggestions;
        if (prefix.empty()) return suggestions;

        for (const auto& [name, cmd] : m_Commands) {
            if (name.rfind(prefix, 0) == 0) {
                suggestions.push_back(name);
            }
        }

        if (m_CVarSystem) {
            for (const auto& [name, cvar] : m_CVarSystem->GetCVars()) {
                if (name.rfind(prefix, 0) == 0) {
                    suggestions.push_back(name);
                }
            }
        }

        std::sort(suggestions.begin(), suggestions.end());
        return suggestions;
    }

    int RuntimeConsole::HandleInputCallback(ImGuiInputTextCallbackData* data) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        switch (data->EventFlag) {
            case ImGuiInputTextFlags_CallbackCompletion: {
                std::string currentText(data->Buf);
                std::vector<std::string> suggestions = GetAutoCompleteSuggestions(currentText);
                if (!suggestions.empty()) {
                    if (suggestions.size() == 1) {
                        data->DeleteChars(0, data->BufTextLen);
                        data->InsertChars(0, suggestions[0].c_str());
                    } else {
                        AddLog("=== Suggestions ===");
                        for (const auto& s : suggestions) {
                            AddLog("  " + s);
                        }
                    }
                }
                break;
            }
            case ImGuiInputTextFlags_CallbackHistory: {
                const int prevHistoryIndex = m_HistoryIndex;
                if (data->EventKey == ImGuiKey_UpArrow) {
                    if (m_HistoryIndex == -1) {
                        m_HistoryIndex = static_cast<int>(m_History.size()) - 1;
                    } else if (m_HistoryIndex > 0) {
                        m_HistoryIndex--;
                    }
                } else if (data->EventKey == ImGuiKey_DownArrow) {
                    if (m_HistoryIndex != -1) {
                        m_HistoryIndex++;
                        if (m_HistoryIndex >= static_cast<int>(m_History.size())) {
                            m_HistoryIndex = -1;
                        }
                    }
                }

                if (m_HistoryIndex != prevHistoryIndex) {
                    data->DeleteChars(0, data->BufTextLen);
                    if (m_HistoryIndex != -1) {
                        data->InsertChars(0, m_History[m_HistoryIndex].c_str());
                    }
                }
                break;
            }
        }
        return 0;
    }

    void RuntimeConsole::RenderUI(bool* pOpen) {
        if (!*pOpen) return;

        ImGui::SetNextWindowSize(ImVec2(520, 300), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Runtime Console", pOpen)) {
            ImGui::End();
            return;
        }

        const float footerHeightToReserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
        ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footerHeightToReserve), false, ImGuiWindowFlags_HorizontalScrollbar);

        std::lock_guard<std::mutex> lock(m_Mutex);

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));
        for (const auto& log : m_Logs) {
            ImVec4 color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
            if (log.rfind("[Error]", 0) == 0 || log.rfind("[ImportService] [Error]", 0) == 0) {
                color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
            } else if (log.rfind("[Warning]", 0) == 0 || log.rfind("[ImportService] [Warning]", 0) == 0) {
                color = ImVec4(1.0f, 0.8f, 0.4f, 1.0f);
            } else if (log.rfind(">", 0) == 0) {
                color = ImVec4(0.4f, 0.8f, 1.0f, 1.0f);
            }
            ImGui::TextColored(color, "%s", log.c_str());
        }

        if (m_ScrollToBottom) {
            ImGui::SetScrollHereY(1.0f);
            m_ScrollToBottom = false;
        }

        ImGui::PopStyleVar();
        ImGui::EndChild();
        ImGui::Separator();

        bool reclaimFocus = false;
        ImGuiInputTextFlags inputTextFlags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory;
        
        auto callback = [](ImGuiInputTextCallbackData* data) -> int {
            RuntimeConsole* console = static_cast<RuntimeConsole*>(data->UserData);
            return console->HandleInputCallback(data);
        };

        // We unlock before rendering InputText to avoid deadlock during the callback execution which locks m_Mutex
        m_Mutex.unlock();
        if (ImGui::InputText("Command", m_InputBuf, IM_ARRAYSIZE(m_InputBuf), inputTextFlags, callback, (void*)this)) {
            std::string inputStr(m_InputBuf);
            if (!inputStr.empty()) {
                ExecuteCommand(inputStr);
            }
            m_InputBuf[0] = '\0';
            reclaimFocus = true;
        }
        m_Mutex.lock();

        ImGui::SetItemDefaultFocus();
        if (reclaimFocus) {
            ImGui::SetKeyboardFocusHere(-1);
        }

        ImGui::End();
    }

} // namespace eng::runtime
