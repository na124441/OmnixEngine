#pragma once

#include "Gameplay/GameSessionState.h"
#include "Gameplay/GameState.h"
#include <memory>

#include "Runtime/RuntimeContext.h"

// Forward declare Entity and systems
namespace eng::runtime {
    class ObjectiveSystem;
    class CheckpointSystem;
    class GameplayHUD;
    class ObjectActivationSystem;
    struct GameplaySaveSnapshot;
}
using Entity = std::uint32_t;

namespace eng::runtime {

    class GameMode
    {
    public:
        GameMode();
        virtual ~GameMode();

        virtual void OnLevelStart(RuntimeContext* context);
        virtual void OnLevelEnd();

        virtual void Tick(float dt);

        virtual void CompleteLevel();
        virtual void FailLevel();
        virtual void RestartLevel();

        virtual void PauseLevel();
        virtual void ResumeLevel();

        virtual bool RestoreFromSnapshot(const GameplaySaveSnapshot& snapshot);

        GameSessionState GetState() const;
        const GameState& GetGameState() const { return m_GameState; }
        GameState& GetGameStateMutable() { return m_GameState; }

        ObjectiveSystem* GetObjectiveSystem() const { return m_ObjectiveSystem.get(); }
        ObjectActivationSystem* GetObjectActivationSystem() const { return m_ObjectActivationSystem.get(); }
        CheckpointSystem* GetCheckpointSystem() const { return m_CheckpointSystem.get(); }
        GameplayHUD* GetGameplayHUD() const { return m_GameplayHUD.get(); }

        Entity FindPlayerEntity() const;

    protected:
        GameSessionState m_State = GameSessionState::None;
        GameState m_GameState;
        RuntimeContext* m_Context = nullptr;

        std::unique_ptr<ObjectiveSystem> m_ObjectiveSystem;
        std::unique_ptr<CheckpointSystem> m_CheckpointSystem;
        std::unique_ptr<GameplayHUD> m_GameplayHUD;
        std::unique_ptr<ObjectActivationSystem> m_ObjectActivationSystem;
    };

} // namespace eng::runtime
