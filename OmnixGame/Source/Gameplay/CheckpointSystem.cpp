#include "Gameplay/CheckpointSystem.h"
#include "Gameplay/GameplayEventBus.h"
#include "Gameplay/GameMode.h"
#include "Gameplay/PlayerStateComponent.h"
#include "Gameplay/Objectives/ObjectiveSystem.h"
#include "Gameplay/StateObjects/SimpleStateComponent.h"
#include "Gameplay/StateObjects/ActivatableComponent.h"
#include "Gameplay/StateObjects/DoorComponent.h"
#include "Runtime/RuntimeContext.h"
#include "Core/Logging/Logger.h"
#include "ECS/Coordinator.h"
#include "ECS/ECSComponents.h"
#include "Core/World.h"
#include <algorithm>

namespace eng::runtime {

    void CheckpointSystem::Initialize(RuntimeContext* context)
    {
        m_Context = context;
        if (!m_Context || !m_Context->gameplayEventBus)
        {
            return;
        }

        m_Context->gameplayEventBus->Subscribe(GameplayEventType::TriggerEnter, [this](const GameplayEvent& event) {
            OnGameplayEvent(event);
        });
    }

    void CheckpointSystem::OnPlayStart()
    {
        m_CurrentSnapshot.Valid = false;
        m_LastCheckpointEvent = "None";

        if (!m_Context || !m_Context->ecs) return;
        auto& coordinator = m_Context->ecs->getCoordinator();
        auto cpType = coordinator.GetComponentType<CheckpointComponent>();

        for (Entity ent : coordinator.GetActiveEntities())
        {
            if (ent != INVALID_ENTITY && coordinator.IsEntityAlive(ent) && coordinator.GetSignature(ent).test(cpType))
            {
                coordinator.GetComponent<CheckpointComponent>(ent).HasActivated = false;
            }
        }
    }

    void CheckpointSystem::OnPlayStop()
    {
        m_CurrentSnapshot.Valid = false;
        m_LastCheckpointEvent = "None";
    }

    void CheckpointSystem::Update(float dt)
    {
        // Checkpoints do not need continuous updates
    }

    void CheckpointSystem::OnGameplayEvent(const GameplayEvent& event)
    {
        // Ignore checkpoint activation in Edit Mode
        bool isPlay = (m_Context->mode == RuntimeMode::Game) ||
                      (m_Context->mode == RuntimeMode::Editor && m_Context->editorSimulationState == EditorSimulationState::Play);
        if (!isPlay) return;

        if (event.Type != GameplayEventType::TriggerEnter) return;

        Entity triggerEntity = event.Target;
        bool activated = false;

        if (triggerEntity != INVALID_ENTITY && m_Context && m_Context->ecs)
        {
            auto& coordinator = m_Context->ecs->getCoordinator();
            if (coordinator.IsEntityAlive(triggerEntity))
            {
                auto sig = coordinator.GetSignature(triggerEntity);
                auto cpType = coordinator.GetComponentType<CheckpointComponent>();

                if (sig.test(cpType))
                {
                    auto& cp = coordinator.GetComponent<CheckpointComponent>(triggerEntity);
                    if (cp.ActivateOnTriggerEnter)
                    {
                        ActivateCheckpoint(triggerEntity);
                        activated = true;
                    }
                }
            }
        }

        // Fallback for legacy name-based checkpoint triggers
        if (!activated)
        {
            std::string triggerName = event.ObjectiveID;
            std::string cpID;

            if (triggerName.rfind("Checkpoint_", 0) == 0)
            {
                cpID = triggerName.substr(11);
            }
            else if (triggerName.rfind("CP_", 0) == 0)
            {
                cpID = triggerName;
            }
            else if (triggerName == "Checkpoint")
            {
                cpID = "CP_DEFAULT";
            }

            if (!cpID.empty())
            {
                LOG_INFO("[CheckpointSystem] Legacy/Fallback trigger '%s' resolved to ID '%s'", triggerName.c_str(), cpID.c_str());
                m_CurrentSnapshot.CheckpointID = cpID;
                m_CurrentSnapshot.CheckpointName = triggerName;

                if (m_Context && m_Context->ecs && m_Context->gameMode)
                {
                    Entity player = m_Context->gameMode->FindPlayerEntity();
                    if (player != INVALID_ENTITY)
                    {
                        auto& coordinator = m_Context->ecs->getCoordinator();
                        auto transformType = coordinator.GetComponentType<TransformComponent>();
                        if (coordinator.GetSignature(player).test(transformType))
                        {
                            m_CurrentSnapshot.PlayerTransform = coordinator.GetComponent<TransformComponent>(player);
                        }
                        else
                        {
                            m_CurrentSnapshot.PlayerTransform = TransformComponent();
                        }
                        m_CurrentSnapshot.Valid = true;
                    }
                }
                
                m_LastCheckpointEvent = "CheckpointReached -> " + cpID;

                if (m_Context->gameMode)
                {
                    m_Context->gameMode->GetGameStateMutable().CurrentCheckpointID = cpID;
                }

                if (m_Context->gameplayEventBus)
                {
                    GameplayEvent cpEvent;
                    cpEvent.Type = GameplayEventType::CheckpointReached;
                    cpEvent.Source = event.Source;
                    cpEvent.Target = event.Target;
                    cpEvent.CheckpointID = cpID;
                    m_Context->gameplayEventBus->QueueEvent(cpEvent);
                }
            }
        }
    }

