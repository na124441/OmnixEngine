#include "Editor/Panels/CommandPalette.h"
#include "ThirdParty/imgui/imgui.h"
#include "ThirdParty/imgui/imgui_internal.h"
#include "Scene/SceneManager.h"
#include "Scene/Scene.h"
#include "Scene/SceneObject.h"
#include "Runtime/AssetRegistry.h"
#include "Runtime/CVarSystem.h"
#include "Core/Logging/Logger.h"
#include <algorithm>
#include <cctype>

namespace eng::runtime {

    namespace {
        std::string ToLower(const std::string& str) {
            std::string lower = str;
            std::transform(lower.begin(), lower.end(), lower.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            return lower;
        }
    }

    void CommandPalette::Initialize(RuntimeContext* context) {
        m_Context = context;
        m_IsOpen = false;
        m_SearchBuffer[0] = '\0';
        m_SelectedIndex = 0;
    }

    void CommandPalette::ToggleOpen() {
        if (m_IsOpen) {
            Close();
        } else {
            Open();
        }
    }

    void CommandPalette::Open() {
        m_IsOpen = true;
        m_FocusInput = true;
        m_SearchBuffer[0] = '\0';
        m_SelectedIndex = 0;
        RebuildSearchIndex();
    }

    void CommandPalette::Close() {
        m_IsOpen = false;
    }

    void CommandPalette::RebuildSearchIndex() {
        m_AllItems.clear();
        m_FilteredItems.clear();

        if (!m_Context) return;

        // 1. Static Action Commands
        m_AllItems.push_back(CommandItem{"Save Scene", "Action", CommandItemType::Action, 0, {}});
        m_AllItems.push_back(CommandItem{"Load Scene", "Action", CommandItemType::Action, 0, {}});
        m_AllItems.push_back(CommandItem{"Toggle Physics Colliders", "Action", CommandItemType::Action, 0, {}});
        m_AllItems.push_back(CommandItem{"Toggle Grid", "Action", CommandItemType::Action, 0, {}});
        m_AllItems.push_back(CommandItem{"Reset Window Layout", "Action", CommandItemType::Action, 0, {}});
        m_AllItems.push_back(CommandItem{"Create Empty Entity", "Action", CommandItemType::Action, 0, {}});

        // 2. Index Scene Entities
        if (m_Context->scenes) {
            auto* sceneMgr = dynamic_cast<SceneManager*>(m_Context->scenes);
            if (sceneMgr && sceneMgr->GetActiveScene()) {
                const auto& objects = sceneMgr->GetActiveScene()->GetAllSceneObjects();
                for (const auto& obj : objects) {
                    if (obj) {
                        std::string name = obj->GetName();
                        if (name.empty()) name = "Entity " + std::to_string(obj->GetID());
                        m_AllItems.push_back(CommandItem{name, "Entity", CommandItemType::Entity, static_cast<uint64_t>(obj->GetID()), {}});
                    }
                }
            }
        }

        // 3. Index Registered Assets
        if (m_Context->assetRegistry) {
            const auto& assets = m_Context->assetRegistry->GetAssets();
            for (const auto& [handle, meta] : assets) {
                std::string path = meta.sourcePath;
                if (path.empty()) path = meta.importedPath;
                if (path.empty()) path = "Asset #" + std::to_string(handle.value);

                std::string cat = "Asset (" + std::string(AssetTypeToString(meta.type)) + ")";
                m_AllItems.push_back(CommandItem{path, cat, CommandItemType::Asset, handle.value, {}});
            }
        }

        // 4. Index CVars / Console Commands
        if (m_Context->cvarSystem) {
            const auto& cvars = m_Context->cvarSystem->GetCVars();
            for (const auto& [name, cvar] : cvars) {
                std::string label = "cvar: " + name + " = " + m_Context->cvarSystem->GetValueAsString(name);
                m_AllItems.push_back(CommandItem{label, "CVar", CommandItemType::ConsoleCommand, 0, {}});
            }
        }

        m_FilteredItems = m_AllItems;
    }

    void CommandPalette::Render(EditorSelection& selection, EditorDirtyState& dirtyState) {
        if (!m_IsOpen) return;

        ImGui::OpenPopup("Command Palette");

        ImGui::SetNextWindowSize(ImVec2(650, 400), ImGuiCond_Appearing);
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(ImVec2(center.x, center.y - 100), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;

        if (ImGui::BeginPopupModal("Command Palette", &m_IsOpen, flags)) {
            // Search Input Field
            if (m_FocusInput) {
                ImGui::SetKeyboardFocusHere();
                m_FocusInput = false;
            }

            ImGui::PushItemWidth(-1.0f);
            if (ImGui::InputText("##SearchInput", m_SearchBuffer, sizeof(m_SearchBuffer))) {
                std::string query = ToLower(m_SearchBuffer);
                m_FilteredItems.clear();
                if (query.empty()) {
                    m_FilteredItems = m_AllItems;
                } else {
                    for (const auto& item : m_AllItems) {
                        if (ToLower(item.label).find(query) != std::string::npos ||
                            ToLower(item.category).find(query) != std::string::npos) {
                            m_FilteredItems.push_back(item);
                        }
                    }
                }
                m_SelectedIndex = 0;
            }
            ImGui::PopItemWidth();

            // Navigation Handling
            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
                m_SelectedIndex = std::max(0, m_SelectedIndex - 1);
            }
            if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
                m_SelectedIndex = std::min((int)m_FilteredItems.size() - 1, m_SelectedIndex + 1);
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Enter) && !m_FilteredItems.empty()) {
                if (m_SelectedIndex >= 0 && m_SelectedIndex < (int)m_FilteredItems.size()) {
                    ExecuteSelected(m_FilteredItems[m_SelectedIndex], selection);
                    Close();
                    ImGui::CloseCurrentPopup();
                }
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                Close();
                ImGui::CloseCurrentPopup();
            }

            ImGui::Separator();

            // Render Filtered Results List
            if (ImGui::BeginChild("ResultList", ImVec2(0, 300), true)) {
                for (int i = 0; i < (int)m_FilteredItems.size(); ++i) {
                    const auto& item = m_FilteredItems[i];
                    bool isSelected = (i == m_SelectedIndex);

                    ImGui::PushID(i);
                    std::string displayText = "[" + item.category + "] " + item.label;
                    if (ImGui::Selectable(displayText.c_str(), isSelected)) {
                        m_SelectedIndex = i;
                        ExecuteSelected(item, selection);
                        Close();
                        ImGui::CloseCurrentPopup();
                    }
                    if (isSelected) {
                        ImGui::SetScrollHereY();
                    }
                    ImGui::PopID();
                }
                ImGui::EndChild();
            }

            ImGui::EndPopup();
        }
    }

    void CommandPalette::ExecuteSelected(const CommandItem& item, EditorSelection& selection) {
        if (item.type == CommandItemType::Entity) {
            selection.Select(static_cast<Entity>(item.id));
            CORE_LOG_INFO("[CommandPalette] Selected Entity ID: %llu", item.id);
        } else if (item.type == CommandItemType::Asset) {
            CORE_LOG_INFO("[CommandPalette] Selected Asset Handle: %llu", item.id);
        } else if (item.action) {
            item.action();
        }
    }

} // namespace eng::runtime
