#pragma once

#include "Runtime/Public/Gameplay/GameplayEvent.h"
#include <functional>
#include <vector>
#include <unordered_map>

namespace Omnix {
    class EventManager;
}

namespace eng::runtime {

    class GameplayEventBus
    {
    public:
        using Handler = std::function<void(const GameplayEvent&)>;

        GameplayEventBus(Omnix::EventManager* underlyingBus = nullptr);
        ~GameplayEventBus();

        // Prevent copy/move
        GameplayEventBus(const GameplayEventBus&) = delete;
        GameplayEventBus& operator=(const GameplayEventBus&) = delete;

        // Pub/Sub API
        void Subscribe(GameplayEventType type, Handler handler);
        void Publish(const GameplayEvent& event);
        void ClearHandlers();

        // Queue API
        void QueueEvent(GameplayEvent event);
        void FlushEvents();
        void ClearQueue();

        // Diagnostics
        uint32_t GetFrameEventCount() const { return m_FrameEventCount; }
        uint64_t GetTotalEventCount() const { return m_TotalEventCount; }
        const GameplayEvent& GetLastEvent() const { return m_LastEvent; }

    private:
        Omnix::EventManager* m_UnderlyingBus = nullptr;
        std::unordered_map<GameplayEventType, std::vector<Handler>> m_Handlers;
        std::vector<GameplayEvent> m_EventQueue;
        uint64_t m_NextSequenceID = 0;

        // Diagnostics metrics
        uint32_t m_FrameEventCount = 0;
        uint64_t m_TotalEventCount = 0;
        GameplayEvent m_LastEvent;
    };

} // namespace eng::runtime
