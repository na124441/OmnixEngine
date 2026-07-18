#include "Gameplay/StateObjects/ObjectActivationSystem.h"
#include "Runtime/RuntimeContext.h"
#include "Gameplay/GameplayEventBus.h"
#include "ECS/ECSComponents.h"
#include "ECS/Coordinator.h"
#include "Core/World.h"
#include "Core/Logger.h"
#include "ThirdParty/imgui/imgui.h"
#include <algorithm>
#include <iostream>

namespace eng::runtime {

    static Vector3 Lerp(const Vector3& start, const Vector3& end, float t)
    {
        return start + (end - start) * t;
    }

    static float DistanceSquared(const Vector3& a, const Vector3& b)
    {
        Vector3 diff = a - b;
        return diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
    }

    void ObjectActivationSystem::Initialize(RuntimeContext* context)
    {
        m_Context = context;
    }

    void ObjectActivationSystem::OnPlayStart()
    {
        if (!m_Context || !m_Context->ecs) return;

        auto& coordinator = m_Context->ecs->getCoordinator();
        const auto& entities = coordinator.GetActiveEntities();
        
        auto doorType = coordinator.GetComponentType<DoorComponent>();
        auto transformType = coordinator.GetComponentType<TransformComponent>();
        auto stateType = coordinator.GetComponentType<SimpleStateComponent>();
        auto activatableType = coordinator.GetComponentType<ActivatableComponent>();

        for (Entity entity : entities)
        {
            if (entity != 0 && coordinator.IsEntityAlive(entity))
            {
                auto signature = coordinator.GetSignature(entity);
                if (signature.test(doorType) && signature.test(transformType))
                {
                    auto& door = coordinator.GetComponent<DoorComponent>(entity);
                    auto& transform = coordinator.GetComponent<TransformComponent>(entity);
                    door.ClosedPosition = transform.position;
                    door.IsOpen = false;
                    door.IsOpening = false;
                }
                if (signature.test(stateType))
                {
                    auto& state = coordinator.GetComponent<SimpleStateComponent>(entity);
                    if (state.ResetOnPlay)
                    {
                        state.Reset();
                    }
                }
                if (signature.test(activatableType))
                {
                    auto& activatable = coordinator.GetComponent<ActivatableComponent>(entity);
                    activatable.HasActivated = false;
                }
            }
        }

        m_LastActivationID = "None";
        m_LastActivationError = "None";

        // Subscribe to event bus
        if (m_Context && m_Context->gameplayEventBus)
        {
            m_Context->gameplayEventBus->Subscribe(GameplayEventType::Interaction, [this](const GameplayEvent& e) {
                OnGameplayEvent(e);
            });
        }
    }

    void ObjectActivationSystem::OnPlayStop()
    {
        if (!m_Context || !m_Context->ecs) return;

        auto& coordinator = m_Context->ecs->getCoordinator();
        const auto& entities = coordinator.GetActiveEntities();
        
        auto doorType = coordinator.GetComponentType<DoorComponent>();
        auto transformType = coordinator.GetComponentType<TransformComponent>();

        for (Entity entity : entities)
        {
            if (entity != 0 && coordinator.IsEntityAlive(entity))
            {
                auto signature = coordinator.GetSignature(entity);
                if (signature.test(doorType) && signature.test(transformType))
                {
                    auto& door = coordinator.GetComponent<DoorComponent>(entity);
                    auto& transform = coordinator.GetComponent<TransformComponent>(entity);
                    transform.position = door.ClosedPosition;
                    door.IsOpen = false;
                    door.IsOpening = false;
                }
            }
        }
    }

    void ObjectActivationSystem::Update(float dt)
    {
        if (!m_Context || !m_Context->ecs) return;

        // Only move doors if we are in Play/Simulation Mode
        bool isPlay = (m_Context->mode == RuntimeMode::Game) ||
                      (m_Context->mode == RuntimeMode::Editor && m_Context->editorSimulationState == EditorSimulationState::Play);
        if (!isPlay) return;

        auto& coordinator = m_Context->ecs->getCoordinator();
        const auto& entities = coordinator.GetActiveEntities();
        
        auto doorType = coordinator.GetComponentType<DoorComponent>();
        auto transformType = coordinator.GetComponentType<TransformComponent>();
        auto stateType = coordinator.GetComponentType<SimpleStateComponent>();

        for (Entity entity : entities)
        {
            if (entity != 0 && coordinator.IsEntityAlive(entity))
            {
                auto signature = coordinator.GetSignature(entity);
                if (signature.test(doorType) && signature.test(transformType))
                {
                    auto& door = coordinator.GetComponent<DoorComponent>(entity);
                    bool hasState = signature.test(stateType);

                    bool shouldMove = door.IsOpening;
                    if (hasState)
                    {
                        auto& state = coordinator.GetComponent<SimpleStateComponent>(entity);
                        if (state.CurrentState == SimpleObjectState::Active)
                        {
                            shouldMove = true;
                        }
                    }

                    if (door.OpenMode == DoorOpenMode::Smooth && shouldMove)
                    {
                        auto& transform = coordinator.GetComponent<TransformComponent>(entity);
                        Vector3 target = door.ClosedPosition + door.OpenOffset;

                        transform.position = Lerp(transform.position, target, door.OpenSpeed * dt);

                        if (DistanceSquared(transform.position, target) < 0.001f)
                        {
                            transform.position = target;
                            door.IsOpen = true;
                            door.IsOpening = false;

                            if (hasState)
                            {
                                auto& state = coordinator.GetComponent<SimpleStateComponent>(entity);
                                state.CurrentState = SimpleObjectState::Completed;
                                LOG_INFO("[ObjectActivation] Entity %u state: Active -> Completed (Smooth Open Complete)", entity);
                            }
                        }
                    }
                }
            }
        }
    }

