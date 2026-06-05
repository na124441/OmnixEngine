#include "Runtime/Public/Gameplay/CheckpointSystem.h"
#include "Runtime/Public/Gameplay/GameplayEventBus.h"
#include "Runtime/Public/RuntimeContext.h"
#include "Core/Logging/Logger.h"

namespace eng::runtime {

    void CheckpointSystem::Initialize(RuntimeContext* context)
    {
        m_Context = context;
        if (!m_Context || !m_Context->gameplayEventBus)
        {
            return;
        }

        m_Context->gameplayEventBus->Subscribe(GameplayEventType::TriggerEnter, [this](const GameplayEvent& event) {
            HandleTriggerEnter(event);
        });
    }

    void CheckpointSystem::HandleTriggerEnter(const GameplayEvent& event)
    {
        std::string triggerName = event.ObjectiveID;
        std::string cpID;

        // Parse checkpoint name (e.g. Checkpoint_CP_001 or CP_001)
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
            LOG_INFO("[CheckpointSystem] Detected checkpoint trigger '%s', resolved ID to '%s'", triggerName.c_str(), cpID.c_str());
            
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

} // namespace eng::runtime
