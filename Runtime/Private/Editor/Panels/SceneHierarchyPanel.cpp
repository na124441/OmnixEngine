#include "Runtime/Private/Editor/Panels/SceneHierarchyPanel.h"
#include "Runtime/Public/Editor/EditorEntityCommands.h"
#include "Physics/Public/PhysicsWorld.h"
#include "ECS/Coordinator.h"
#include "ECS/ECSComponents.h"
#include "ECS/Public/IECSWorld.h"
#include "ThirdParty/imgui/imgui.h"

namespace eng::runtime {

    void SceneHierarchyPanel::Initialize(RuntimeContext* context) {
        m_Context = context;
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

        // Entity List
        const auto& activeEntities = coordinator.GetActiveEntities();
        for (Entity entity : activeEntities) {
            std::string entityName = "Entity " + std::to_string(entity);
            Signature sig = coordinator.GetSignature(entity);
            if (sig.test(coordinator.GetComponentType<NameComponent>())) {
                entityName = coordinator.GetComponent<NameComponent>(entity).name;
            }

            ImGuiTreeNodeFlags flags = ((selectedEntity == entity) ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_Leaf;
            
            bool opened = ImGui::TreeNodeEx((void*)(uintptr_t)entity, flags, "%s", entityName.c_str());
            
            if (ImGui::IsItemClicked()) {
                selection.Select(entity);
            }

            if (opened) {
                ImGui::TreePop();
            }
        }

        if (ImGui::BeginPopupContextWindow()) {
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
