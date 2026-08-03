#include "Editor/Panels/AssetBrowserPanel.h"
#include "Runtime/AssetRegistry.h"
#include "ECS/Coordinator.h"
#include "ECS/ECSComponents.h"
#include "ECS/Public/IECSWorld.h"
#include "ThirdParty/imgui/imgui.h"
#include "Core/Logging/Logger.h"
#include <string>
#include <filesystem>
#include <fstream>

namespace eng::runtime {

    void AssetBrowserPanel::Initialize(RuntimeContext* context) {
        m_Context = context;
    }

    void AssetBrowserPanel::Render(EditorSelection& selection, EditorDirtyState& dirtyState, std::function<void(AssetHandle)> onCreateEntityFromMesh) {
        ImGui::Begin("Asset Browser");

        if (!m_Context || !m_Context->assetRegistry) {
            ImGui::TextDisabled("No active Asset Registry found.");
            ImGui::End();
            return;
        }

        auto* registry = m_Context->assetRegistry;

        // 1. Filter Bar
        ImGui::Text("Filter:");
        ImGui::SameLine();
        ImGui::RadioButton("All", &m_FilterType, 0);
        ImGui::SameLine();
        ImGui::RadioButton("Mesh", &m_FilterType, 1);
        ImGui::SameLine();
        ImGui::RadioButton("Material", &m_FilterType, 2);
        ImGui::SameLine();
        ImGui::RadioButton("Texture", &m_FilterType, 3);

        ImGui::SameLine();
        ImGui::Spacing();
        ImGui::SameLine();
        if (ImGui::Button("Rescan Assets")) {
            registry->ScanProjectAssets();
            CORE_LOG_INFO("[AssetBrowser] Rescanned project assets.");
        }

        ImGui::Separator();
        ImGui::Spacing();

        // Determine what type to filter for
        AssetType targetFilter = AssetType::Unknown;
        if (m_FilterType == 1) targetFilter = AssetType::Mesh;
        else if (m_FilterType == 2) targetFilter = AssetType::Material;
        else if (m_FilterType == 3) targetFilter = AssetType::Texture;

        // 2. Action Bar
        Entity selectedEntity = selection.GetSelectedEntity();
        bool hasSelection = (selectedEntity != 0 && m_Context->ecs && m_Context->ecs->getCoordinator().IsEntityAlive(selectedEntity));
        bool isPlaying = (m_Context->editorSimulationState == EditorSimulationState::Play);
        if (isPlaying) {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Asset assignment disabled during Play Mode.");
        }
        bool canAssign = hasSelection && m_SelectedAsset.IsValid() && !isPlaying;

        // Find details of selected asset
        const AssetMetadata* selectedMeta = nullptr;
        if (m_SelectedAsset.IsValid()) {
            selectedMeta = registry->GetMetadata(m_SelectedAsset);
        }

        if (!canAssign) {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button("Assign to Selected Entity")) {
            if (selectedMeta && m_Context->ecs) {
                auto& coordinator = m_Context->ecs->getCoordinator();
                if (selectedMeta->type == AssetType::Mesh) {
                    coordinator.AddComponent<RenderableMeshComponent>(selectedEntity, RenderableMeshComponent(m_SelectedAsset));
                    dirtyState.MarkSceneDirty();
                    CORE_LOG_INFO("[AssetBrowser] Assigned mesh asset to Entity %u", selectedEntity);
                } else if (selectedMeta->type == AssetType::Material) {
                    coordinator.AddComponent<MaterialComponent>(selectedEntity, MaterialComponent(m_SelectedAsset));
                    dirtyState.MarkSceneDirty();
                    CORE_LOG_INFO("[AssetBrowser] Assigned material asset to Entity %u", selectedEntity);
                }
            }
        }
        if (!canAssign) {
            ImGui::EndDisabled();
        }

        ImGui::SameLine();
        bool canCreate = m_SelectedAsset.IsValid() && selectedMeta && selectedMeta->type == AssetType::Mesh && !isPlaying;
        if (!canCreate) {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button("Create Entity From Selected Asset")) {
            if (onCreateEntityFromMesh) {
                onCreateEntityFromMesh(m_SelectedAsset);
            }
        }
        if (!canCreate) {
            ImGui::EndDisabled();
        }

        ImGui::Spacing();

        // 3. Asset Table
        if (ImGui::BeginTable("AssetRegistryTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY, ImVec2(0.0f, -ImGui::GetFrameHeightWithSpacing()))) {
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("Type");
            ImGui::TableSetupColumn("Handle");
            ImGui::TableSetupColumn("Path");
            ImGui::TableHeadersRow();

            const auto& assets = registry->GetAssets();
            for (const auto& [handle, meta] : assets) {
                // Filter logic
                if (m_FilterType != 0 && meta.type != targetFilter) {
                    continue;
                }

                ImGui::TableNextRow();

                // Column 1: Name (Extracted from path)
                ImGui::TableNextColumn();
                std::string filename = std::filesystem::path(meta.sourcePath).filename().string();
                if (filename.empty()) {
                    filename = "Asset_" + std::to_string(handle.value);
                }

                bool isSelected = (m_SelectedAsset == handle);
                ImGuiSelectableFlags selectFlags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick;
                if (ImGui::Selectable(filename.c_str(), isSelected, selectFlags)) {
                    m_SelectedAsset = handle;

                    // Double click assignment
                    if (ImGui::IsMouseDoubleClicked(0) && hasSelection && !isPlaying) {
                        if (m_Context->ecs) {
                            auto& coordinator = m_Context->ecs->getCoordinator();
                            if (meta.type == AssetType::Mesh) {
                                coordinator.AddComponent<RenderableMeshComponent>(selectedEntity, RenderableMeshComponent(handle));
                                dirtyState.MarkSceneDirty();
                                CORE_LOG_INFO("[AssetBrowser] Double-click assigned mesh asset to Entity %u", selectedEntity);
                            } else if (meta.type == AssetType::Material) {
                                coordinator.AddComponent<MaterialComponent>(selectedEntity, MaterialComponent(handle));
                                dirtyState.MarkSceneDirty();
                                CORE_LOG_INFO("[AssetBrowser] Double-click assigned material asset to Entity %u", selectedEntity);
                            }
                        }
                    }
                }

                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                    ImGui::SetDragDropPayload("DRAG_DROP_ASSET_HANDLE", &handle.value, sizeof(uint64_t));
                    ImGui::Text("Asset: %s", filename.c_str());
                    ImGui::EndDragDropSource();
                }

                // Column 2: Type
                ImGui::TableNextColumn();
                ImGui::Text("%s", AssetTypeToString(meta.type));

                // Column 3: Handle
                ImGui::TableNextColumn();
                ImGui::Text("%llu", handle.value);

                // Column 4: Path
                ImGui::TableNextColumn();
                ImGui::Text("%s", meta.sourcePath.c_str());
            }

            ImGui::EndTable();
        }

        if (ImGui::BeginPopupContextWindow("AssetBrowserContextMenu", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
            if (ImGui::MenuItem("New Material Asset (.omat)")) {
                std::filesystem::create_directories("Assets/Materials");
                std::ofstream f("Assets/Materials/NewMaterial.omat");
                if (f.is_open()) {
                    f << "{\n  \"name\": \"NewMaterial\",\n  \"albedo\": [0.8, 0.8, 0.8, 1.0],\n  \"roughness\": 0.5,\n  \"metallic\": 0.0\n}\n";
                    f.close();
                }
                registry->ScanProjectAssets();
            }
            if (ImGui::MenuItem("New Scene Template (.json)")) {
                std::filesystem::create_directories("Assets/Scenes");
                std::ofstream f("Assets/Scenes/NewScene.json");
                if (f.is_open()) {
                    f << "{\n  \"name\": \"NewScene\",\n  \"entities\": []\n}\n";
                    f.close();
                }
                registry->ScanProjectAssets();
            }
            ImGui::EndPopup();
        }

        ImGui::End();
    }

} // namespace eng::runtime