    void CheckpointSystem::ActivateCheckpoint(uint32_t checkpointEntity)
    {
        if (!m_Context || !m_Context->ecs) return;

        auto& coordinator = m_Context->ecs->getCoordinator();
        auto& cp = coordinator.GetComponent<CheckpointComponent>(checkpointEntity);

        if (cp.OneShot && cp.HasActivated)
        {
            return;
        }

        if (cp.CheckpointID.empty())
        {
            LOG_WARN("[CheckpointSystem] Checkpoint trigger on entity %u ignored: empty CheckpointID", checkpointEntity);
            return;
        }

        LOG_INFO("[CheckpointSystem] Activating checkpoint '%s' (%s)", cp.CheckpointID.c_str(), cp.CheckpointName.c_str());

        CaptureSnapshot(cp.CheckpointID, cp.CheckpointName);

        cp.HasActivated = true;
        m_LastCheckpointEvent = "CheckpointReached -> " + cp.CheckpointID;

        // Emit CheckpointReached event
        if (m_Context->gameplayEventBus)
        {
            GameplayEvent cpEvent;
            cpEvent.Type = GameplayEventType::CheckpointReached;
            cpEvent.Source = checkpointEntity;
            cpEvent.CheckpointID = cp.CheckpointID;
            m_Context->gameplayEventBus->QueueEvent(cpEvent);
        }
    }

    void CheckpointSystem::CaptureSnapshot(const std::string& cpID, const std::string& cpName)
    {
        if (!m_Context || !m_Context->ecs || !m_Context->gameMode) return;

        auto& coordinator = m_Context->ecs->getCoordinator();
        Entity player = m_Context->gameMode->FindPlayerEntity();

        m_CurrentSnapshot.CheckpointID = cpID;
        m_CurrentSnapshot.CheckpointName = cpName;

        if (player != INVALID_ENTITY)
        {
            auto transformType = coordinator.GetComponentType<TransformComponent>();
            if (coordinator.GetSignature(player).test(transformType))
            {
                m_CurrentSnapshot.PlayerTransform = coordinator.GetComponent<TransformComponent>(player);
            }
            else
            {
                m_CurrentSnapshot.PlayerTransform = TransformComponent();
            }
        }

        const auto& gs = m_Context->gameMode->GetGameState();
        m_CurrentSnapshot.ActiveObjectiveID = gs.ActiveObjectiveID;
        m_CurrentSnapshot.CompletedObjectives = gs.CompletedObjectives;
        m_CurrentSnapshot.ElapsedGameplayTime = gs.ElapsedGameplayTime;

        // Capture simple state objects by activation ID
        m_CurrentSnapshot.SimpleObjectStates.clear();
        auto stateType = coordinator.GetComponentType<SimpleStateComponent>();
        auto activatableType = coordinator.GetComponentType<ActivatableComponent>();

        for (Entity ent : coordinator.GetActiveEntities())
        {
            if (ent != INVALID_ENTITY && coordinator.IsEntityAlive(ent))
            {
                auto sig = coordinator.GetSignature(ent);
                if (sig.test(stateType) && sig.test(activatableType))
                {
                    const auto& act = coordinator.GetComponent<ActivatableComponent>(ent);
                    if (!act.ActivationID.empty())
                    {
                        const auto& state = coordinator.GetComponent<SimpleStateComponent>(ent);
                        m_CurrentSnapshot.SimpleObjectStates[act.ActivationID] = state.CurrentState;
                    }
                }
            }
        }

        m_CurrentSnapshot.Valid = true;
    }

