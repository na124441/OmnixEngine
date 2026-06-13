#include "Runtime/Private/Editor/Panels/SceneHierarchyPanel.h"
#include "Runtime/Public/Editor/EditorEntityCommands.h"
#include "Runtime/Public/Editor/EditorSceneService.h"
#include "Physics/Public/PhysicsWorld.h"
#include "Core/World.h"
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

    void SceneHierarchyPanel::EnsureActiveSceneAndSync() {
        auto* sceneMgr = m_Context ? dynamic_cast<SceneManager*>(m_Context->scenes) : nullptr;
        auto* world = m_Context ? dynamic_cast<World*>(m_Context->ecs) : nullptr;
        EditorSceneService sceneService(sceneMgr, world, nullptr, nullptr, nullptr);
        sceneService.EnsureActiveScene();
        sceneService.SyncAfterMutation("hierarchy panel");
    }

    void SceneHierarchyPanel::DrawNode(::SceneObject* obj, EditorSelection& selection, EditorDirtyState& dirtyState, const std::string& searchFilter) {
        if (!obj) return;
        
        Entity entity = obj->GetECSEntity();
        auto& coordinator = m_Context->ecs->getCoordinator();
        
        std::string entityName = obj->GetName();
        
        // Component Icons: [C] Camera, [L] Light, [M] Mesh, [A] Audio, [T] Trigger Volume, [P] Player Spawn
        std::string prefix = "";
        if (obj->m_HasCameraComponent) prefix += "[C]";
        if (obj->m_HasDirectionalLight || obj->m_HasPointLight || obj->m_HasSkyLight || obj->m_HasSpotLight) prefix += "[L]";
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
                auto* sceneMgr = m_Context ? dynamic_cast<SceneManager*>(m_Context->scenes) : nullptr;
                auto* world = m_Context ? dynamic_cast<World*>(m_Context->ecs) : nullptr;
                EditorSceneService sceneService(sceneMgr, world, &dirtyState, &selection, nullptr);
                sceneService.RenameObject(entity, renameBuffer);
                renamingEntity = 0;
            }
            ImGui::SameLine();
            if (ImGui::Button("OK")) {
                auto* sceneMgr = m_Context ? dynamic_cast<SceneManager*>(m_Context->scenes) : nullptr;
                auto* world = m_Context ? dynamic_cast<World*>(m_Context->ecs) : nullptr;
                EditorSceneService sceneService(sceneMgr, world, &dirtyState, &selection, nullptr);
                sceneService.RenameObject(entity, renameBuffer);
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
                    auto* sceneMgr = m_Context ? dynamic_cast<SceneManager*>(m_Context->scenes) : nullptr;
                    auto* world = m_Context ? dynamic_cast<World*>(m_Context->ecs) : nullptr;
                    EditorSceneService(sceneMgr, world, &dirtyState, &selection, nullptr).DuplicateObject(entity);
                }
                if (ImGui::MenuItem("Delete (Del)")) {
                    if (m_Context->physicsWorld) {
                        m_Context->physicsWorld->UnregisterEntity(entity);
                    }
                    auto* sceneMgr = m_Context ? dynamic_cast<SceneManager*>(m_Context->scenes) : nullptr;
                    auto* world = m_Context ? dynamic_cast<World*>(m_Context->ecs) : nullptr;
                    EditorSceneService(sceneMgr, world, &dirtyState, &selection, nullptr).DeleteObject(entity);
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
            auto* sceneMgr = m_Context ? dynamic_cast<SceneManager*>(m_Context->scenes) : nullptr;
            auto* world = m_Context ? dynamic_cast<World*>(m_Context->ecs) : nullptr;
            EditorSceneService(sceneMgr, world, &dirtyState, &selection, nullptr).CreateEmptyObject();
        }
        ImGui::SameLine();
        if (ImGui::Button("Create Player Start")) {
            auto* sceneMgr = m_Context ? dynamic_cast<SceneManager*>(m_Context->scenes) : nullptr;
            auto* world = m_Context ? dynamic_cast<World*>(m_Context->ecs) : nullptr;
            EditorSceneService(sceneMgr, world, &dirtyState, &selection, nullptr).CreatePlayerStart();
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
            auto* sceneMgr = m_Context ? dynamic_cast<SceneManager*>(m_Context->scenes) : nullptr;
            auto* world = m_Context ? dynamic_cast<World*>(m_Context->ecs) : nullptr;
            EditorSceneService(sceneMgr, world, &dirtyState, &selection, nullptr).DeleteObject(selectedEntity);
            hasSelection = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Duplicate")) {
            auto* sceneMgr = m_Context ? dynamic_cast<SceneManager*>(m_Context->scenes) : nullptr;
            auto* world = m_Context ? dynamic_cast<World*>(m_Context->ecs) : nullptr;
            EditorSceneService(sceneMgr, world, &dirtyState, &selection, nullptr).DuplicateObject(selectedEntity);
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
                    auto* sceneMgr = m_Context ? dynamic_cast<SceneManager*>(m_Context->scenes) : nullptr;
                    auto* world = m_Context ? dynamic_cast<World*>(m_Context->ecs) : nullptr;
                    EditorSceneService(sceneMgr, world, &dirtyState, &selection, nullptr).DeleteObject(selectedEntity);
                    hasSelection = false;
                }
            }
            if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && ImGui::IsKeyPressed(ImGuiKey_D)) {
                if (hasSelection) {
                    auto* sceneMgr = m_Context ? dynamic_cast<SceneManager*>(m_Context->scenes) : nullptr;
                    auto* world = m_Context ? dynamic_cast<World*>(m_Context->ecs) : nullptr;
                    EditorSceneService(sceneMgr, world, &dirtyState, &selection, nullptr).DuplicateObject(selectedEntity);
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
                        if (obj->m_HasDirectionalLight || obj->m_HasPointLight || obj->m_HasSkyLight || obj->m_HasSpotLight) prefix += "[L]";
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

        ImGui::Separator();
        uint32_t ecsCount = m_Context->ecs ? m_Context->ecs->getCoordinator().GetLivingEntityCount() : 0;
        size_t sceneObjectCount = activeScene ? activeScene->GetAllSceneObjects().size() : 0;
        std::string selectedName = "None";
        if (hasSelection && coordinator.GetSignature(selectedEntity).test(coordinator.GetComponentType<NameComponent>())) {
            selectedName = coordinator.GetComponent<NameComponent>(selectedEntity).name;
        } else if (hasSelection) {
            selectedName = "Entity " + std::to_string(selectedEntity);
        }

        ImGui::Text("Scene: %s", activeScene ? activeScene->GetName().c_str() : "Untitled");
        ImGui::Text("Dirty: %s", dirtyState.IsSceneDirty() ? "Yes" : "No");
        ImGui::Text("ECS Entities: %u", ecsCount);
        ImGui::Text("Scene Objects: %zu", sceneObjectCount);
        ImGui::Text("Selected: %s", selectedName.c_str());
        if (ecsCount != sceneObjectCount) {
            ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.2f, 1.0f), "WARNING: ECS / Scene graph mismatch detected.");
        }

        // Context menu on empty space
        if (ImGui::BeginPopupContextWindow(nullptr, ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
            if (ImGui::MenuItem("Create Empty Entity")) {
                auto* sceneMgr = m_Context ? dynamic_cast<SceneManager*>(m_Context->scenes) : nullptr;
                auto* world = m_Context ? dynamic_cast<World*>(m_Context->ecs) : nullptr;
                EditorSceneService(sceneMgr, world, &dirtyState, &selection, nullptr).CreateEmptyObject();
            }
            if (ImGui::MenuItem("Create Player Start")) {
                auto* sceneMgr = m_Context ? dynamic_cast<SceneManager*>(m_Context->scenes) : nullptr;
                auto* world = m_Context ? dynamic_cast<World*>(m_Context->ecs) : nullptr;
                EditorSceneService(sceneMgr, world, &dirtyState, &selection, nullptr).CreatePlayerStart();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Create Directional Light")) {
                auto* sceneMgr = m_Context ? dynamic_cast<SceneManager*>(m_Context->scenes) : nullptr;
                auto* world = m_Context ? dynamic_cast<World*>(m_Context->ecs) : nullptr;
                EditorSceneService(sceneMgr, world, &dirtyState, &selection, nullptr).CreateDirectionalLight();
            }
            if (ImGui::MenuItem("Create Point Light")) {
                auto* sceneMgr = m_Context ? dynamic_cast<SceneManager*>(m_Context->scenes) : nullptr;
                auto* world = m_Context ? dynamic_cast<World*>(m_Context->ecs) : nullptr;
                EditorSceneService(sceneMgr, world, &dirtyState, &selection, nullptr).CreatePointLight();
            }
            if (ImGui::MenuItem("Create Sky Light")) {
                auto* sceneMgr = m_Context ? dynamic_cast<SceneManager*>(m_Context->scenes) : nullptr;
                auto* world = m_Context ? dynamic_cast<World*>(m_Context->ecs) : nullptr;
                EditorSceneService(sceneMgr, world, &dirtyState, &selection, nullptr).CreateSkyLight();
            }
            if (ImGui::MenuItem("Create Spot Light")) {
                auto* sceneMgr = m_Context ? dynamic_cast<SceneManager*>(m_Context->scenes) : nullptr;
                auto* world = m_Context ? dynamic_cast<World*>(m_Context->ecs) : nullptr;
                EditorSceneService(sceneMgr, world, &dirtyState, &selection, nullptr).CreateSpotLight();
            }
            ImGui::EndPopup();
        }

        ImGui::End();
    }

} // namespace eng::runtime
