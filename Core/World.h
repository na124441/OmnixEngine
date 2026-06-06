//==========================================================================
// Core/World.h
//
// One‑stop wrapper for the ECS world state.
//==========================================================================

#pragma once

#include "Coordinator.h"
#include "SystemManager.h"
#include "../ECS/ECSComponents.h"
#include "ECS/Public/IECSWorld.h"
#include "Runtime/Public/Gameplay/PlayerStateComponent.h"
#include "Runtime/Public/World/ZoneEntityComponent.h"

// System Includes
#include "../Components/Behavior/MovementCapability.h"
#include "../ECS/PhysicsSystem.h"
#include "../ECS/RenderSystem.h"
#include "../ECS/PlayerSystem.h"
#include "../ECS/CameraSystem.h"
#include "../ECS/PlayerControllerSystem.h"
#include "../ECS/TriggerSystem.h"
#include "../ECS/LightCollectionSystem.h"
#include "Runtime/Public/Gameplay/Systems/InteractionSystem.h"
#include "../ECS/BoundsUpdateSystem.h"

namespace eng::runtime {

class World : public IECSWorld
{
public:
    using SystemFn = std::function<void(World&, float)>;

    World()
    {
        m_coordinator.Init();

        // 1. Register Components
        m_coordinator.RegisterComponent<TransformComponent>();
        m_coordinator.RegisterComponent<RigidBodyComponent>();
        m_coordinator.RegisterComponent<MeshRendererComponent>();
        m_coordinator.RegisterComponent<CameraComponent>();
        m_coordinator.RegisterComponent<ColliderComponent>();
        m_coordinator.RegisterComponent<PlayerControllerComponent>();
        m_coordinator.RegisterComponent<TagComponent>();
        m_coordinator.RegisterComponent<LayerComponent>();
        m_coordinator.RegisterComponent<HealthComponent>();
        m_coordinator.RegisterComponent<RenderableMeshComponent>();
        m_coordinator.RegisterComponent<MaterialComponent>();
        m_coordinator.RegisterComponent<StaticBodyComponent>();
        m_coordinator.RegisterComponent<BoxColliderComponent>();
        m_coordinator.RegisterComponent<SphereColliderComponent>();
        m_coordinator.RegisterComponent<CapsuleColliderComponent>();
        m_coordinator.RegisterComponent<NameComponent>();
        m_coordinator.RegisterComponent<LightComponent>();
        m_coordinator.RegisterComponent<AudioSourceComponent>();
        m_coordinator.RegisterComponent<AnimatorComponent>();
        m_coordinator.RegisterComponent<ScriptComponent>();
        m_coordinator.RegisterComponent<PlayerStartComponent>();
        m_coordinator.RegisterComponent<CharacterControllerComponent>();
        m_coordinator.RegisterComponent<InputComponent>();
        m_coordinator.RegisterComponent<TriggerComponent>();
        m_coordinator.RegisterComponent<InteractableComponent>();
        m_coordinator.RegisterComponent<DirectionalLightComponent>();
        m_coordinator.RegisterComponent<PointLightComponent>();
        m_coordinator.RegisterComponent<AmbientLightComponent>();
        m_coordinator.RegisterComponent<SpotLightComponent>();
        m_coordinator.RegisterComponent<PlayerStateComponent>();
        m_coordinator.RegisterComponent<PlayerTagComponent>();
        m_coordinator.RegisterComponent<ObjectiveComponent>();
        m_coordinator.RegisterComponent<SimpleStateComponent>();
        m_coordinator.RegisterComponent<ActivatableComponent>();
        m_coordinator.RegisterComponent<DoorComponent>();
        m_coordinator.RegisterComponent<CheckpointComponent>();
        m_coordinator.RegisterComponent<ZoneEntityComponent>();
        m_coordinator.RegisterComponent<BoundsComponent>();

        // 2. Register Systems & Signatures

        // --- PlayerControllerSystem ---
        {
            auto playerControllerSys = m_coordinator.RegisterSystem<PlayerControllerSystem>();
            ::Signature sig;
            sig.set(m_coordinator.GetComponentType<TransformComponent>());
            sig.set(m_coordinator.GetComponentType<CharacterControllerComponent>());
            sig.set(m_coordinator.GetComponentType<CameraComponent>());
            m_coordinator.SetSystemSignature<PlayerControllerSystem>(sig);
        }

        // --- PlayerSystem ---
        {
            auto playerSys = m_coordinator.RegisterSystem<PlayerSystem>();
            ::Signature sig;
            sig.set(m_coordinator.GetComponentType<TransformComponent>());
            sig.set(m_coordinator.GetComponentType<RigidBodyComponent>());
            sig.set(m_coordinator.GetComponentType<PlayerControllerComponent>());
            m_coordinator.SetSystemSignature<PlayerSystem>(sig);
        }

        // --- PhysicsSystem ---
        {
            auto physicsSys = m_coordinator.RegisterSystem<PhysicsSystem>();
            ::Signature sig;
            sig.set(m_coordinator.GetComponentType<TransformComponent>());
            sig.set(m_coordinator.GetComponentType<RigidBodyComponent>());
            m_coordinator.SetSystemSignature<PhysicsSystem>(sig);
        }

        // --- CameraSystem ---
        {
            auto cameraSys = m_coordinator.RegisterSystem<CameraSystem>();
            ::Signature sig;
            sig.set(m_coordinator.GetComponentType<TransformComponent>());
            sig.set(m_coordinator.GetComponentType<CameraComponent>());
            m_coordinator.SetSystemSignature<CameraSystem>(sig);
        }

        // --- RenderSystem ---
        {
            auto renderSys = m_coordinator.RegisterSystem<RenderSystem>();
            ::Signature sig;
            sig.set(m_coordinator.GetComponentType<TransformComponent>());
            sig.set(m_coordinator.GetComponentType<MeshRendererComponent>());
            m_coordinator.SetSystemSignature<RenderSystem>(sig);
        }

        // --- TriggerSystem ---
        {
            auto triggerSys = m_coordinator.RegisterSystem<TriggerSystem>();
            ::Signature sig;
            sig.set(m_coordinator.GetComponentType<TransformComponent>());
            sig.set(m_coordinator.GetComponentType<TriggerComponent>());
            m_coordinator.SetSystemSignature<TriggerSystem>(sig);
        }

        // --- LightCollectionSystem ---
        {
            auto lightCollectionSys = m_coordinator.RegisterSystem<LightCollectionSystem>();
            ::Signature sig;
            sig.set(m_coordinator.GetComponentType<TransformComponent>());
            m_coordinator.SetSystemSignature<LightCollectionSystem>(sig);
        }

        // --- InteractionSystem ---
        {
            auto interactionSys = m_coordinator.RegisterSystem<InteractionSystem>();
            ::Signature sig;
            sig.set(m_coordinator.GetComponentType<TransformComponent>());
            sig.set(m_coordinator.GetComponentType<InteractableComponent>());
            m_coordinator.SetSystemSignature<InteractionSystem>(sig);
        }

        // --- BoundsUpdateSystem ---
        {
            auto boundsSys = m_coordinator.RegisterSystem<BoundsUpdateSystem>();
            ::Signature sig;
            sig.set(m_coordinator.GetComponentType<TransformComponent>());
            sig.set(m_coordinator.GetComponentType<BoundsComponent>());
            m_coordinator.SetSystemSignature<BoundsUpdateSystem>(sig);
        }

        CORE_LOG_INFO("World: Golden Scene ECS initialized");
    }

