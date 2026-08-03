#pragma once

#include "Gameplay/Save/GameplaySaveSnapshot.h"
#include "Gameplay/Save/GameplaySaveHeader.h"
#include <string>

#include "Runtime/RuntimeContext.h"

namespace eng::runtime {

    class GameplaySaveSystem
    {
    public:
        GameplaySaveSystem();
        ~GameplaySaveSystem();

        void Initialize(RuntimeContext* context);

        GameplaySaveSnapshot CaptureSnapshot();

        bool SaveToFile(const std::string& path, const GameplaySaveSnapshot& snapshot);
        bool LoadFromFile(const std::string& path, GameplaySaveSnapshot& outSnapshot);

        bool RestoreSnapshot(const GameplaySaveSnapshot& snapshot);

        uint64_t ComputeChecksum(const GameplaySaveSnapshot& snapshot);

        // Save diagnostics getters
        const std::string& GetLastSavePath() const { return m_LastSavePath; }
        bool IsLastSaveValid() const { return m_LastSaveValid; }
        uint32_t GetLastSaveVersion() const { return m_LastSaveVersion; }
        const std::string& GetLastSaveScene() const { return m_LastSaveScene; }
        uint64_t GetLastSaveChecksum() const { return m_LastSaveChecksum; }
        uint64_t GetLastSavePayloadSize() const { return m_LastSavePayloadSize; }
        const TransformComponent& GetLastSavePlayerTransform() const { return m_LastSavePlayerTransform; }
        float GetLastSavePlayerHealth() const { return m_LastSavePlayerHealth; }
        bool IsLastSavePlayerAlive() const { return m_LastSavePlayerAlive; }
        const std::string& GetLastSaveActiveObjectiveID() const { return m_LastSaveActiveObjectiveID; }
        size_t GetLastSaveCompletedObjectivesCount() const { return m_LastSaveCompletedObjectivesCount; }
        const std::string& GetLastSaveCheckpointID() const { return m_LastSaveCheckpointID; }
        size_t GetLastSaveInteractableStatesCount() const { return m_LastSaveInteractableStatesCount; }
        size_t GetLastSaveSimpleObjectStatesCount() const { return m_LastSaveSimpleObjectStatesCount; }

        // Restore diagnostics getters
        const std::string& GetLastRestoreResult() const { return m_LastRestoreResult; }
        const std::string& GetLastRestoreSourceSave() const { return m_LastRestoreSourceSave; }
        bool IsLastRestoreSceneMatch() const { return m_LastRestoreSceneMatch; }
        bool IsLastRestoreChecksumValid() const { return m_LastRestoreChecksumValid; }
        size_t GetLastRestoreObjectsCount() const { return m_LastRestoreObjectsCount; }
        size_t GetLastRestoreInteractablesCount() const { return m_LastRestoreInteractablesCount; }
        size_t GetLastRestoreWarnings() const { return m_LastRestoreWarnings; }

    private:
        RuntimeContext* m_Context = nullptr;

        // Save diagnostics state
        std::string m_LastSavePath = "None";
        bool m_LastSaveValid = false;
        uint32_t m_LastSaveVersion = 0;
        std::string m_LastSaveScene = "None";
        uint64_t m_LastSaveChecksum = 0;
        uint64_t m_LastSavePayloadSize = 0;
        TransformComponent m_LastSavePlayerTransform;
        float m_LastSavePlayerHealth = 0.0f;
        bool m_LastSavePlayerAlive = false;
        std::string m_LastSaveActiveObjectiveID = "None";
        size_t m_LastSaveCompletedObjectivesCount = 0;
        std::string m_LastSaveCheckpointID = "None";
        size_t m_LastSaveInteractableStatesCount = 0;
        size_t m_LastSaveSimpleObjectStatesCount = 0;

        // Restore diagnostics state
        std::string m_LastRestoreResult = "None";
        std::string m_LastRestoreSourceSave = "None";
        bool m_LastRestoreSceneMatch = false;
        bool m_LastRestoreChecksumValid = false;
        size_t m_LastRestoreObjectsCount = 0;
        size_t m_LastRestoreInteractablesCount = 0;
        size_t m_LastRestoreWarnings = 0;
    };

} // namespace eng::runtime
