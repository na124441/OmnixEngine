#include "Runtime/Private/Editor/Panels/SceneHierarchyPanel.h"
#include "Runtime/Public/Editor/EditorEntityCommands.h"
#include "Physics/Public/PhysicsWorld.h"
#include "ECS/Coordinator.h"
#include "ECS/ECSComponents.h"
#include "ECS/Public/IECSWorld.h"
#include "ThirdParty/imgui/imgui.h"
#include "Scene/Scene.h"
#include "Scene/SceneObject.h"
#include "Scene/SceneManager.h"
#include <algorithm>
#include <cstring>

namespace eng::runtime {

    void SceneHierarchyPanel::Initialize(RuntimeContext* context) {
        m_Context = context;
    }

    void SceneHierarchyPanel::DrawNode(::SceneObject* obj, EditorSelection& selection, EditorDirtyState& dirtyState, const std::string& searchFilter) {
        if (!obj) return;
        
        Entity entity = obj->GetECSEntity();
        auto& coordinator = m_Context->ecs->getCoordinator();
        
        std::string entityName = obj->GetName();
        
        // Component Icons: [C] Camera, [L] Light, [M] Mesh, [A] Audio, [T] Trigger Volume, [P] Player Spawn
        std::string prefix = "";
        if (obj->m_HasCameraComponent) prefix += "[C]";
        if (obj->m_HasDirectionalLight || obj->m_HasPointLight || obj->m_HasAmbientLight || obj->m_HasSpotLight) prefix += "[L]";
        if (obj->m_HasRenderableMesh) prefix += "[M]";
        if (obj->m_HasAudioSource) prefix += "[A]";
        if (obj->m_HasTrigger) prefix += "[T]";
        if (obj->m_HasPlayerStart) prefix += "[P]";
        
        std::string displayName = prefix.empty() ? entityName : (prefix + " " + entityName);

        bool hasChildren = obj->HasChildren();
        ImGuiTreeNodeFlags flags = ((selection.GetSelectedEntity() == entity) ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (!hasChildren) {
            flags |= ImGuiTreeNodeFlags_Leaf;
        }

        static Entity renamingEntity = 0;
        static char renameBuffer[128] = "";

        bool isRenamingThis = (renamingEntity == entity && entity != 0);

        bool opened = false;
        if (isRenamingThis) {
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 40.0f);
            if (ImGui::InputText("##rename", renameBuffer, IM_ARRAYSIZE(renameBuffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
                obj->SetName(renameBuffer);
                if (coordinator.IsEntityAlive(entity) && coordinator.GetSignature(entity).test(coordinator.GetComponentType<NameComponent>())) {
                    coordinator.GetComponent<NameComponent>(entity).name = renameBuffer;
                }
                dirtyState.MarkSceneDirty();
                renamingEntity = 0;
            }
            ImGui::SameLine();
            if (ImGui::Button("OK")) {
                obj->SetName(renameBuffer);
                if (coordinator.IsEntityAlive(entity) && coordinator.GetSignature(entity).test(coordinator.GetComponentType<NameComponent>())) {
                    coordinator.GetComponent<NameComponent>(entity).name = renameBuffer;
                }
                dirtyState.MarkSceneDirty();
                renamingEntity = 0;
            }
        } else {
            opened = ImGui::TreeNodeEx((void*)(uintptr_t)entity, flags, "%s", displayName.c_str());
            
            if (ImGui::IsItemClicked()) {
                selection.Select(entity);
            }

            if (ImGui::BeginPopupContextItem()) {
                selection.Select(entity);
                if (ImGui::MenuItem("Rename (F2)")) {
                    renamingEntity = entity;
                    std::strncpy(renameBuffer, entityName.c_str(), sizeof(renameBuffer));
                    renameBuffer[sizeof(renameBuffer) - 1] = '\0';
                }
                if (ImGui::MenuItem("Duplicate (Ctrl+D)")) {
                    EditorEntityCommands::Duplicate(coordinator, entity, dirtyState, selection);
                }
                if (ImGui::MenuItem("Delete (Del)")) {
                    if (m_Context->physicsWorld) {
                        m_Context->physicsWorld->UnregisterEntity(entity);
                    }
                    EditorEntityCommands::Delete(coordinator, entity, dirtyState, selection);
                }
                ImGui::EndPopup();
            }
        }

        // Inline rename trigger on F2 if selected
        if (selection.GetSelectedEntity() == entity && !isRenamingThis && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::IsKeyPressed(ImGuiKey_F2)) {
            renamingEntity = entity;
            std::strncpy(renameBuffer, entityName.c_str(), sizeof(renameBuffer));
            renameBuffer[sizeof(renameBuffer) - 1] = '\0';
        }

        if (opened) {
            if (!isRenamingThis) {
                for (SceneObject* child : obj->GetChildren()) {
                    DrawNode(child, selection, dirtyState, searchFilter);
                }
            }
            ImGui::TreePop();
        }
    }

    void SceneHierarchyPanel::Render(EditorSelection& selection, EditorDirtyState& dirtyState) {
        ImGui::Begin("Scene Hierarchy");

        if (!m_Context || !m_Context->ecs) {
            ImGui::Text("No active ECS Coordinator");
            ImGui::End();
            return;
        }

        auto& coordinator = m_Context->ecs->getCoordinator();
        Entity selectedEntity = selection.GetSelectedEntity();

        bool isPlaying = (m_Context->editorSimulationState == EditorSimulationState::Play);
        if (isPlaying) {
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "PLAY MODE - Viewing Runtime Scene");
            ImGui::BeginDisabled();
        }

        // CRUD buttons
        if (ImGui::Button("Create Empty")) {
            EditorEntityCommands::CreateEmpty(coordinator, dirtyState, selection);
        }
        ImGui::SameLine();
        if (ImGui::Button("Create Player Start")) {
            EditorEntityCommands::CreatePlayerStart(coordinator, dirtyState, selection);
        }
        
        ImGui::SameLine();
        bool hasSelection = (selectedEntity != 0 && coordinator.IsEntityAlive(selectedEntity));
        if (!hasSelection) {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button("Delete")) {
            if (m_Context->physicsWorld) {
                m_Context->physicsWorld->UnregisterEntity(selectedEntity);
            }
            EditorEntityCommands::Delete(coordinator, selectedEntity, dirtyState, selection);
            hasSelection = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Duplicate")) {
            EditorEntityCommands::Duplicate(coordinator, selectedEntity, dirtyState, selection);
        }
        if (!hasSelection) {
            ImGui::EndDisabled();
        }

        if (isPlaying) {
            ImGui::EndDisabled();
        }

        ImGui::Separator();

        // Search bar
        ImGui::Text("Search:"); ImGui::SameLine();
        ImGui::InputText("##hierarchy_search", m_SearchBuffer, IM_ARRAYSIZE(m_SearchBuffer));
        ImGui::Separator();

        // Hotkeys (Delete, Ctrl+D duplication)
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::GetIO().WantTextInput) {
            if (ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Backspace)) {
                if (hasSelection) {
                    if (m_Context->physicsWorld) {
                        m_Context->physicsWorld->UnregisterEntity(selectedEntity);
                    }
                    EditorEntityCommands::Delete(coordinator, selectedEntity, dirtyState, selection);
                    hasSelection = false;
                }
            }
            if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && ImGui::IsKeyPressed(ImGuiKey_D)) {
                if (hasSelection) {
                    EditorEntityCommands::Duplicate(coordinator, selectedEntity, dirtyState, selection);
                }
            }
        }

        // Render nodes
        auto* sceneMgr = dynamic_cast<SceneManager*>(m_Context->scenes);
        Scene* activeScene = sceneMgr ? sceneMgr->GetActiveScene() : nullptr;

        std::string searchStr(m_SearchBuffer);
        std::transform(searchStr.begin(), searchStr.end(), searchStr.begin(), ::tolower);

        if (activeScene) {
            if (searchStr.empty()) {
                for (const auto& rootObj : activeScene->GetRootObjects()) {
                    DrawNode(rootObj.get(), selection, dirtyState, searchStr);
                }
            } else {
                for (const auto& obj : activeScene->GetAllSceneObjects()) {
                    std::string nameLower = obj->GetName();
                    std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                    if (nameLower.find(searchStr) != std::string::npos) {
                        Entity entity = obj->GetECSEntity();
                        ImGuiTreeNodeFlags flags = ((selectedEntity == entity) ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_Leaf;
                        
                        std::string prefix = "";
                        if (obj->m_HasCameraComponent) prefix += "[C]";
                        if (obj->m_HasDirectionalLight || obj->m_HasPointLight || obj->m_HasAmbientLight || obj->m_HasSpotLight) prefix += "[L]";
                        if (obj->m_HasRenderableMesh) prefix += "[M]";
                        if (obj->m_HasAudioSource) prefix += "[A]";
                        if (obj->m_HasTrigger) prefix += "[T]";
                        if (obj->m_HasPlayerStart) prefix += "[P]";
                        
                        std::string displayName = prefix.empty() ? obj->GetName() : (prefix + " " + obj->GetName());
                        
                        bool opened = ImGui::TreeNodeEx((void*)(uintptr_t)entity, flags, "%s", displayName.c_str());
                        if (ImGui::IsItemClicked()) {
                            selection.Select(entity);
                        }
                        if (opened) {
                            ImGui::TreePop();
                        }
                    }
                }
            }
        } else {
            ImGui::Text("No active scene");
        }

        // Context menu on empty space
        if (ImGui::BeginPopupContextWindow(nullptr, ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
            if (ImGui::MenuItem("Create Empty Entity")) {
                EditorEntityCommands::CreateEmpty(coordinator, dirtyState, selection);
            }
            if (ImGui::MenuItem("Create Player Start")) {
                EditorEntityCommands::CreatePlayerStart(coordinator, dirtyState, selection);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Create Directional Light")) {
                EditorEntityCommands::CreateDirectionalLight(coordinator, dirtyState, selection);
            }
            if (ImGui::MenuItem("Create Point Light")) {
                EditorEntityCommands::CreatePointLight(coordinator, dirtyState, selection);
            }
            if (ImGui::MenuItem("Create Ambient Light")) {
                EditorEntityCommands::CreateAmbientLight(coordinator, dirtyState, selection);
            }
            if (ImGui::MenuItem("Create Spot Light")) {
                EditorEntityCommands::CreateSpotLight(coordinator, dirtyState, selection);
            }
            ImGui::EndPopup();
        }

        ImGui::End();
    }

} // namespace eng::runtime
