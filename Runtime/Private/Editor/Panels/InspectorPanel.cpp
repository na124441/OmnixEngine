#include "Runtime/Private/Editor/Panels/InspectorPanel.h"
#include "Runtime/Private/Editor/Widgets/TransformWidget.h"
#include "Runtime/Private/Editor/Widgets/ComponentWidgets.h"
#include "Physics/Public/PhysicsWorld.h"
#include "ECS/Coordinator.h"
#include "ECS/ECSComponents.h"
#include "ECS/Public/IECSWorld.h"
#include "ThirdParty/imgui/imgui.h"

namespace eng::runtime {

    void InspectorPanel::Initialize(RuntimeContext* context) {
        m_Context = context;
    }

    void InspectorPanel::Render(EditorSelection& selection, EditorDirtyState& dirtyState) {
        ImGui::Begin("Inspector");

        if (!m_Context || !m_Context->ecs) {
            ImGui::Text("No active ECS Coordinator");
            ImGui::End();
            return;
        }

        auto& coordinator = m_Context->ecs->getCoordinator();
        Entity selectedEntity = selection.GetSelectedEntity();

        if (selectedEntity != 0 && coordinator.IsEntityAlive(selectedEntity)) {
            Signature sig = coordinator.GetSignature(selectedEntity);

            bool isPlaying = (m_Context->editorSimulationState == EditorSimulationState::Play);
            if (isPlaying) {
                ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "PLAY MODE - Runtime copy active. Editing locked.");
                ImGui::BeginDisabled();
            }

            ImGui::Text("Selected Entity ID: %u", selectedEntity);
            
            // Display debug signature info in debug section
            int componentCount = 0;
            for (int i = 0; i < 32; ++i) {
                if (sig.test(i)) componentCount++;
            }
            ImGui::TextDisabled("Signature: %s (%d components)", sig.to_string().c_str(), componentCount);
            
            ImGui::Separator();
            ImGui::Spacing();

            // 1. Name Component
            if (sig.test(coordinator.GetComponentType<NameComponent>())) {
                if (ImGui::CollapsingHeader("Name Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& nameComp = coordinator.GetComponent<NameComponent>(selectedEntity);
                    ComponentWidgets::DrawName(nameComp, dirtyState);
                    ImGui::Spacing();
                    if (ImGui::Button("Remove Name Component")) {
                        coordinator.RemoveComponent<NameComponent>(selectedEntity);
                        dirtyState.MarkSceneDirty();
                    }
                }
                ImGui::Separator();
            } else {
                ImGui::TextDisabled("No Name Component.");
                if (ImGui::Button("Add Name Component")) {
                    coordinator.AddComponent<NameComponent>(selectedEntity, NameComponent("Entity"));
                    dirtyState.MarkSceneDirty();
                }
                ImGui::Separator();
            }

            // 2. Transform Component (removable flag = false)
            if (sig.test(coordinator.GetComponentType<TransformComponent>())) {
                if (ImGui::CollapsingHeader("Transform Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& transComp = coordinator.GetComponent<TransformComponent>(selectedEntity);
                    if (TransformWidget::Draw(transComp, dirtyState)) {
                        if (sig.test(coordinator.GetComponentType<StaticBodyComponent>()) && m_Context->physicsWorld) {
                            m_Context->physicsWorld->RebuildStaticActor(coordinator, selectedEntity);
                        }
                    }
                }
                ImGui::Separator();
            } else {
                ImGui::TextDisabled("No Transform Component.");
                if (ImGui::Button("Add Transform Component")) {
                    coordinator.AddComponent<TransformComponent>(selectedEntity, TransformComponent());
                    dirtyState.MarkSceneDirty();
                }
                ImGui::Separator();
            }

            // 3. MeshRenderer Component
            if (sig.test(coordinator.GetComponentType<MeshRendererComponent>())) {
                if (ImGui::CollapsingHeader("Mesh Renderer Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& meshRenderer = coordinator.GetComponent<MeshRendererComponent>(selectedEntity);
                    ComponentWidgets::DrawMeshRenderer(meshRenderer, dirtyState);
                    ImGui::Spacing();
                    if (ImGui::Button("Remove Mesh Renderer Component")) {
                        coordinator.RemoveComponent<MeshRendererComponent>(selectedEntity);
                        dirtyState.MarkSceneDirty();
                    }
                }
                ImGui::Separator();
            }

            // 4. RigidBody Component
            if (sig.test(coordinator.GetComponentType<RigidBodyComponent>())) {
                if (ImGui::CollapsingHeader("Rigid Body Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& rb = coordinator.GetComponent<RigidBodyComponent>(selectedEntity);
                    ComponentWidgets::DrawRigidBody(rb, dirtyState);
                    ImGui::Spacing();
                    if (ImGui::Button("Remove Rigid Body Component")) {
                        coordinator.RemoveComponent<RigidBodyComponent>(selectedEntity);
                        dirtyState.MarkSceneDirty();
                    }
                }
                ImGui::Separator();
            }

            // 5. Tag Component
            if (sig.test(coordinator.GetComponentType<TagComponent>())) {
                if (ImGui::CollapsingHeader("Tag Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& tagComp = coordinator.GetComponent<TagComponent>(selectedEntity);
                    ComponentWidgets::DrawTag(tagComp, dirtyState);
                    ImGui::Spacing();
                    if (ImGui::Button("Remove Tag Component")) {
                        coordinator.RemoveComponent<TagComponent>(selectedEntity);
                        dirtyState.MarkSceneDirty();
                    }
                }
                ImGui::Separator();
            }

            // 6. Layer Component
            if (sig.test(coordinator.GetComponentType<LayerComponent>())) {
                if (ImGui::CollapsingHeader("Layer Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& layerComp = coordinator.GetComponent<LayerComponent>(selectedEntity);
                    ComponentWidgets::DrawLayer(layerComp, dirtyState);
                    ImGui::Spacing();
                    if (ImGui::Button("Remove Layer Component")) {
                        coordinator.RemoveComponent<LayerComponent>(selectedEntity);
                        dirtyState.MarkSceneDirty();
                    }
                }
                ImGui::Separator();
            }

            // 7. Health Component
            if (sig.test(coordinator.GetComponentType<HealthComponent>())) {
                if (ImGui::CollapsingHeader("Health Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& healthComp = coordinator.GetComponent<HealthComponent>(selectedEntity);
                    ComponentWidgets::DrawHealth(healthComp, dirtyState);
                    ImGui::Spacing();
                    if (ImGui::Button("Remove Health Component")) {
                        coordinator.RemoveComponent<HealthComponent>(selectedEntity);
                        dirtyState.MarkSceneDirty();
                    }
                }
                ImGui::Separator();
            }

            // 8. Collider Component
            if (sig.test(coordinator.GetComponentType<ColliderComponent>())) {
                if (ImGui::CollapsingHeader("Collider Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& colComp = coordinator.GetComponent<ColliderComponent>(selectedEntity);
                    ComponentWidgets::DrawCollider(colComp, dirtyState);
                    ImGui::Spacing();
                    if (ImGui::Button("Remove Collider Component")) {
                        coordinator.RemoveComponent<ColliderComponent>(selectedEntity);
                        dirtyState.MarkSceneDirty();
                    }
                }
                ImGui::Separator();
            }

            // 9. PlayerController Component
            if (sig.test(coordinator.GetComponentType<PlayerControllerComponent>())) {
                if (ImGui::CollapsingHeader("Player Controller Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& pcComp = coordinator.GetComponent<PlayerControllerComponent>(selectedEntity);
                    ComponentWidgets::DrawPlayerController(pcComp, dirtyState);
                    ImGui::Spacing();
                    if (ImGui::Button("Remove Player Controller Component")) {
                        coordinator.RemoveComponent<PlayerControllerComponent>(selectedEntity);
                        dirtyState.MarkSceneDirty();
                    }
                }
                ImGui::Separator();
            }

            // 10. RenderableMesh Component
            if (sig.test(coordinator.GetComponentType<RenderableMeshComponent>())) {
                if (ImGui::CollapsingHeader("Renderable Mesh Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& comp = coordinator.GetComponent<RenderableMeshComponent>(selectedEntity);
                    ComponentWidgets::DrawRenderableMesh(comp, *m_Context->assetRegistry, dirtyState);
                    ImGui::Spacing();
                    if (ImGui::Button("Remove Renderable Mesh Component")) {
                        coordinator.RemoveComponent<RenderableMeshComponent>(selectedEntity);
                        dirtyState.MarkSceneDirty();
                    }
                }
                ImGui::Separator();
            }

            // 11. Material Component
            if (sig.test(coordinator.GetComponentType<MaterialComponent>())) {
                if (ImGui::CollapsingHeader("Material Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& comp = coordinator.GetComponent<MaterialComponent>(selectedEntity);
                    ComponentWidgets::DrawMaterial(comp, *m_Context->assetRegistry, dirtyState);
                    ImGui::Spacing();
                    if (ImGui::Button("Remove Material Component")) {
                        coordinator.RemoveComponent<MaterialComponent>(selectedEntity);
                        dirtyState.MarkSceneDirty();
                    }
                }
                ImGui::Separator();
            }

            // StaticBody Component
            if (sig.test(coordinator.GetComponentType<StaticBodyComponent>())) {
                if (ImGui::CollapsingHeader("Static Body Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& comp = coordinator.GetComponent<StaticBodyComponent>(selectedEntity);
                    if (ComponentWidgets::DrawStaticBody(comp, dirtyState)) {
                        if (m_Context->physicsWorld) {
                            m_Context->physicsWorld->RebuildStaticActor(coordinator, selectedEntity);
                        }
                    }
                    ImGui::Spacing();
                    if (ImGui::Button("Remove Static Body Component")) {
                        coordinator.RemoveComponent<StaticBodyComponent>(selectedEntity);
                        dirtyState.MarkSceneDirty();
                        if (m_Context->physicsWorld) {
                            m_Context->physicsWorld->UnregisterEntity(selectedEntity);
                        }
                    }
                }
                ImGui::Separator();
            }

            // BoxCollider Component
            if (sig.test(coordinator.GetComponentType<BoxColliderComponent>())) {
                if (ImGui::CollapsingHeader("Box Collider Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& comp = coordinator.GetComponent<BoxColliderComponent>(selectedEntity);
                    if (ComponentWidgets::DrawBoxCollider(comp, dirtyState)) {
                        if (m_Context->physicsWorld) {
                            m_Context->physicsWorld->RebuildStaticActor(coordinator, selectedEntity);
                        }
                    }
                    ImGui::Spacing();
                    if (ImGui::Button("Remove Box Collider Component")) {
                        coordinator.RemoveComponent<BoxColliderComponent>(selectedEntity);
                        dirtyState.MarkSceneDirty();
                        if (m_Context->physicsWorld) {
                            m_Context->physicsWorld->RebuildStaticActor(coordinator, selectedEntity);
                        }
                    }
                }
                ImGui::Separator();
            }

            // SphereCollider Component
            if (sig.test(coordinator.GetComponentType<SphereColliderComponent>())) {
                if (ImGui::CollapsingHeader("Sphere Collider Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& comp = coordinator.GetComponent<SphereColliderComponent>(selectedEntity);
                    if (ComponentWidgets::DrawSphereCollider(comp, dirtyState)) {
                        if (m_Context->physicsWorld) {
                            m_Context->physicsWorld->RebuildStaticActor(coordinator, selectedEntity);
                        }
                    }
                    ImGui::Spacing();
                    if (ImGui::Button("Remove Sphere Collider Component")) {
                        coordinator.RemoveComponent<SphereColliderComponent>(selectedEntity);
                        dirtyState.MarkSceneDirty();
                        if (m_Context->physicsWorld) {
                            m_Context->physicsWorld->RebuildStaticActor(coordinator, selectedEntity);
                        }
                    }
                }
                ImGui::Separator();
            }

            // CapsuleCollider Component
            if (sig.test(coordinator.GetComponentType<CapsuleColliderComponent>())) {
                if (ImGui::CollapsingHeader("Capsule Collider Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& comp = coordinator.GetComponent<CapsuleColliderComponent>(selectedEntity);
                    if (ComponentWidgets::DrawCapsuleCollider(comp, dirtyState)) {
                        if (m_Context->physicsWorld) {
                            m_Context->physicsWorld->RebuildStaticActor(coordinator, selectedEntity);
                        }
                    }
                    ImGui::Spacing();
                    if (ImGui::Button("Remove Capsule Collider Component")) {
                        coordinator.RemoveComponent<CapsuleColliderComponent>(selectedEntity);
                        dirtyState.MarkSceneDirty();
                        if (m_Context->physicsWorld) {
                            m_Context->physicsWorld->RebuildStaticActor(coordinator, selectedEntity);
                        }
                    }
                }
                ImGui::Separator();
            }

            // --- Add Component Selector ---
            ImGui::Spacing();
            if (ImGui::Button("Add Component...")) {
                ImGui::OpenPopup("AddComponentPopup");
            }

            if (ImGui::BeginPopup("AddComponentPopup")) {
                if (!sig.test(coordinator.GetComponentType<NameComponent>())) {
                    if (ImGui::MenuItem("Name Component")) {
                        coordinator.AddComponent<NameComponent>(selectedEntity, NameComponent("Entity"));
                        dirtyState.MarkSceneDirty();
                    }
                }
                if (!sig.test(coordinator.GetComponentType<TransformComponent>())) {
                    if (ImGui::MenuItem("Transform Component")) {
                        coordinator.AddComponent<TransformComponent>(selectedEntity, TransformComponent());
                        dirtyState.MarkSceneDirty();
                    }
                }
                if (!sig.test(coordinator.GetComponentType<MeshRendererComponent>())) {
                    if (ImGui::MenuItem("MeshRenderer Component")) {
                        coordinator.AddComponent<MeshRendererComponent>(selectedEntity, MeshRendererComponent());
                        dirtyState.MarkSceneDirty();
                    }
                }
                if (!sig.test(coordinator.GetComponentType<RigidBodyComponent>())) {
                    if (ImGui::MenuItem("RigidBody Component")) {
                        coordinator.AddComponent<RigidBodyComponent>(selectedEntity, RigidBodyComponent());
                        dirtyState.MarkSceneDirty();
                    }
                }
                if (!sig.test(coordinator.GetComponentType<TagComponent>())) {
                    if (ImGui::MenuItem("Tag Component")) {
                        coordinator.AddComponent<TagComponent>(selectedEntity, TagComponent("Tag"));
                        dirtyState.MarkSceneDirty();
                    }
                }
                if (!sig.test(coordinator.GetComponentType<LayerComponent>())) {
                    if (ImGui::MenuItem("Layer Component")) {
                        coordinator.AddComponent<LayerComponent>(selectedEntity, LayerComponent(0, "Default"));
                        dirtyState.MarkSceneDirty();
                    }
                }
                if (!sig.test(coordinator.GetComponentType<HealthComponent>())) {
                    if (ImGui::MenuItem("Health Component")) {
                        coordinator.AddComponent<HealthComponent>(selectedEntity, HealthComponent(100.0f));
                        dirtyState.MarkSceneDirty();
                    }
                }
                if (!sig.test(coordinator.GetComponentType<ColliderComponent>())) {
                    if (ImGui::MenuItem("Collider Component")) {
                        coordinator.AddComponent<ColliderComponent>(selectedEntity, ColliderComponent());
                        dirtyState.MarkSceneDirty();
                    }
                }
                if (!sig.test(coordinator.GetComponentType<PlayerControllerComponent>())) {
                    if (ImGui::MenuItem("PlayerController Component")) {
                        coordinator.AddComponent<PlayerControllerComponent>(selectedEntity, PlayerControllerComponent());
                        dirtyState.MarkSceneDirty();
                    }
                }
                if (!sig.test(coordinator.GetComponentType<RenderableMeshComponent>())) {
                    if (ImGui::MenuItem("RenderableMesh Component")) {
                        coordinator.AddComponent<RenderableMeshComponent>(selectedEntity, RenderableMeshComponent());
                        dirtyState.MarkSceneDirty();
                    }
                }
                if (!sig.test(coordinator.GetComponentType<MaterialComponent>())) {
                    if (ImGui::MenuItem("Material Component")) {
                        coordinator.AddComponent<MaterialComponent>(selectedEntity, MaterialComponent());
                        dirtyState.MarkSceneDirty();
                    }
                }
                if (!sig.test(coordinator.GetComponentType<StaticBodyComponent>())) {
                    if (ImGui::MenuItem("StaticBody Component")) {
                        coordinator.AddComponent<StaticBodyComponent>(selectedEntity, StaticBodyComponent());
                        dirtyState.MarkSceneDirty();
                        if (m_Context->physicsWorld) {
                            m_Context->physicsWorld->RebuildStaticActor(coordinator, selectedEntity);
                        }
                    }
                }
                if (!sig.test(coordinator.GetComponentType<BoxColliderComponent>())) {
                    if (ImGui::MenuItem("BoxCollider Component")) {
                        coordinator.AddComponent<BoxColliderComponent>(selectedEntity, BoxColliderComponent());
                        dirtyState.MarkSceneDirty();
                        if (m_Context->physicsWorld) {
                            m_Context->physicsWorld->RebuildStaticActor(coordinator, selectedEntity);
                        }
                    }
                }
                if (!sig.test(coordinator.GetComponentType<SphereColliderComponent>())) {
                    if (ImGui::MenuItem("SphereCollider Component")) {
                        coordinator.AddComponent<SphereColliderComponent>(selectedEntity, SphereColliderComponent());
                        dirtyState.MarkSceneDirty();
                        if (m_Context->physicsWorld) {
                            m_Context->physicsWorld->RebuildStaticActor(coordinator, selectedEntity);
                        }
                    }
                }
                if (!sig.test(coordinator.GetComponentType<CapsuleColliderComponent>())) {
                    if (ImGui::MenuItem("CapsuleCollider Component")) {
                        coordinator.AddComponent<CapsuleColliderComponent>(selectedEntity, CapsuleColliderComponent());
                        dirtyState.MarkSceneDirty();
                        if (m_Context->physicsWorld) {
                            m_Context->physicsWorld->RebuildStaticActor(coordinator, selectedEntity);
                        }
                    }
                }
                ImGui::EndPopup();
            }

            if (isPlaying) {
                ImGui::EndDisabled();
            }

        } else {
            ImGui::TextDisabled("Select an entity to view details.");
        }

        ImGui::End();
    }

} // namespace eng::runtime
