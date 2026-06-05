#pragma once

#include <memory>
#include "ECSConfig.h"
#include "EntityManager.h"
#include "ComponentManager.h"
#include "SystemManager.h"
#include "ECSComponents.h"
#include "Public/ComponentTypes.h"
#include "Logger.h"
#include "Runtime/Public/Gameplay/PlayerStateComponent.h"

class Coordinator {
public:
    Coordinator() = default;

    void Init() {
        m_EntityManager = std::make_unique<EntityManager>();
        m_ComponentManager = std::make_unique<ComponentManager>();
        m_SystemManager = std::make_unique<SystemManager>();
    }

    // Entity methods
    const std::vector<Entity>& GetActiveEntities() const {
        return m_EntityManager->GetActiveEntities();
    }

    bool IsEntityAlive(Entity entity) const {
        return m_EntityManager->IsEntityAlive(entity);
    }

    std::uint32_t GetLivingEntityCount() const {
        return m_EntityManager ? m_EntityManager->m_LivingEntityCount : 0;
    }

    Entity CreateEntity() {
        return m_EntityManager->CreateEntity();
    }

    void DestroyEntity(Entity entity) {
        m_EntityManager->DestroyEntity(entity);
        m_ComponentManager->EntityDestroyed(entity);
        m_SystemManager->EntityDestroyed(entity);
    }

    // Component methods
    template<typename T>
    void RegisterComponent() {
        m_ComponentManager->RegisterComponent<T>();
    }

    template<typename T>
    void AddComponent(Entity entity, T component) {
        auto signature = m_EntityManager->GetSignature(entity);
        if (signature.test(GetComponentType<T>())) {
            m_ComponentManager->GetComponent<T>(entity) = component;
            return;
        }

        m_ComponentManager->AddComponent(entity, component);

        signature.set(GetComponentType<T>(), true);
        m_EntityManager->SetSignature(entity, signature);
        m_SystemManager->EntitySignatureChanged(entity, signature);
    }

    template<typename T>
    void RemoveComponent(Entity entity) {
        auto signature = m_EntityManager->GetSignature(entity);
        if (!signature.test(GetComponentType<T>())) {
            return;
        }

        m_ComponentManager->RemoveComponent<T>(entity);

        signature.set(GetComponentType<T>(), false);
        m_EntityManager->SetSignature(entity, signature);
        m_SystemManager->EntitySignatureChanged(entity, signature);
    }

    template<typename T>
    T& GetComponent(Entity entity) {
        return m_ComponentManager->GetComponent<T>(entity);
    }

    template<typename T>
    ComponentType GetComponentType() {
        if constexpr (std::is_same_v<T, HealthComponent>) return HEALTH_COMPONENT;
        if constexpr (std::is_same_v<T, TransformComponent>) return TRANSFORM_COMPONENT;
        if constexpr (std::is_same_v<T, RigidBodyComponent>) return PHYSICS_COMPONENT;
        if constexpr (std::is_same_v<T, MeshRendererComponent>) return MESH_RENDERER_COMPONENT;
        if constexpr (std::is_same_v<T, CameraComponent>) return CAMERA_COMPONENT;
        if constexpr (std::is_same_v<T, ColliderComponent>) return COLLIDER_COMPONENT;
        if constexpr (std::is_same_v<T, PlayerControllerComponent>) return PLAYER_CONTROLLER_COMPONENT;
        if constexpr (std::is_same_v<T, TagComponent>) return TAG_COMPONENT;
        if constexpr (std::is_same_v<T, LayerComponent>) return LAYER_COMPONENT;
        if constexpr (std::is_same_v<T, NameComponent>) return NAME_COMPONENT;
        if constexpr (std::is_same_v<T, RenderableMeshComponent>) return RENDERABLE_MESH_COMPONENT;
        if constexpr (std::is_same_v<T, MaterialComponent>) return MATERIAL_COMPONENT;
        if constexpr (std::is_same_v<T, StaticBodyComponent>) return STATIC_BODY_COMPONENT;
        if constexpr (std::is_same_v<T, BoxColliderComponent>) return BOX_COLLIDER_COMPONENT;
        if constexpr (std::is_same_v<T, SphereColliderComponent>) return SPHERE_COLLIDER_COMPONENT;
        if constexpr (std::is_same_v<T, CapsuleColliderComponent>) return CAPSULE_COLLIDER_COMPONENT;
        if constexpr (std::is_same_v<T, PlayerStartComponent>) return PLAYER_START_COMPONENT;
        if constexpr (std::is_same_v<T, CharacterControllerComponent>) return CHARACTER_CONTROLLER_COMPONENT;
        if constexpr (std::is_same_v<T, InputComponent>) return INPUT_COMPONENT;
        if constexpr (std::is_same_v<T, TriggerComponent>) return TRIGGER_COMPONENT;
        if constexpr (std::is_same_v<T, InteractableComponent>) return INTERACTABLE_COMPONENT;
        if constexpr (std::is_same_v<T, DirectionalLightComponent>) return DIRECTIONAL_LIGHT_COMPONENT;
        if constexpr (std::is_same_v<T, PointLightComponent>) return POINT_LIGHT_COMPONENT;
        if constexpr (std::is_same_v<T, AmbientLightComponent>) return AMBIENT_LIGHT_COMPONENT;
        if constexpr (std::is_same_v<T, SpotLightComponent>) return SPOT_LIGHT_COMPONENT;
        if constexpr (std::is_same_v<T, eng::runtime::PlayerStateComponent>) return PLAYER_STATE_COMPONENT;
        if constexpr (std::is_same_v<T, eng::runtime::PlayerTagComponent>) return PLAYER_TAG_COMPONENT;
        if constexpr (std::is_same_v<T, eng::runtime::ObjectiveComponent>) return OBJECTIVE_COMPONENT;
        
        // This should not happen if all components are registered and known to the serializer
        CORE_LOG_FATAL("ECS: Attempted to get type ID for unknown component type!");
        return 0; // Unreachable due to LOG_FATAL
    }

    // New: Get the component signature for an entity
    Signature GetSignature(Entity entity) const {
        return m_EntityManager->GetSignature(entity);
    }

    // System methods
    template<typename T>
    std::shared_ptr<T> RegisterSystem() {
        return m_SystemManager->RegisterSystem<T>();
    }

    template<typename T>
    void SetSystemSignature(Signature signature) {
        m_SystemManager->SetSignature<T>(signature);
    }

    template<typename T>
    std::shared_ptr<T> GetSystem() {
        return m_SystemManager->GetSystem<T>();
    }

    void CopyFrom(const Coordinator& other) {
        m_EntityManager = other.m_EntityManager->Clone();
        m_ComponentManager = other.m_ComponentManager->Clone();
        m_SystemManager = other.m_SystemManager->Clone();
    }

    std::unique_ptr<Coordinator> Clone() const {
        auto clone = std::make_unique<Coordinator>();
        clone->CopyFrom(*this);
        return clone;
    }

private:
    std::unique_ptr<EntityManager> m_EntityManager;
    std::unique_ptr<ComponentManager> m_ComponentManager;
    std::unique_ptr<SystemManager> m_SystemManager;
};
