#include "Runtime/Private/Editor/Panels/InspectorPanel.h"
#include "Runtime/Private/Editor/Widgets/TransformWidget.h"
#include "Runtime/Private/Editor/Widgets/ComponentWidgets.h"
#include "Physics/Public/PhysicsWorld.h"
#include "ECS/Coordinator.h"
#include "ECS/ECSComponents.h"
#include "ECS/Public/IECSWorld.h"
#include "ThirdParty/imgui/imgui.h"
#include "Runtime/Public/AssetRegistry.h"
#include "RenderingEngine/Runtime/engine/EngineLoop.h"
#include "RenderingEngine/Renderer/SceneRenderer.h"
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

            // Ambient Light Component
            if (sig.test(coordinator.GetComponentType<AmbientLightComponent>())) {
                if (ImGui::CollapsingHeader("Ambient Light Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& comp = coordinator.GetComponent<AmbientLightComponent>(selectedEntity);
                    ComponentWidgets::DrawAmbientLight(comp, dirtyState);
                    ImGui::Spacing();
                    if (ImGui::Button("Remove Ambient Light Component")) {
                        coordinator.RemoveComponent<AmbientLightComponent>(selectedEntity);
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
                if (!sig.test(coordinator.GetComponentType<InteractableComponent>())) {
                    if (ImGui::MenuItem("Interactable Component")) {
                        coordinator.AddComponent<InteractableComponent>(selectedEntity, InteractableComponent());
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
                if (!sig.test(coordinator.GetComponentType<AmbientLightComponent>())) {
                    if (ImGui::MenuItem("AmbientLight Component")) {
                        coordinator.AddComponent<AmbientLightComponent>(selectedEntity, AmbientLightComponent());
                        dirtyState.MarkSceneDirty();
                    }
                }
                if (!sig.test(coordinator.GetComponentType<SpotLightComponent>())) {
                    if (ImGui::MenuItem("SpotLight Component")) {
                        coordinator.AddComponent<SpotLightComponent>(selectedEntity, SpotLightComponent());
                        dirtyState.MarkSceneDirty();
                    }
                }
                if (!sig.test(coordinator.GetComponentType<ObjectiveComponent>())) {
                    if (ImGui::MenuItem("Objective Component")) {
                        coordinator.AddComponent<ObjectiveComponent>(selectedEntity, ObjectiveComponent());
                        dirtyState.MarkSceneDirty();
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
