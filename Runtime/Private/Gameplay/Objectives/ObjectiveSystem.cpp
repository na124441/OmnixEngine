#include "Runtime/Public/Gameplay/Objectives/ObjectiveSystem.h"
#include "Runtime/Public/RuntimeContext.h"
#include "Runtime/Public/Gameplay/GameplayEventBus.h"
#include "Runtime/Public/Gameplay/GameMode.h"
#include "ECS/Coordinator.h"
#include "ECS/Public/IECSWorld.h"
#include "Core/Logging/Logger.h"

namespace eng::runtime {

    void ObjectiveSystem::Initialize(RuntimeContext* context)
    {
        m_Context = context;
        if (!m_Context || !m_Context->gameplayEventBus) return;

        m_Context->gameplayEventBus->Subscribe(GameplayEventType::Interaction, [this](const GameplayEvent& event) {
            OnGameplayEvent(event);
        });

        m_Context->gameplayEventBus->Subscribe(GameplayEventType::TriggerEnter, [this](const GameplayEvent& event) {
            OnGameplayEvent(event);
        });

        m_Context->gameplayEventBus->Subscribe(GameplayEventType::CheckpointReached, [this](const GameplayEvent& event) {
            OnGameplayEvent(event);
        });
    }

    void ObjectiveSystem::OnLevelStart()
    {
        m_Objectives.clear();
        m_CompletedCount = 0;
        m_LastEventName = "None";
        m_LastEventObjectiveID = "None";

        if (!m_Context || !m_Context->ecs) return;

        // Register objectives only in Play Mode
        bool shouldSimulate = (m_Context->mode == RuntimeMode::Game) ||
                              (m_Context->mode == RuntimeMode::Editor && m_Context->editorSimulationState == EditorSimulationState::Play);
        if (!shouldSimulate) return;

        auto& coordinator = m_Context->ecs->getCoordinator();
        auto objectiveCompType = coordinator.GetComponentType<ObjectiveComponent>();

        for (Entity entity : coordinator.GetActiveEntities())
        {
            if (entity == INVALID_ENTITY || !coordinator.IsEntityAlive(entity)) continue;

            auto sig = coordinator.GetSignature(entity);
            if (sig.test(objectiveCompType))
            {
                const auto& component = coordinator.GetComponent<ObjectiveComponent>(entity);

                if (component.ObjectiveID.empty())
                {
                    LOG_ERROR("[ObjectiveSystem] Rejected objective registration: Empty Objective ID on Entity %u", entity);
                    continue;
                }

                if (m_Objectives.find(component.ObjectiveID) != m_Objectives.end())
                {
                    LOG_WARN("[ObjectiveSystem] Duplicate Objective ID '%s' detected! Overwriting previous registration.", component.ObjectiveID.c_str());
                }

                Objective objective;
                objective.ID = component.ObjectiveID;
                objective.Title = component.Title;
                objective.Description = component.Description;
                objective.Repeatable = component.Repeatable;
                objective.State = component.StartsActive ? ObjectiveState::Active : ObjectiveState::Inactive;
                
                if (component.Completed)
                {
                    objective.State = ObjectiveState::Completed;
                    m_CompletedCount++;
                }

                m_Objectives[objective.ID] = objective;

                if (objective.State == ObjectiveState::Active)
                {
                    StartObjective(objective.ID);
                }
            }
        }
    }

    void ObjectiveSystem::OnLevelRestart()
    {
        OnLevelStart();
    }

    void ObjectiveSystem::RestoreObjectiveState(const std::string& activeID, const std::vector<std::string>& completedIDs)
    {
        m_CompletedCount = 0;
        for (auto& [id, obj] : m_Objectives)
        {
            if (std::find(completedIDs.begin(), completedIDs.end(), id) != completedIDs.end())
            {
                obj.State = ObjectiveState::Completed;
                m_CompletedCount++;
            }
            else if (id == activeID)
            {
                obj.State = ObjectiveState::Active;
            }
            else
            {
                obj.State = ObjectiveState::Inactive;
            }
        }
    }

    void ObjectiveSystem::Update(float dt)
    {
        // Static objectives do not require per-frame updates, but method is required by the plan interface
    }