    void ObjectActivationSystem::OnGameplayEvent(const GameplayEvent& event)
    {
        // Prevent event handling in Edit Mode
        bool isPlay = (m_Context->mode == RuntimeMode::Game) ||
                      (m_Context->mode == RuntimeMode::Editor && m_Context->editorSimulationState == EditorSimulationState::Play);
        if (!isPlay) return;

        if (event.Type != GameplayEventType::Interaction)
            return;

        Entity sourceObject = event.Target;
        if (sourceObject == 0 || !m_Context || !m_Context->ecs) return;

        auto& coordinator = m_Context->ecs->getCoordinator();
        if (!coordinator.IsEntityAlive(sourceObject)) return;

        auto signature = coordinator.GetSignature(sourceObject);
        auto activatableType = coordinator.GetComponentType<ActivatableComponent>();

        if (signature.test(activatableType))
        {
            auto& activator = coordinator.GetComponent<ActivatableComponent>(sourceObject);

            if (activator.OneShot && activator.HasActivated)
                return;

            ActivateByID(activator.TargetActivationID);
            activator.HasActivated = true;
        }
    }

    void ObjectActivationSystem::ActivateByID(const std::string& activationID)
    {
        if (activationID.empty()) return;

        auto& coordinator = m_Context->ecs->getCoordinator();
        const auto& entities = coordinator.GetActiveEntities();
        auto activatableType = coordinator.GetComponentType<ActivatableComponent>();

        bool found = false;
        for (Entity entity : entities)
        {
            if (entity != 0 && coordinator.IsEntityAlive(entity))
            {
                auto signature = coordinator.GetSignature(entity);
                if (signature.test(activatableType))
                {
                    auto& act = coordinator.GetComponent<ActivatableComponent>(entity);
                    if (act.ActivationID == activationID)
                    {
                        ActivateEntity(entity);
                        found = true;
                    }
                }
            }
        }

        if (!found)
        {
            LOG_WARN("[ObjectActivation] Missing activation target ID: '%s'", activationID.c_str());
            m_LastActivationError = "Target not found: " + activationID;
        }
        else
        {
            m_LastActivationID = activationID;
            m_LastActivationError = "None";
        }
    }

    void ObjectActivationSystem::ActivateEntity(uint32_t entity)
    {
        auto& coordinator = m_Context->ecs->getCoordinator();
        auto signature = coordinator.GetSignature(entity);

        auto stateType = coordinator.GetComponentType<SimpleStateComponent>();
        auto doorType = coordinator.GetComponentType<DoorComponent>();

        bool hasState = signature.test(stateType);
        bool hasDoor = signature.test(doorType);

        if (hasState)
        {
            auto& state = coordinator.GetComponent<SimpleStateComponent>(entity);
            if (state.CurrentState == SimpleObjectState::Locked)
            {
                state.CurrentState = SimpleObjectState::Unlocked;
                LOG_INFO("[ObjectActivation] Entity %u state: Locked -> Unlocked", entity);
                
                state.CurrentState = SimpleObjectState::Active;
                LOG_INFO("[ObjectActivation] Entity %u state: Unlocked -> Active", entity);
            }
            else if (state.CurrentState == SimpleObjectState::Inactive || state.CurrentState == SimpleObjectState::Unlocked)
            {
                state.CurrentState = SimpleObjectState::Active;
                LOG_INFO("[ObjectActivation] Entity %u state: Active", entity);
            }
        }

        if (hasDoor)
        {
            auto& door = coordinator.GetComponent<DoorComponent>(entity);
            if (door.OpenMode == DoorOpenMode::Instant)
            {
                auto transformType = coordinator.GetComponentType<TransformComponent>();
                if (signature.test(transformType))
                {
                    auto& transform = coordinator.GetComponent<TransformComponent>(entity);
                    transform.position = door.ClosedPosition + door.OpenOffset;
                    door.IsOpen = true;
                    door.IsOpening = false;
                    if (hasState)
                    {
                        auto& state = coordinator.GetComponent<SimpleStateComponent>(entity);
                        state.CurrentState = SimpleObjectState::Completed;
                        LOG_INFO("[ObjectActivation] Entity %u state: Active -> Completed (Instant Open)", entity);
                    }
                }
            }
            else
            {
                door.IsOpening = true;
                door.IsOpen = false;
            }
        }
    }

