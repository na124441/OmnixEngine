#pragma once

#include "Runtime/Public/Gameplay/Objectives/Objective.h"
#include "Runtime/Public/Gameplay/Components/ObjectiveComponent.h"
#include "Runtime/Public/Gameplay/GameplayEvent.h"
#include <string>
#include <unordered_map>
#include <memory>

namespace eng::runtime {

    struct RuntimeContext;

    class ObjectiveSystem
    {
    public:
        ObjectiveSystem() = default;
        ~ObjectiveSystem() = default;

        void Initialize(RuntimeContext* context);
        void OnLevelStart();
        void OnLevelRestart();
        void Update(float dt);

        void StartObjective(const std::string& objectiveID);
        void CompleteObjective(const std::string& objectiveID);

        // Accessors for diagnostics/tests
        const std::unordered_map<std::string, Objective>& GetObjectives() const { return m_Objectives; }
        const Objective* GetObjective(const std::string& id) const;
        size_t GetCompletedCount() const { return m_CompletedCount; }
        size_t GetObjectiveEventCount() const { return m_ObjectiveEventCount; }
        
        std::string GetLastEventName() const { return m_LastEventName; }
        std::string GetLastEventObjectiveID() const { return m_LastEventObjectiveID; }

    private:
        void OnGameplayEvent(const GameplayEvent& event);
        void TryCompleteObjectiveFromInteraction(Entity target);
        void TryCompleteObjectiveFromTrigger(const std::string& eventName, Entity target);

        RuntimeContext* m_Context = nullptr;
        std::unordered_map<std::string, Objective> m_Objectives;
        size_t m_CompletedCount = 0;
        size_t m_ObjectiveEventCount = 0;
        
        std::string m_LastEventName = "None";
        std::string m_LastEventObjectiveID = "None";
    };

} // namespace eng::runtime
