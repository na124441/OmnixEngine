#include "Runtime/Public/Editor/EditorEntityCommands.h"
#include <string>

namespace eng::runtime {

    Entity EditorEntityCommands::CreateEmpty(Coordinator& coordinator, EditorDirtyState& dirtyState, EditorSelection& selection) {
        Entity newEntity = coordinator.CreateEntity();
        coordinator.AddComponent<NameComponent>(newEntity, NameComponent("Empty Entity"));
        coordinator.AddComponent<TransformComponent>(newEntity, TransformComponent());
        
        dirtyState.MarkSceneDirty();
        selection.Select(newEntity);
        return newEntity;
    }

    void EditorEntityCommands::Delete(Coordinator& coordinator, Entity entity, EditorDirtyState& dirtyState, EditorSelection& selection) {
        if (entity != 0 && coordinator.IsEntityAlive(entity)) {
            coordinator.DestroyEntity(entity);
            dirtyState.MarkSceneDirty();
            if (selection.GetSelectedEntity() == entity) {
                selection.Clear();
            }
        }
    }

    Entity EditorEntityCommands::Duplicate(Coordinator& coordinator, Entity source, EditorDirtyState& dirtyState, EditorSelection& selection) {
        if (source == 0 || !coordinator.IsEntityAlive(source)) {
            return 0;
        }

        Entity duplicate = coordinator.CreateEntity();
        Signature srcSignature = coordinator.GetSignature(source);

        // Copy Name
        if (srcSignature.test(coordinator.GetComponentType<NameComponent>())) {
            const auto& srcName = coordinator.GetComponent<NameComponent>(source);
            coordinator.AddComponent<NameComponent>(duplicate, NameComponent(srcName.name + " Copy"));
        } else {
            coordinator.AddComponent<NameComponent>(duplicate, NameComponent("Entity " + std::to_string(duplicate)));
        }

        // Copy Transform
        if (srcSignature.test(coordinator.GetComponentType<TransformComponent>())) {
            const auto& srcTrans = coordinator.GetComponent<TransformComponent>(source);
            coordinator.AddComponent<TransformComponent>(duplicate, srcTrans);
        }

        // Copy Tag
        if (srcSignature.test(coordinator.GetComponentType<TagComponent>())) {
            const auto& srcTag = coordinator.GetComponent<TagComponent>(source);
            coordinator.AddComponent<TagComponent>(duplicate, srcTag);
        }

        // Copy Layer
        if (srcSignature.test(coordinator.GetComponentType<LayerComponent>())) {
            const auto& srcLayer = coordinator.GetComponent<LayerComponent>(source);
            coordinator.AddComponent<LayerComponent>(duplicate, srcLayer);
        }

        // Copy MeshRenderer
        if (srcSignature.test(coordinator.GetComponentType<MeshRendererComponent>())) {
            const auto& srcMR = coordinator.GetComponent<MeshRendererComponent>(source);
            coordinator.AddComponent<MeshRendererComponent>(duplicate, srcMR);
        }

        // Copy Health
        if (srcSignature.test(coordinator.GetComponentType<HealthComponent>())) {
            const auto& srcHealth = coordinator.GetComponent<HealthComponent>(source);
            coordinator.AddComponent<HealthComponent>(duplicate, srcHealth);
        }

        // Copy RigidBody
        if (srcSignature.test(coordinator.GetComponentType<RigidBodyComponent>())) {
            const auto& srcRB = coordinator.GetComponent<RigidBodyComponent>(source);
            coordinator.AddComponent<RigidBodyComponent>(duplicate, srcRB);
        }

        // Copy Collider
        if (srcSignature.test(coordinator.GetComponentType<ColliderComponent>())) {
            const auto& srcCol = coordinator.GetComponent<ColliderComponent>(source);
            coordinator.AddComponent<ColliderComponent>(duplicate, srcCol);
        }

        // Copy PlayerController
        if (srcSignature.test(coordinator.GetComponentType<PlayerControllerComponent>())) {
            const auto& srcPC = coordinator.GetComponent<PlayerControllerComponent>(source);
            coordinator.AddComponent<PlayerControllerComponent>(duplicate, srcPC);
        }

        // Copy RenderableMesh
        if (srcSignature.test(coordinator.GetComponentType<RenderableMeshComponent>())) {
            const auto& srcRM = coordinator.GetComponent<RenderableMeshComponent>(source);
            coordinator.AddComponent<RenderableMeshComponent>(duplicate, srcRM);
        }

        // Copy Material
        if (srcSignature.test(coordinator.GetComponentType<MaterialComponent>())) {
            const auto& srcMat = coordinator.GetComponent<MaterialComponent>(source);
            coordinator.AddComponent<MaterialComponent>(duplicate, srcMat);
        }

        // Copy StaticBody
        if (srcSignature.test(coordinator.GetComponentType<StaticBodyComponent>())) {
            const auto& srcComp = coordinator.GetComponent<StaticBodyComponent>(source);
            coordinator.AddComponent<StaticBodyComponent>(duplicate, srcComp);
        }

        // Copy BoxCollider
        if (srcSignature.test(coordinator.GetComponentType<BoxColliderComponent>())) {
            const auto& srcComp = coordinator.GetComponent<BoxColliderComponent>(source);
            coordinator.AddComponent<BoxColliderComponent>(duplicate, srcComp);
        }

        // Copy SphereCollider
        if (srcSignature.test(coordinator.GetComponentType<SphereColliderComponent>())) {
            const auto& srcComp = coordinator.GetComponent<SphereColliderComponent>(source);
            coordinator.AddComponent<SphereColliderComponent>(duplicate, srcComp);
        }

        // Copy CapsuleCollider
        if (srcSignature.test(coordinator.GetComponentType<CapsuleColliderComponent>())) {
            const auto& srcComp = coordinator.GetComponent<CapsuleColliderComponent>(source);
            coordinator.AddComponent<CapsuleColliderComponent>(duplicate, srcComp);
        }

        dirtyState.MarkSceneDirty();
        selection.Select(duplicate);
        return duplicate;
    }

} // namespace eng::runtime
