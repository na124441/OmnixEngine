#pragma once

#include "Runtime/Public/Gameplay/GameSessionState.h"
#include "Runtime/Public/Gameplay/GameState.h"
#include <memory>

// Forward declare RuntimeContext, Entity, and systems
namespace eng::runtime {
    struct RuntimeContext;
    class ObjectiveSystem;
    class CheckpointSystem;
    class GameplayHUD;
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

        GameSessionState GetState() const;
        const GameState& GetGameState() const { return m_GameState; }
        GameState& GetGameStateMutable() { return m_GameState; }

        ObjectiveSystem* GetObjectiveSystem() const { return m_ObjectiveSystem.get(); }
        GameplayHUD* GetGameplayHUD() const { return m_GameplayHUD.get(); }

        Entity FindPlayerEntity() const;

    protected:
        GameSessionState m_State = GameSessionState::None;
        GameState m_GameState;
        RuntimeContext* m_Context = nullptr;

        std::unique_ptr<ObjectiveSystem> m_ObjectiveSystem;
        std::unique_ptr<CheckpointSystem> m_CheckpointSystem;
        std::unique_ptr<GameplayHUD> m_GameplayHUD;
    };

} // namespace eng::runtime
