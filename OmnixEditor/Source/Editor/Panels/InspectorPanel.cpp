#include "Editor/Panels/InspectorPanel.h"
#include "Editor/Widgets/TransformWidget.h"
#include "Editor/Widgets/ComponentWidgets.h"
#include "Physics/Public/PhysicsWorld.h"
#include "ECS/Coordinator.h"
#include "ECS/ECSComponents.h"
#include "Runtime/World/WorldManager.h"
#include "Runtime/World/ZoneMembershipComponent.h"
#include "Runtime/World/GroundSectionComponent.h"
#include "ECS/Public/IECSWorld.h"
#include "ThirdParty/imgui/imgui.h"
#include "Runtime/AssetRegistry.h"
#include "Rendering/Core/Renderer.h"
#include "RenderingEngine/Runtime/engine/EngineLoop.h"
#include "RenderingEngine/Renderer/scene/Mesh.h"

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
                    bool transformCommitted = false;
                    if (TransformWidget::Draw(transComp, dirtyState, transformCommitted)) {
                        // Changed
                    }
                    if (transformCommitted) {
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
                    ComponentWidgets::DrawMeshRenderer(meshRenderer, dirtyState, m_Context);
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
                    if (comp.meshAssetHandle.IsValid()) {
                        if (ImGui::Button("Fit Box Collider To Mesh")) {
                            auto* engineLoop = m_Context ? dynamic_cast<eng::runtime::EngineLoop*>(m_Context->renderer) : nullptr;
                            if (engineLoop && engineLoop->GetSceneRenderer()) {
                                auto* sceneRenderer = engineLoop->GetSceneRenderer();
                                
                                eng::renderer::Mesh* meshPtr = nullptr;
                                auto it = sceneRenderer->m_EcsMeshCache.find(comp.meshAssetHandle.value);
                                if (it != sceneRenderer->m_EcsMeshCache.end() && it->second) {
                                    meshPtr = it->second;
                                } else {
                                    if (m_Context->assetRegistry) {
                                        const auto* meta = m_Context->assetRegistry->GetMetadata(comp.meshAssetHandle);
                                        if (meta && meta->type == AssetType::Mesh) {
                                            meshPtr = sceneRenderer->getScene().createMeshFromOBJ(meta->sourcePath, sceneRenderer->resources);
                                            if (meshPtr) {
                                                sceneRenderer->m_EcsMeshCache[comp.meshAssetHandle.value] = meshPtr;
                                            }
                                        }
                                    }
                                }
                                
                                if (meshPtr) {
                                    glm::vec3 minB = meshPtr->minBounds;
                                    glm::vec3 maxB = meshPtr->maxBounds;
                                    
                                    glm::vec3 size = maxB - minB;
                                    glm::vec3 center = (minB + maxB) * 0.5f;
                                    
                                    if (!sig.test(coordinator.GetComponentType<BoxColliderComponent>())) {
                                        coordinator.AddComponent<BoxColliderComponent>(selectedEntity, BoxColliderComponent());
                                        sig = coordinator.GetSignature(selectedEntity);
                                    }
                                    auto& bcc = coordinator.GetComponent<BoxColliderComponent>(selectedEntity);
                                    bcc.size = Vector3(size.x, size.y, size.z);
                                    bcc.offset = Vector3(center.x, center.y, center.z);
                                    bcc.debugDraw = true;
                                    
                                    if (!sig.test(coordinator.GetComponentType<StaticBodyComponent>())) {
                                        coordinator.AddComponent<StaticBodyComponent>(selectedEntity, StaticBodyComponent());
                                        sig = coordinator.GetSignature(selectedEntity);
                                    }
                                    
                                    dirtyState.MarkSceneDirty();
                                    if (m_Context->physicsWorld) {
                                        m_Context->physicsWorld->RebuildStaticActor(coordinator, selectedEntity);
                                    }
                                }
                            }
                        }
                        
                        ImGui::SameLine();
                        if (ImGui::Button("Add Static Body")) {
                            if (!sig.test(coordinator.GetComponentType<StaticBodyComponent>())) {
                                coordinator.AddComponent<StaticBodyComponent>(selectedEntity, StaticBodyComponent());
                                sig = coordinator.GetSignature(selectedEntity);
                                dirtyState.MarkSceneDirty();
                                if (m_Context->physicsWorld) {
                                    m_Context->physicsWorld->RebuildStaticActor(coordinator, selectedEntity);
                                }
                            }
                        }
                        
                        ImGui::SameLine();
                        if (sig.test(coordinator.GetComponentType<BoxColliderComponent>())) {
                            auto& bcc = coordinator.GetComponent<BoxColliderComponent>(selectedEntity);
                            bool showBounds = bcc.debugDraw;
                            if (ImGui::Checkbox("Show Collider Bounds", &showBounds)) {
                                bcc.debugDraw = showBounds;
                                dirtyState.MarkSceneDirty();
                            }
                        } else {
                            if (ImGui::Button("Show Collider Bounds")) {
                                coordinator.AddComponent<BoxColliderComponent>(selectedEntity, BoxColliderComponent());
                                sig = coordinator.GetSignature(selectedEntity);
                                auto& bcc = coordinator.GetComponent<BoxColliderComponent>(selectedEntity);
                                bcc.debugDraw = true;
                                dirtyState.MarkSceneDirty();
                            }
                        }
                        ImGui::Spacing();
                    }

                    if (ImGui::Button("Remove Renderable Mesh Component")) {
                        coordinator.RemoveComponent<RenderableMeshComponent>(selectedEntity);
                        sig = coordinator.GetSignature(selectedEntity);
                        dirtyState.MarkSceneDirty();
                    }
                }
                ImGui::Separator();
            }

            // 11. Material Component
            if (sig.test(coordinator.GetComponentType<MaterialComponent>())) {
                if (ImGui::CollapsingHeader("Material Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& comp = coordinator.GetComponent<MaterialComponent>(selectedEntity);
                    ComponentWidgets::DrawMaterial(comp, *m_Context->assetRegistry, dirtyState, m_Context);
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
                    bool colliderCommitted = false;
                    if (ComponentWidgets::DrawBoxCollider(comp, dirtyState, colliderCommitted)) {
                        // Changed
                    }
                    if (colliderCommitted) {
                        if (m_Context->physicsWorld) {
                            m_Context->physicsWorld->RebuildStaticActor(coordinator, selectedEntity);
                        }
                    }
                    ImGui::Spacing();
                    
                    if (sig.test(coordinator.GetComponentType<RenderableMeshComponent>())) {
                        auto& meshComp = coordinator.GetComponent<RenderableMeshComponent>(selectedEntity);
                        if (meshComp.meshAssetHandle.IsValid()) {
                            if (ImGui::Button("Fit Box Collider to Mesh Bounds")) {
                                auto* engineLoop = m_Context ? dynamic_cast<eng::runtime::EngineLoop*>(m_Context->renderer) : nullptr;
                                if (engineLoop && engineLoop->GetSceneRenderer()) {
                                    auto* sceneRenderer = engineLoop->GetSceneRenderer();
                                    
                                                                    eng::renderer::Mesh* meshPtr = nullptr;
                                    auto it = sceneRenderer->m_EcsMeshCache.find(meshComp.meshAssetHandle.value);
                                    if (it != sceneRenderer->m_EcsMeshCache.end() && it->second) {
                                        meshPtr = it->second;
                                    } else {
                                        if (m_Context->assetRegistry) {
                                            const auto* meta = m_Context->assetRegistry->GetMetadata(meshComp.meshAssetHandle);
                                            if (meta && meta->type == AssetType::Mesh) {
                                                meshPtr = sceneRenderer->getScene().createMeshFromOBJ(meta->sourcePath, sceneRenderer->resources);
                                                if (meshPtr) {
                                                    sceneRenderer->m_EcsMeshCache[meshComp.meshAssetHandle.value] = meshPtr;
                                                }
                                            }
                                        }
                                    }
                                    
                                    if (meshPtr) {
                                        glm::vec3 minB = meshPtr->minBounds;
                                        glm::vec3 maxB = meshPtr->maxBounds;
                                        
                                        glm::vec3 size = maxB - minB;
                                        glm::vec3 center = (minB + maxB) * 0.5f;
                                        
                                        comp.size = Vector3(size.x, size.y, size.z);
                                        comp.offset = Vector3(center.x, center.y, center.z);
                                        
                                        if (!sig.test(coordinator.GetComponentType<StaticBodyComponent>())) {
                                            coordinator.AddComponent<StaticBodyComponent>(selectedEntity, StaticBodyComponent());
                                        }
                                        
                                        dirtyState.MarkSceneDirty();
                                        if (m_Context->physicsWorld) {
                                            m_Context->physicsWorld->RebuildStaticActor(coordinator, selectedEntity);
                                        }
                                    }
                                }
                            }
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
                    bool colliderCommitted = false;
                    if (ComponentWidgets::DrawSphereCollider(comp, dirtyState, colliderCommitted)) {
                        // Changed
                    }
                    if (colliderCommitted) {
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
                    bool colliderCommitted = false;
                    if (ComponentWidgets::DrawCapsuleCollider(comp, dirtyState, colliderCommitted)) {
                        // Changed
                    }
                    if (colliderCommitted) {
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

            // PlayerStart Component
            if (sig.test(coordinator.GetComponentType<PlayerStartComponent>())) {
                if (ImGui::CollapsingHeader("Player Start Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& comp = coordinator.GetComponent<PlayerStartComponent>(selectedEntity);
                    ComponentWidgets::DrawPlayerStart(comp, dirtyState);
                    ImGui::Spacing();
                    if (ImGui::Button("Remove Player Start Component")) {
                        coordinator.RemoveComponent<PlayerStartComponent>(selectedEntity);
                        dirtyState.MarkSceneDirty();
                    }
                }
                ImGui::Separator();
            }

            // CharacterController Component
            if (sig.test(coordinator.GetComponentType<CharacterControllerComponent>())) {
                if (ImGui::CollapsingHeader("Character Controller Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& comp = coordinator.GetComponent<CharacterControllerComponent>(selectedEntity);
                    ComponentWidgets::DrawCharacterController(comp, dirtyState);
                    ImGui::Spacing();
                    if (ImGui::Button("Remove Character Controller Component")) {
                        coordinator.RemoveComponent<CharacterControllerComponent>(selectedEntity);
                        dirtyState.MarkSceneDirty();
                    }
                }
                ImGui::Separator();
            }

            // Camera Component
            if (sig.test(coordinator.GetComponentType<CameraComponent>())) {
                if (ImGui::CollapsingHeader("Camera Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& comp = coordinator.GetComponent<CameraComponent>(selectedEntity);
                    ComponentWidgets::DrawCamera(comp, dirtyState);
                    ImGui::Spacing();
                    if (ImGui::Button("Remove Camera Component")) {
                        coordinator.RemoveComponent<CameraComponent>(selectedEntity);
                        dirtyState.MarkSceneDirty();
                    }
                }
                ImGui::Separator();
            }

            // Input Component
            if (sig.test(coordinator.GetComponentType<InputComponent>())) {
                if (ImGui::CollapsingHeader("Input Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& comp = coordinator.GetComponent<InputComponent>(selectedEntity);
                    ComponentWidgets::DrawInput(comp, dirtyState);
                    ImGui::Spacing();
                    if (ImGui::Button("Remove Input Component")) {
                        coordinator.RemoveComponent<InputComponent>(selectedEntity);
                        dirtyState.MarkSceneDirty();
                    }
                }
                ImGui::Separator();
            }

            // Trigger Component
            if (sig.test(coordinator.GetComponentType<TriggerComponent>())) {
                if (ImGui::CollapsingHeader("Trigger Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& comp = coordinator.GetComponent<TriggerComponent>(selectedEntity);
                    ComponentWidgets::DrawTrigger(comp, dirtyState);
                    ImGui::Spacing();
                    if (ImGui::Button("Remove Trigger Component")) {
                        coordinator.RemoveComponent<TriggerComponent>(selectedEntity);
                        dirtyState.MarkSceneDirty();
                    }
                }
                ImGui::Separator();
            }

            // Interactable Component
            if (sig.test(coordinator.GetComponentType<InteractableComponent>())) {
                if (ImGui::CollapsingHeader("Interactable Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& comp = coordinator.GetComponent<InteractableComponent>(selectedEntity);
                    ComponentWidgets::DrawInteractable(comp, dirtyState);
                    ImGui::Spacing();
                    if (ImGui::Button("Remove Interactable Component")) {
                        coordinator.RemoveComponent<InteractableComponent>(selectedEntity);
                        dirtyState.MarkSceneDirty();
                    }
                }
                ImGui::Separator();
            }

            // Objective Component
            if (sig.test(coordinator.GetComponentType<ObjectiveComponent>())) {
                if (ImGui::CollapsingHeader("Objective Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& comp = coordinator.GetComponent<ObjectiveComponent>(selectedEntity);
                    ComponentWidgets::DrawObjective(comp, dirtyState);
                    ImGui::Spacing();
                    if (ImGui::Button("Remove Objective Component")) {
                        coordinator.RemoveComponent<ObjectiveComponent>(selectedEntity);
                        dirtyState.MarkSceneDirty();
                    }
                }
                ImGui::Separator();
            }

            // AudioSource Component
            if (sig.test(coordinator.GetComponentType<AudioSourceComponent>())) {
                if (ImGui::CollapsingHeader("AudioSource Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& comp = coordinator.GetComponent<AudioSourceComponent>(selectedEntity);
                    ComponentWidgets::DrawAudioSource(comp, dirtyState, m_Context ? m_Context->audioSystem : nullptr);
                    ImGui::Spacing();
                    if (ImGui::Button("Remove AudioSource Component")) {
                        coordinator.RemoveComponent<AudioSourceComponent>(selectedEntity);
                        dirtyState.MarkSceneDirty();
                    }
                }
                ImGui::Separator();
            }

            // SimpleState Component
            if (sig.test(coordinator.GetComponentType<SimpleStateComponent>())) {
                if (ImGui::CollapsingHeader("SimpleState Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& comp = coordinator.GetComponent<SimpleStateComponent>(selectedEntity);
                    ComponentWidgets::DrawSimpleState(comp, dirtyState);
                    ImGui::Spacing();
                    if (ImGui::Button("Remove SimpleState Component")) {
                        coordinator.RemoveComponent<SimpleStateComponent>(selectedEntity);
                        dirtyState.MarkSceneDirty();
                    }
                }
                ImGui::Separator();
            }

            // Activatable Component
            if (sig.test(coordinator.GetComponentType<ActivatableComponent>())) {
                if (ImGui::CollapsingHeader("Activatable Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& comp = coordinator.GetComponent<ActivatableComponent>(selectedEntity);
                    ComponentWidgets::DrawActivatable(comp, dirtyState);
                    ImGui::Spacing();
                    if (ImGui::Button("Remove Activatable Component")) {
                        coordinator.RemoveComponent<ActivatableComponent>(selectedEntity);
                        dirtyState.MarkSceneDirty();
                    }
                }
                ImGui::Separator();
            }

            // Door Component
            if (sig.test(coordinator.GetComponentType<DoorComponent>())) {
                if (ImGui::CollapsingHeader("Door Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& comp = coordinator.GetComponent<DoorComponent>(selectedEntity);
                    ComponentWidgets::DrawDoor(comp, dirtyState);
                    ImGui::Spacing();
                    if (ImGui::Button("Remove Door Component")) {
                        coordinator.RemoveComponent<DoorComponent>(selectedEntity);
                        dirtyState.MarkSceneDirty();
                    }
                }
                ImGui::Separator();
            }

            // Checkpoint Component
            if (sig.test(coordinator.GetComponentType<CheckpointComponent>())) {
                if (ImGui::CollapsingHeader("Checkpoint Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& comp = coordinator.GetComponent<CheckpointComponent>(selectedEntity);
                    ComponentWidgets::DrawCheckpoint(comp, dirtyState);
                    ImGui::Spacing();
                    if (ImGui::Button("Remove Checkpoint Component")) {
                        coordinator.RemoveComponent<CheckpointComponent>(selectedEntity);
                        dirtyState.MarkSceneDirty();
                    }
                }
                ImGui::Separator();
            }

            // Directional Light Component
            if (sig.test(coordinator.GetComponentType<DirectionalLightComponent>())) {
                if (ImGui::CollapsingHeader("Directional Light Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& comp = coordinator.GetComponent<DirectionalLightComponent>(selectedEntity);
                    ComponentWidgets::DrawDirectionalLight(comp, dirtyState);
                    ImGui::Spacing();
                    if (ImGui::Button("Remove Directional Light Component")) {
                        coordinator.RemoveComponent<DirectionalLightComponent>(selectedEntity);
                        dirtyState.MarkSceneDirty();
                    }
                }
                ImGui::Separator();
            }

            // Point Light Component
            if (sig.test(coordinator.GetComponentType<PointLightComponent>())) {
                if (ImGui::CollapsingHeader("Point Light Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& comp = coordinator.GetComponent<PointLightComponent>(selectedEntity);
                    ComponentWidgets::DrawPointLight(comp, dirtyState);
                    ImGui::Spacing();
                    if (ImGui::Button("Remove Point Light Component")) {
                        coordinator.RemoveComponent<PointLightComponent>(selectedEntity);
                        dirtyState.MarkSceneDirty();
                    }
                }
                ImGui::Separator();
            }

            // Sky Light Component
            if (sig.test(coordinator.GetComponentType<SkyLightComponent>())) {
                if (ImGui::CollapsingHeader("Sky Light Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& comp = coordinator.GetComponent<SkyLightComponent>(selectedEntity);
                    ComponentWidgets::DrawSkyLight(comp, dirtyState);
                    ImGui::Spacing();
                    if (ImGui::Button("Remove Sky Light Component")) {
                        coordinator.RemoveComponent<SkyLightComponent>(selectedEntity);
                        dirtyState.MarkSceneDirty();
                    }
                }
                ImGui::Separator();
            }

            // Spot Light Component
            if (sig.test(coordinator.GetComponentType<SpotLightComponent>())) {
                if (ImGui::CollapsingHeader("Spot Light Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& comp = coordinator.GetComponent<SpotLightComponent>(selectedEntity);
                    ComponentWidgets::DrawSpotLight(comp, dirtyState);
                    ImGui::Spacing();
                    if (ImGui::Button("Remove Spot Light Component")) {
                        coordinator.RemoveComponent<SpotLightComponent>(selectedEntity);
                        dirtyState.MarkSceneDirty();
                    }
                }
                ImGui::Separator();
            }

            // Bounds Component
            if (sig.test(coordinator.GetComponentType<BoundsComponent>())) {
                if (ImGui::CollapsingHeader("Bounds Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& comp = coordinator.GetComponent<BoundsComponent>(selectedEntity);
                    ComponentWidgets::DrawBounds(comp, dirtyState);
                    ImGui::Spacing();
                    if (ImGui::Button("Remove Bounds Component")) {
                        coordinator.RemoveComponent<BoundsComponent>(selectedEntity);
                        dirtyState.MarkSceneDirty();
                    }
                }
                ImGui::Separator();
            }

            // Zone Membership Component
            if (sig.test(coordinator.GetComponentType<eng::runtime::ZoneMembershipComponent>())) {
                if (ImGui::CollapsingHeader("Zone Membership Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& zmc = coordinator.GetComponent<eng::runtime::ZoneMembershipComponent>(selectedEntity);
                    ImGui::Text("Zone UUID High: %llu", zmc.zoneUUIDHigh);
                    ImGui::Text("Zone UUID Low: %llu", zmc.zoneUUIDLow);
                    
                    if (m_Context && m_Context->worldManager) {
                        const auto& loadedZones = m_Context->worldManager->GetLoadedZones();
                        std::string currentZoneName = "None";
                        for (const auto& zone : loadedZones) {
                            if (zone.zoneUUIDHigh == zmc.zoneUUIDHigh && zone.zoneUUIDLow == zmc.zoneUUIDLow) {
                                currentZoneName = zone.zoneName;
                                break;
                            }
                        }
                        
                        if (ImGui::BeginCombo("Assigned Zone", currentZoneName.c_str())) {
                            bool isNoneSelected = (zmc.zoneUUIDHigh == 0 && zmc.zoneUUIDLow == 0);
                            if (ImGui::Selectable("None", isNoneSelected)) {
                                zmc.zoneUUIDHigh = 0;
                                zmc.zoneUUIDLow = 0;
                                dirtyState.MarkSceneDirty();
                            }
                            for (const auto& zone : loadedZones) {
                                bool isSelected = (zone.zoneUUIDHigh == zmc.zoneUUIDHigh && zone.zoneUUIDLow == zmc.zoneUUIDLow);
                                if (ImGui::Selectable(zone.zoneName.c_str(), isSelected)) {
                                    zmc.zoneUUIDHigh = zone.zoneUUIDHigh;
                                    zmc.zoneUUIDLow = zone.zoneUUIDLow;
                                    dirtyState.MarkSceneDirty();
                                }
                            }
                            ImGui::EndCombo();
                        }
                    }
                    
                    ImGui::Spacing();
                    if (ImGui::Button("Remove Zone Membership Component")) {
                        coordinator.RemoveComponent<eng::runtime::ZoneMembershipComponent>(selectedEntity);
                        dirtyState.MarkSceneDirty();
                    }
                }
                ImGui::Separator();
            }

            // Ground Section Component
            if (sig.test(coordinator.GetComponentType<eng::runtime::GroundSectionComponent>())) {
                if (ImGui::CollapsingHeader("Ground Section Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& gsc = coordinator.GetComponent<eng::runtime::GroundSectionComponent>(selectedEntity);
                    
                    if (m_Context && m_Context->worldManager) {
                        const auto& loadedZones = m_Context->worldManager->GetLoadedZones();
                        std::string currentZoneName = "None";
                        for (const auto& zone : loadedZones) {
                            if (zone.zoneUUIDHigh == gsc.zoneUUIDHigh && zone.zoneUUIDLow == gsc.zoneUUIDLow) {
                                currentZoneName = zone.zoneName;
                                break;
                            }
                        }
                        
                        if (ImGui::BeginCombo("Assigned Zone", currentZoneName.c_str())) {
                            bool isNoneSelected = (gsc.zoneUUIDHigh == 0 && gsc.zoneUUIDLow == 0);
                            if (ImGui::Selectable("None", isNoneSelected)) {
                                gsc.zoneUUIDHigh = 0;
                                gsc.zoneUUIDLow = 0;
                                dirtyState.MarkSceneDirty();
                            }
                            for (const auto& zone : loadedZones) {
                                bool isSelected = (zone.zoneUUIDHigh == gsc.zoneUUIDHigh && zone.zoneUUIDLow == gsc.zoneUUIDLow);
                                if (ImGui::Selectable(zone.zoneName.c_str(), isSelected)) {
                                    gsc.zoneUUIDHigh = zone.zoneUUIDHigh;
                                    gsc.zoneUUIDLow = zone.zoneUUIDLow;
                                    dirtyState.MarkSceneDirty();
                                }
                            }
                            ImGui::EndCombo();
                        }
                    }

                    // Text inputs for paths
                    char meshPathBuf[256];
                    std::strncpy(meshPathBuf, gsc.meshAssetPath.c_str(), sizeof(meshPathBuf));
                    meshPathBuf[sizeof(meshPathBuf)-1] = '\0';
                    if (ImGui::InputText("Mesh Asset Path", meshPathBuf, sizeof(meshPathBuf))) {
                        gsc.meshAssetPath = meshPathBuf;
                        dirtyState.MarkSceneDirty();
                    }

                    char matPathBuf[256];
                    std::strncpy(matPathBuf, gsc.materialAssetPath.c_str(), sizeof(matPathBuf));
                    matPathBuf[sizeof(matPathBuf)-1] = '\0';
                    if (ImGui::InputText("Material Asset Path", matPathBuf, sizeof(matPathBuf))) {
                        gsc.materialAssetPath = matPathBuf;
                        dirtyState.MarkSceneDirty();
                    }

                    char colPathBuf[256];
                    std::strncpy(colPathBuf, gsc.collisionAssetPath.c_str(), sizeof(colPathBuf));
                    colPathBuf[sizeof(colPathBuf)-1] = '\0';
                    if (ImGui::InputText("Collision Asset Path", colPathBuf, sizeof(colPathBuf))) {
                        gsc.collisionAssetPath = colPathBuf;
                        dirtyState.MarkSceneDirty();
                    }

                    // Bounds inputs
                    float min[3] = { gsc.boundsMin.x, gsc.boundsMin.y, gsc.boundsMin.z };
                    if (ImGui::DragFloat3("Bounds Min", min, 0.1f)) {
                        gsc.boundsMin = { min[0], min[1], min[2] };
                        dirtyState.MarkSceneDirty();
                    }

                    float max[3] = { gsc.boundsMax.x, gsc.boundsMax.y, gsc.boundsMax.z };
                    if (ImGui::DragFloat3("Bounds Max", max, 0.1f)) {
                        gsc.boundsMax = { max[0], max[1], max[2] };
                        dirtyState.MarkSceneDirty();
                    }

                    // Debug Draw checkbox
                    if (ImGui::Checkbox("Debug Draw Bounds", &gsc.debugDraw)) {
                        dirtyState.MarkSceneDirty();
                    }

                    ImGui::Spacing();
                    if (ImGui::Button("Remove Ground Section Component")) {
                        coordinator.RemoveComponent<eng::runtime::GroundSectionComponent>(selectedEntity);
                        dirtyState.MarkSceneDirty();
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
                if (!sig.test(coordinator.GetComponentType<PlayerStartComponent>())) {
                    if (ImGui::MenuItem("PlayerStart Component")) {
                        coordinator.AddComponent<PlayerStartComponent>(selectedEntity, PlayerStartComponent());
                        dirtyState.MarkSceneDirty();
                    }
                }
                if (!sig.test(coordinator.GetComponentType<CharacterControllerComponent>())) {
                    if (ImGui::MenuItem("CharacterController Component")) {
                        coordinator.AddComponent<CharacterControllerComponent>(selectedEntity, CharacterControllerComponent());
                        dirtyState.MarkSceneDirty();
                    }
                }
                if (!sig.test(coordinator.GetComponentType<CameraComponent>())) {
                    if (ImGui::MenuItem("Camera Component")) {
                        coordinator.AddComponent<CameraComponent>(selectedEntity, CameraComponent());
                        dirtyState.MarkSceneDirty();
                    }
                }
                if (!sig.test(coordinator.GetComponentType<InputComponent>())) {
                    if (ImGui::MenuItem("Input Component")) {
                        coordinator.AddComponent<InputComponent>(selectedEntity, InputComponent());
                        dirtyState.MarkSceneDirty();
                    }
                }
                if (!sig.test(coordinator.GetComponentType<TriggerComponent>())) {
                    if (ImGui::MenuItem("Trigger Component")) {
                        coordinator.AddComponent<TriggerComponent>(selectedEntity, TriggerComponent());
                        dirtyState.MarkSceneDirty();
                    }
                }
                if (!sig.test(coordinator.GetComponentType<DirectionalLightComponent>())) {
                    if (ImGui::MenuItem("DirectionalLight Component")) {
                        coordinator.AddComponent<DirectionalLightComponent>(selectedEntity, DirectionalLightComponent());
                        dirtyState.MarkSceneDirty();
                    }
                }
                if (!sig.test(coordinator.GetComponentType<PointLightComponent>())) {
                    if (ImGui::MenuItem("PointLight Component")) {
                        coordinator.AddComponent<PointLightComponent>(selectedEntity, PointLightComponent());
                        dirtyState.MarkSceneDirty();
                    }
                }
                if (!sig.test(coordinator.GetComponentType<SkyLightComponent>())) {
                    if (ImGui::MenuItem("SkyLight Component")) {
                        coordinator.AddComponent<SkyLightComponent>(selectedEntity, SkyLightComponent());
                        dirtyState.MarkSceneDirty();
                    }
                }
                if (!sig.test(coordinator.GetComponentType<SpotLightComponent>())) {
                    if (ImGui::MenuItem("SpotLight Component")) {
                        coordinator.AddComponent<SpotLightComponent>(selectedEntity, SpotLightComponent());
                        dirtyState.MarkSceneDirty();
                    }
                }
                if (!sig.test(coordinator.GetComponentType<BoundsComponent>())) {
                    if (ImGui::MenuItem("Bounds Component")) {
                        coordinator.AddComponent<BoundsComponent>(selectedEntity, BoundsComponent());
                        dirtyState.MarkSceneDirty();
                    }
                }
                if (!sig.test(coordinator.GetComponentType<eng::runtime::ZoneMembershipComponent>())) {
                    if (ImGui::MenuItem("ZoneMembership Component")) {
                        coordinator.AddComponent<eng::runtime::ZoneMembershipComponent>(selectedEntity, eng::runtime::ZoneMembershipComponent());
                        dirtyState.MarkSceneDirty();
                    }
                }
                if (!sig.test(coordinator.GetComponentType<eng::runtime::GroundSectionComponent>())) {
                    if (ImGui::MenuItem("GroundSection Component")) {
                        coordinator.AddComponent<eng::runtime::GroundSectionComponent>(selectedEntity, eng::runtime::GroundSectionComponent());
                        dirtyState.MarkSceneDirty();
                    }
                }
                ImGui::Separator();
                if (ImGui::BeginMenu("Gameplay Components")) {
                    if (!sig.test(coordinator.GetComponentType<InteractableComponent>())) {
                        if (ImGui::MenuItem("Interactable Component")) {
                            coordinator.AddComponent<InteractableComponent>(selectedEntity, InteractableComponent());
                            dirtyState.MarkSceneDirty();
                        }
                    }
                    if (!sig.test(coordinator.GetComponentType<ObjectiveComponent>())) {
                        if (ImGui::MenuItem("Objective Component")) {
                            coordinator.AddComponent<ObjectiveComponent>(selectedEntity, ObjectiveComponent());
                            dirtyState.MarkSceneDirty();
                        }
                    }
                    if (!sig.test(coordinator.GetComponentType<AudioSourceComponent>())) {
                        if (ImGui::MenuItem("AudioSource Component")) {
                            coordinator.AddComponent<AudioSourceComponent>(selectedEntity, AudioSourceComponent());
                            dirtyState.MarkSceneDirty();
                        }
                    }
                    if (!sig.test(coordinator.GetComponentType<SimpleStateComponent>())) {
                        if (ImGui::MenuItem("SimpleState Component")) {
                            coordinator.AddComponent<SimpleStateComponent>(selectedEntity, SimpleStateComponent());
                            dirtyState.MarkSceneDirty();
                        }
                    }
                    if (!sig.test(coordinator.GetComponentType<ActivatableComponent>())) {
                        if (ImGui::MenuItem("Activatable Component")) {
                            coordinator.AddComponent<ActivatableComponent>(selectedEntity, ActivatableComponent());
                            dirtyState.MarkSceneDirty();
                        }
                    }
                    if (!sig.test(coordinator.GetComponentType<DoorComponent>())) {
                        if (ImGui::MenuItem("Door Component")) {
                            coordinator.AddComponent<DoorComponent>(selectedEntity, DoorComponent());
                            dirtyState.MarkSceneDirty();
                        }
                    }
                    if (!sig.test(coordinator.GetComponentType<CheckpointComponent>())) {
                        if (ImGui::MenuItem("Checkpoint Component")) {
                            coordinator.AddComponent<CheckpointComponent>(selectedEntity, CheckpointComponent());
                            dirtyState.MarkSceneDirty();
                        }
                    }
                    ImGui::EndMenu();
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