    void ObjectiveSystem::StartObjective(const std::string& objectiveID)
    {
        auto it = m_Objectives.find(objectiveID);
        if (it == m_Objectives.end())
        {
            Objective newObj;
            newObj.ID = objectiveID;
            newObj.Title = objectiveID;
            newObj.Description = "";
            newObj.Repeatable = false;
            newObj.State = ObjectiveState::Inactive;
            it = m_Objectives.emplace_hint(it, objectiveID, std::move(newObj));
        }

        bool shouldSimulate = (m_Context->mode == RuntimeMode::Game) ||
                              (m_Context->mode == RuntimeMode::Editor && m_Context->editorSimulationState == EditorSimulationState::Play);
        if (!shouldSimulate) return;

        it->second.State = ObjectiveState::Active;

        if (m_Context->gameMode)
        {
            auto& gs = m_Context->gameMode->GetGameStateMutable();
            gs.ActiveObjectiveID = objectiveID;
        }

        m_LastEventName = "ObjectiveStarted";
        m_LastEventObjectiveID = objectiveID;

        if (m_Context->gameplayEventBus)
        {
            GameplayEvent startEvent;
            startEvent.Type = GameplayEventType::ObjectiveStarted;
            startEvent.ObjectiveID = objectiveID;
            m_Context->gameplayEventBus->QueueEvent(startEvent);
            m_ObjectiveEventCount++;
        }

        LOG_INFO("[ObjectiveSystem] Started objective '%s'", objectiveID.c_str());
    }

    void ObjectiveSystem::CompleteObjective(const std::string& objectiveID)
    {
        bool shouldSimulate = (m_Context->mode == RuntimeMode::Game) ||
                              (m_Context->mode == RuntimeMode::Editor && m_Context->editorSimulationState == EditorSimulationState::Play);
        if (!shouldSimulate) return;

        auto it = m_Objectives.find(objectiveID);
        if (it == m_Objectives.end())
        {
            Objective newObj;
            newObj.ID = objectiveID;
            newObj.Title = objectiveID;
            newObj.Description = "";
            newObj.Repeatable = false;
            newObj.State = ObjectiveState::Active; // Needs to be active to complete
            it = m_Objectives.emplace_hint(it, objectiveID, std::move(newObj));
        }

        Objective& objective = it->second;

        // Prevent inactive objective completion
        if (objective.State == ObjectiveState::Inactive)
        {
            LOG_WARN("[ObjectiveSystem] Cannot complete objective '%s': Objective is Inactive!", objectiveID.c_str());
            return;
        }

        // Prevent duplicate completion unless repeatable
        if (objective.State == ObjectiveState::Completed && !objective.Repeatable)
        {
            LOG_WARN("[ObjectiveSystem] Ignored completion attempt for non-repeatable completed objective '%s'", objectiveID.c_str());
            return;
        }

        objective.State = ObjectiveState::Completed;
        m_CompletedCount++;

        if (m_Context->gameMode)
        {
            auto& gs = m_Context->gameMode->GetGameStateMutable();
            auto compIt = std::find(gs.CompletedObjectives.begin(), gs.CompletedObjectives.end(), objectiveID);
            if (compIt == gs.CompletedObjectives.end())
            {
                gs.CompletedObjectives.push_back(objectiveID);
            }

            if (gs.ActiveObjectiveID == objectiveID)
            {
                gs.ActiveObjectiveID = "";
            }
        }

        m_LastEventName = "ObjectiveCompleted";
        m_LastEventObjectiveID = objectiveID;

        if (m_Context->gameplayEventBus)
        {
            GameplayEvent compEvent;
            compEvent.Type = GameplayEventType::ObjectiveCompleted;
            compEvent.ObjectiveID = objectiveID;
            m_Context->gameplayEventBus->QueueEvent(compEvent);
            m_ObjectiveEventCount++;
        }

        LOG_INFO("[ObjectiveSystem] Completed objective '%s'", objectiveID.c_str());
    }

