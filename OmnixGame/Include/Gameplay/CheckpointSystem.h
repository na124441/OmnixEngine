#pragma once
#include "Gameplay/GameplayEvent.h"
#include "Gameplay/Checkpoints/CheckpointSnapshot.h"
#include <string>

#include "Runtime/RuntimeContext.h"

namespace eng::runtime {

    class CheckpointSystem
    {
    public:
        CheckpointSystem() = default;
        ~CheckpointSystem() = default;

        void Initialize(RuntimeContext* context);
        void OnPlayStart();
        void OnPlayStop();
        void Update(float dt);

        void OnGameplayEvent(const GameplayEvent& event);

        bool HasValidCheckpoint() const { return m_CurrentSnapshot.Valid; }
        void RestoreLatestCheckpoint();
        const CheckpointSnapshot& GetCurrentSnapshot() const { return m_CurrentSnapshot; }
        std::string GetLastCheckpointEvent() const { return m_LastCheckpointEvent; }

    private:
        void ActivateCheckpoint(uint32_t checkpointEntity);
        void CaptureSnapshot(const std::string& cpID, const std::string& cpName);
        void RestoreSimpleStateObjects(const CheckpointSnapshot& snapshot);

        RuntimeContext* m_Context = nullptr;
        CheckpointSnapshot m_CurrentSnapshot;
        std::string m_LastCheckpointEvent = "None";
    };
}