    void CheckpointSystem::RestoreLatestCheckpoint()
    {
        if (!m_CurrentSnapshot.Valid || !m_Context || !m_Context->ecs || !m_Context->gameMode) return;

        auto& coordinator = m_Context->ecs->getCoordinator();
        Entity player = m_Context->gameMode->FindPlayerEntity();

        if (player != INVALID_ENTITY)
        {
            // Restore transform
            auto transformType = coordinator.GetComponentType<TransformComponent>();
            if (coordinator.GetSignature(player).test(transformType))
            {
                coordinator.GetComponent<TransformComponent>(player) = m_CurrentSnapshot.PlayerTransform;
            }

            // Reset player health/alive state
            auto pscType = coordinator.GetComponentType<PlayerStateComponent>();
            if (coordinator.GetSignature(player).test(pscType))
            {
                auto& psc = coordinator.GetComponent<PlayerStateComponent>(player);
                psc.Health = psc.MaxHealth;
                psc.IsAlive = true;
                psc.CurrentInteractionTarget = INVALID_ENTITY;
            }
        }

        // Restore GameState mutable structures
        auto& gs = m_Context->gameMode->GetGameStateMutable();
        gs.ActiveObjectiveID = m_CurrentSnapshot.ActiveObjectiveID;
        gs.CompletedObjectives = m_CurrentSnapshot.CompletedObjectives;
        gs.CurrentCheckpointID = m_CurrentSnapshot.CheckpointID;
        gs.ElapsedGameplayTime = m_CurrentSnapshot.ElapsedGameplayTime;
        gs.SessionState = GameSessionState::Playing;

        // Restore Objective System state
        auto* objSys = m_Context->gameMode->GetObjectiveSystem();
        if (objSys)
        {
            objSys->RestoreObjectiveState(m_CurrentSnapshot.ActiveObjectiveID, m_CurrentSnapshot.CompletedObjectives);
        }

        // Restore Simple State Objects
        RestoreSimpleStateObjects(m_CurrentSnapshot);
    }

    void CheckpointSystem::RestoreSimpleStateObjects(const CheckpointSnapshot& snapshot)
    {
        if (!m_Context || !m_Context->ecs) return;

        auto& coordinator = m_Context->ecs->getCoordinator();
        auto stateType = coordinator.GetComponentType<SimpleStateComponent>();
        auto activatableType = coordinator.GetComponentType<ActivatableComponent>();
        auto doorType = coordinator.GetComponentType<DoorComponent>();
        auto transformType = coordinator.GetComponentType<TransformComponent>();

        for (Entity ent : coordinator.GetActiveEntities())
        {
            if (ent != INVALID_ENTITY && coordinator.IsEntityAlive(ent))
            {
                auto sig = coordinator.GetSignature(ent);
                if (sig.test(stateType) && sig.test(activatableType))
                {
                    const auto& act = coordinator.GetComponent<ActivatableComponent>(ent);
                    auto it = snapshot.SimpleObjectStates.find(act.ActivationID);
                    if (it != snapshot.SimpleObjectStates.end())
                    {
                        auto& state = coordinator.GetComponent<SimpleStateComponent>(ent);
                        state.CurrentState = it->second;

                        // If it is also a door, restore its transform position and open flags
                        if (sig.test(doorType) && sig.test(transformType))
                        {
                            auto& door = coordinator.GetComponent<DoorComponent>(ent);
                            auto& transform = coordinator.GetComponent<TransformComponent>(ent);

                            if (state.CurrentState == SimpleObjectState::Completed)
                            {
                                transform.position = door.ClosedPosition + door.OpenOffset;
                                door.IsOpen = true;
                                door.IsOpening = false;
                            }
                            else
                            {
                                transform.position = door.ClosedPosition;
                                door.IsOpen = false;
                                door.IsOpening = false;
                            }
                        }
                    }
                }
            }
        }
    }

} // namespace eng::runtime