    Coordinator& getCoordinator() override { return m_coordinator; }

    std::unique_ptr<IECSWorld> Clone() const override {
        auto clone = std::make_unique<World>();
        clone->m_coordinator.CopyFrom(m_coordinator);
        return clone;
    }

    void Initialize() override {
        CORE_LOG_INFO("World: ECS World Initialize called");
    }

    void Shutdown() override {
        CORE_LOG_INFO("World: ECS World Shutdown called");
        m_Systems.clear();
    }

    template<typename T> std::shared_ptr<T> GetSystem() { return m_coordinator.GetSystem<T>(); }

    // --------------------------------------------------------------------
    // Compatibility with RenderingEngine ECS
    // --------------------------------------------------------------------
    ::Entity CreateEntity() { return m_coordinator.CreateEntity(); }
    void DestroyEntity(::Entity e) { m_coordinator.DestroyEntity(e); }

    template<typename T> void AddComponent(::Entity e, T component) { m_coordinator.AddComponent<T>(e, component); }
    template<typename T> T& GetComponent(::Entity e) { return m_coordinator.GetComponent<T>(e); }

    void RegisterSystem(SystemFn sys) { m_Systems.push_back(std::move(sys)); }
    
    void Update(float deltaTime) override {
        for (auto& sys : m_Systems) {
            sys(*this, deltaTime);
        }
    }

private:
    Coordinator m_coordinator;
    std::vector<SystemFn> m_Systems;
};

} // namespace eng::runtime