    size_t ObjectActivationSystem::GetStateObjectsCount() const
    {
        if (!m_Context || !m_Context->ecs) return 0;
        auto& coordinator = m_Context->ecs->getCoordinator();
        auto stateType = coordinator.GetComponentType<SimpleStateComponent>();
        size_t count = 0;
        for (Entity ent : coordinator.GetActiveEntities())
        {
            if (ent != 0 && coordinator.IsEntityAlive(ent) && coordinator.GetSignature(ent).test(stateType))
                count++;
        }
        return count;
    }

    size_t ObjectActivationSystem::GetActivatableObjectsCount() const
    {
        if (!m_Context || !m_Context->ecs) return 0;
        auto& coordinator = m_Context->ecs->getCoordinator();
        auto activatableType = coordinator.GetComponentType<ActivatableComponent>();
        size_t count = 0;
        for (Entity ent : coordinator.GetActiveEntities())
        {
            if (ent != 0 && coordinator.IsEntityAlive(ent) && coordinator.GetSignature(ent).test(activatableType))
                count++;
        }
        return count;
    }

    size_t ObjectActivationSystem::GetDoorsCount() const
    {
        if (!m_Context || !m_Context->ecs) return 0;
        auto& coordinator = m_Context->ecs->getCoordinator();
        auto doorType = coordinator.GetComponentType<DoorComponent>();
        size_t count = 0;
        for (Entity ent : coordinator.GetActiveEntities())
        {
            if (ent != 0 && coordinator.IsEntityAlive(ent) && coordinator.GetSignature(ent).test(doorType))
                count++;
        }
        return count;
    }

    void ObjectActivationSystem::DrawDiagnosticsGUI()
    {
        if (!m_Context || !m_Context->ecs) return;
        auto& coordinator = m_Context->ecs->getCoordinator();
        const auto& entities = coordinator.GetActiveEntities();

        auto stateType = coordinator.GetComponentType<SimpleStateComponent>();
        auto activatableType = coordinator.GetComponentType<ActivatableComponent>();
        auto doorType = coordinator.GetComponentType<DoorComponent>();
        auto nameType = coordinator.GetComponentType<NameComponent>();

        for (Entity entity : entities)
        {
            if (entity != 0 && coordinator.IsEntityAlive(entity))
            {
                auto signature = coordinator.GetSignature(entity);
                bool hasState = signature.test(stateType);
                bool hasActivatable = signature.test(activatableType);
                bool hasDoor = signature.test(doorType);

                if (hasState || hasActivatable || hasDoor)
                {
                    std::string name = "Entity " + std::to_string(entity);
                    if (signature.test(nameType))
                    {
                        name = coordinator.GetComponent<NameComponent>(entity).name;
                    }

                    ImGui::Text("Object: %s", name.c_str());
                    if (hasActivatable)
                    {
                        auto& act = coordinator.GetComponent<ActivatableComponent>(entity);
                        if (!act.ActivationID.empty())
                        {
                            ImGui::Text("  ActivationID: %s", act.ActivationID.c_str());
                        }
                        if (!act.TargetActivationID.empty())
                        {
                            ImGui::Text("  TargetActivationID: %s", act.TargetActivationID.c_str());
                            ImGui::Text("  HasActivated: %s | OneShot: %s", 
                                act.HasActivated ? "true" : "false", 
                                act.OneShot ? "true" : "false");
                        }
                    }
                    if (hasState)
                    {
                        auto& state = coordinator.GetComponent<SimpleStateComponent>(entity);
                        ImGui::Text("  State: %s (Initial: %s)", 
                            SimpleObjectStateToString(state.CurrentState).c_str(), 
                            SimpleObjectStateToString(state.InitialState).c_str());
                    }
                    if (hasDoor)
                    {
                        auto& door = coordinator.GetComponent<DoorComponent>(entity);
                        ImGui::Text("  Door: %s", door.IsOpen ? "Open" : (door.IsOpening ? "Opening" : "Closed"));
                        ImGui::Text("  OpenMode: %s | Speed: %.2f", 
                            door.OpenMode == DoorOpenMode::Instant ? "Instant" : "Smooth", 
                            door.OpenSpeed);
                    }
                    ImGui::Spacing();
                }
            }
        }
    }

} // namespace eng::runtime
