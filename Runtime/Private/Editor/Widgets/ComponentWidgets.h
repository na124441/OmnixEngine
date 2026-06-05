#pragma once

#include "Runtime/Public/Editor/EditorDirtyState.h"
#include "ECS/ECSComponents.h"

namespace eng::runtime {

    class AssetRegistry;

    class ComponentWidgets {
    public:
        static bool DrawName(NameComponent& component, EditorDirtyState& dirtyState);
        static bool DrawTag(TagComponent& component, EditorDirtyState& dirtyState);
        static bool DrawLayer(LayerComponent& component, EditorDirtyState& dirtyState);
        static bool DrawHealth(HealthComponent& component, EditorDirtyState& dirtyState);
        static bool DrawRigidBody(RigidBodyComponent& component, EditorDirtyState& dirtyState);
        static bool DrawCollider(ColliderComponent& component, EditorDirtyState& dirtyState);
        static bool DrawMeshRenderer(MeshRendererComponent& component, EditorDirtyState& dirtyState);
        static bool DrawPlayerController(PlayerControllerComponent& component, EditorDirtyState& dirtyState);
        static bool DrawRenderableMesh(RenderableMeshComponent& component, AssetRegistry& registry, EditorDirtyState& dirtyState);
        static bool DrawMaterial(MaterialComponent& component, AssetRegistry& registry, EditorDirtyState& dirtyState);
        static bool DrawStaticBody(StaticBodyComponent& component, EditorDirtyState& dirtyState);
        static bool DrawBoxCollider(BoxColliderComponent& component, EditorDirtyState& dirtyState);
        static bool DrawSphereCollider(SphereColliderComponent& component, EditorDirtyState& dirtyState);
        static bool DrawCapsuleCollider(CapsuleColliderComponent& component, EditorDirtyState& dirtyState);
        static bool DrawPlayerStart(PlayerStartComponent& component, EditorDirtyState& dirtyState);
        static bool DrawCharacterController(CharacterControllerComponent& component, EditorDirtyState& dirtyState);
        static bool DrawCamera(CameraComponent& component, EditorDirtyState& dirtyState);
        static bool DrawInput(InputComponent& component, EditorDirtyState& dirtyState);
        static bool DrawTrigger(TriggerComponent& component, EditorDirtyState& dirtyState);
        static bool DrawInteractable(InteractableComponent& component, EditorDirtyState& dirtyState);
        static bool DrawObjective(ObjectiveComponent& component, EditorDirtyState& dirtyState);
        static bool DrawDirectionalLight(DirectionalLightComponent& component, EditorDirtyState& dirtyState);
        static bool DrawPointLight(PointLightComponent& component, EditorDirtyState& dirtyState);
        static bool DrawAmbientLight(AmbientLightComponent& component, EditorDirtyState& dirtyState);
        static bool DrawSpotLight(SpotLightComponent& component, EditorDirtyState& dirtyState);
    };

} // namespace eng::runtime