    void ObjectiveSystem::OnGameplayEvent(const GameplayEvent& event)
    {
        bool shouldSimulate = (m_Context->mode == RuntimeMode::Game) ||
                              (m_Context->mode == RuntimeMode::Editor && m_Context->editorSimulationState == EditorSimulationState::Play);
        if (!shouldSimulate) return;

        if (m_Objectives.empty())
        {
            if (!m_Context || !m_Context->gameMode) return;
            auto& gs = m_Context->gameMode->GetGameStateMutable();

            if (event.Type == GameplayEventType::TriggerEnter)
            {
                if (gs.ActiveObjectiveID == "OBJ_001" && 
                    (event.ObjectiveID == "OBJ_001_Trigger" || event.ObjectiveID == "TerminalTrigger"))
                {
                    LOG_INFO("[ObjectiveSystem] Legacy fallback: Trigger enter matched OBJ_001 requirements.");
                    gs.CompletedObjectives.push_back("OBJ_001");
                    gs.ActiveObjectiveID = "OBJ_002";
                    m_LastEventName = "ObjectiveCompleted";
                    m_LastEventObjectiveID = "OBJ_001";
                    
                    if (m_Context->gameplayEventBus)
                    {
                        GameplayEvent compEvent;
                        compEvent.Type = GameplayEventType::ObjectiveCompleted;
                        compEvent.ObjectiveID = "OBJ_001";
                        m_Context->gameplayEventBus->QueueEvent(compEvent);

                        GameplayEvent startEvent;
                        startEvent.Type = GameplayEventType::ObjectiveStarted;
                        startEvent.ObjectiveID = "OBJ_002";
                        m_Context->gameplayEventBus->QueueEvent(startEvent);
                    }
                }
            }
            else if (event.Type == GameplayEventType::Interaction)
            {
                if (gs.ActiveObjectiveID == "OBJ_002")
                {
                    LOG_INFO("[ObjectiveSystem] Legacy fallback: Interaction matched OBJ_002 requirements.");
                    gs.CompletedObjectives.push_back("OBJ_002");
                    gs.ActiveObjectiveID = "OBJ_003";
                    m_LastEventName = "ObjectiveCompleted";
                    m_LastEventObjectiveID = "OBJ_002";

                    if (m_Context->gameplayEventBus)
                    {
                        GameplayEvent compEvent;
                        compEvent.Type = GameplayEventType::ObjectiveCompleted;
                        compEvent.ObjectiveID = "OBJ_002";
                        m_Context->gameplayEventBus->QueueEvent(compEvent);

                        GameplayEvent startEvent;
                        startEvent.Type = GameplayEventType::ObjectiveStarted;
                        startEvent.ObjectiveID = "OBJ_003";
                        m_Context->gameplayEventBus->QueueEvent(startEvent);
                    }
                }
            }
            else if (event.Type == GameplayEventType::CheckpointReached)
            {
                if (gs.ActiveObjectiveID == "OBJ_003" && event.CheckpointID == "CP_001")
                {
                    LOG_INFO("[ObjectiveSystem] Legacy fallback: Checkpoint CP_001 matched OBJ_003 requirements.");
                    gs.CompletedObjectives.push_back("OBJ_003");
                    gs.ActiveObjectiveID = "";
                    m_LastEventName = "ObjectiveCompleted";
                    m_LastEventObjectiveID = "OBJ_003";

                    if (m_Context->gameplayEventBus)
                    {
                        GameplayEvent compEvent;
                        compEvent.Type = GameplayEventType::ObjectiveCompleted;
                        compEvent.ObjectiveID = "OBJ_003";
                        m_Context->gameplayEventBus->QueueEvent(compEvent);

                        GameplayEvent levelEvent;
                        levelEvent.Type = GameplayEventType::LevelCompleted;
                        levelEvent.Source = event.Source;
                        levelEvent.Target = event.Target;
                        m_Context->gameplayEventBus->QueueEvent(levelEvent);
                    }
                }
            }
            return;
        }

        if (event.Type == GameplayEventType::Interaction)
        {
            TryCompleteObjectiveFromInteraction(event.Target);
        }
        else if (event.Type == GameplayEventType::TriggerEnter)
        {
            TryCompleteObjectiveFromTrigger(event.ObjectiveID, event.Target);
        }
    }

    void ObjectiveSystem::TryCompleteObjectiveFromInteraction(Entity target)
    {
        if (target == INVALID_ENTITY || !m_Context || !m_Context->ecs) return;

        auto& coordinator = m_Context->ecs->getCoordinator();
        auto objectiveCompType = coordinator.GetComponentType<ObjectiveComponent>();

        if (coordinator.IsEntityAlive(target) && coordinator.GetSignature(target).test(objectiveCompType))
        {
            const auto& comp = coordinator.GetComponent<ObjectiveComponent>(target);
            if (comp.CompletionMode == ObjectiveCompletionMode::Interaction)
            {
                CompleteObjective(comp.ObjectiveID);
            }
        }
    }

    void ObjectiveSystem::TryCompleteObjectiveFromTrigger(const std::string& eventName, Entity target)
    {
        if (target == INVALID_ENTITY || !m_Context || !m_Context->ecs) return;

        auto& coordinator = m_Context->ecs->getCoordinator();
        auto objectiveCompType = coordinator.GetComponentType<ObjectiveComponent>();

        if (coordinator.IsEntityAlive(target) && coordinator.GetSignature(target).test(objectiveCompType))
        {
            const auto& comp = coordinator.GetComponent<ObjectiveComponent>(target);
            if (comp.CompletionMode == ObjectiveCompletionMode::TriggerEnter)
            {
                CompleteObjective(comp.ObjectiveID);
            }
        }
    }

    const Objective* ObjectiveSystem::GetObjective(const std::string& id) const
    {
        auto it = m_Objectives.find(id);
        if (it != m_Objectives.end())
        {
            return &it->second;
        }
        return nullptr;
    }

} // namespace eng::runtime
