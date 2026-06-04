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
    };

} // namespace eng::runtime
