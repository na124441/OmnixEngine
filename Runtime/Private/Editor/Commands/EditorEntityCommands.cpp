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

    Entity EditorEntityCommands::CreatePlayerStart(Coordinator& coordinator, EditorDirtyState& dirtyState, EditorSelection& selection) {
        Entity newEntity = coordinator.CreateEntity();
        coordinator.AddComponent<NameComponent>(newEntity, NameComponent("PlayerStart"));
        coordinator.AddComponent<TransformComponent>(newEntity, TransformComponent());
        coordinator.AddComponent<PlayerStartComponent>(newEntity, PlayerStartComponent());
        
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

        // Copy PlayerStart
        if (srcSignature.test(coordinator.GetComponentType<PlayerStartComponent>())) {
            const auto& srcComp = coordinator.GetComponent<PlayerStartComponent>(source);
            coordinator.AddComponent<PlayerStartComponent>(duplicate, srcComp);
        }

        // Copy CharacterController
        if (srcSignature.test(coordinator.GetComponentType<CharacterControllerComponent>())) {
            const auto& srcComp = coordinator.GetComponent<CharacterControllerComponent>(source);
            coordinator.AddComponent<CharacterControllerComponent>(duplicate, srcComp);
        }

        // Copy CameraComponent
        if (srcSignature.test(coordinator.GetComponentType<CameraComponent>())) {
            const auto& srcComp = coordinator.GetComponent<CameraComponent>(source);
            coordinator.AddComponent<CameraComponent>(duplicate, srcComp);
        }

        // Copy Input
        if (srcSignature.test(coordinator.GetComponentType<InputComponent>())) {
            const auto& srcComp = coordinator.GetComponent<InputComponent>(source);
            coordinator.AddComponent<InputComponent>(duplicate, srcComp);
        }

        // Copy Trigger
        if (srcSignature.test(coordinator.GetComponentType<TriggerComponent>())) {
            const auto& srcComp = coordinator.GetComponent<TriggerComponent>(source);
            coordinator.AddComponent<TriggerComponent>(duplicate, srcComp);
        }

        // Copy Interactable
        if (srcSignature.test(coordinator.GetComponentType<InteractableComponent>())) {
            const auto& srcComp = coordinator.GetComponent<InteractableComponent>(source);
            coordinator.AddComponent<InteractableComponent>(duplicate, srcComp);
        }

        // Copy DirectionalLight
        if (srcSignature.test(coordinator.GetComponentType<DirectionalLightComponent>())) {
            const auto& srcComp = coordinator.GetComponent<DirectionalLightComponent>(source);
            coordinator.AddComponent<DirectionalLightComponent>(duplicate, srcComp);
        }

        // Copy PointLight
        if (srcSignature.test(coordinator.GetComponentType<PointLightComponent>())) {
            const auto& srcComp = coordinator.GetComponent<PointLightComponent>(source);
            coordinator.AddComponent<PointLightComponent>(duplicate, srcComp);
        }

        // Copy SkyLight
        if (srcSignature.test(coordinator.GetComponentType<SkyLightComponent>())) {
            const auto& srcComp = coordinator.GetComponent<SkyLightComponent>(source);
            coordinator.AddComponent<SkyLightComponent>(duplicate, srcComp);
        }

        // Copy SpotLight
        if (srcSignature.test(coordinator.GetComponentType<SpotLightComponent>())) {
            const auto& srcComp = coordinator.GetComponent<SpotLightComponent>(source);
            coordinator.AddComponent<SpotLightComponent>(duplicate, srcComp);
        }

        dirtyState.MarkSceneDirty();
        selection.Select(duplicate);
        return duplicate;
    }

    Entity EditorEntityCommands::CreateDirectionalLight(Coordinator& coordinator, EditorDirtyState& dirtyState, EditorSelection& selection) {
        Entity newEntity = coordinator.CreateEntity();
        coordinator.AddComponent<NameComponent>(newEntity, NameComponent("Directional Light"));
        coordinator.AddComponent<TransformComponent>(newEntity, TransformComponent());
        coordinator.AddComponent<DirectionalLightComponent>(newEntity, DirectionalLightComponent());
        
        dirtyState.MarkSceneDirty();
        selection.Select(newEntity);
        return newEntity;
    }

    Entity EditorEntityCommands::CreatePointLight(Coordinator& coordinator, EditorDirtyState& dirtyState, EditorSelection& selection) {
        Entity newEntity = coordinator.CreateEntity();
        coordinator.AddComponent<NameComponent>(newEntity, NameComponent("Point Light"));
        coordinator.AddComponent<TransformComponent>(newEntity, TransformComponent());
        coordinator.AddComponent<PointLightComponent>(newEntity, PointLightComponent());
        
        dirtyState.MarkSceneDirty();
        selection.Select(newEntity);
        return newEntity;
    }

    Entity EditorEntityCommands::CreateSkyLight(Coordinator& coordinator, EditorDirtyState& dirtyState, EditorSelection& selection) {
        Entity newEntity = coordinator.CreateEntity();
        coordinator.AddComponent<NameComponent>(newEntity, NameComponent("Sky Light"));
        coordinator.AddComponent<TransformComponent>(newEntity, TransformComponent());
        coordinator.AddComponent<SkyLightComponent>(newEntity, SkyLightComponent());
        
        dirtyState.MarkSceneDirty();
        selection.Select(newEntity);
        return newEntity;
    }

    Entity EditorEntityCommands::CreateSpotLight(Coordinator& coordinator, EditorDirtyState& dirtyState, EditorSelection& selection) {
        Entity newEntity = coordinator.CreateEntity();
        coordinator.AddComponent<NameComponent>(newEntity, NameComponent("Spot Light"));
        coordinator.AddComponent<TransformComponent>(newEntity, TransformComponent());
        coordinator.AddComponent<SpotLightComponent>(newEntity, SpotLightComponent());
        
        dirtyState.MarkSceneDirty();
        selection.Select(newEntity);
        return newEntity;
    }

} // namespace eng::runtime
