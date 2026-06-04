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

// System Includes
#include "../Components/Behavior/MovementCapability.h"
#include "../ECS/PhysicsSystem.h"
#include "../ECS/RenderSystem.h"
#include "../ECS/PlayerSystem.h"
#include "../ECS/CameraSystem.h"

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

        // 2. Register Systems & Signatures

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
