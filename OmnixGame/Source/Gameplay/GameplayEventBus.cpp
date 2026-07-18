#include "Gameplay/GameplayEventBus.h"
#include "EventManagement/EventManager.h"
#include "EventManagement/PhysicsEventTypes.h"
#include <algorithm>

namespace eng::runtime {

    GameplayEventBus::GameplayEventBus(Omnix::EventManager* underlyingBus)
        : m_UnderlyingBus(underlyingBus)
    {
    }

    GameplayEventBus::~GameplayEventBus()
    {
        ClearQueue();
    }

    void GameplayEventBus::Subscribe(GameplayEventType type, Handler handler)
    {
        if (handler)
        {
            m_Handlers[type].push_back(std::move(handler));
        }
    }

    void GameplayEventBus::Publish(const GameplayEvent& event)
    {
        // 1. Notify local subscribers
        auto it = m_Handlers.find(event.Type);
        if (it != m_Handlers.end())
        {
            for (const auto& handler : it->second)
            {
                if (handler)
                {
                    handler(event);
                }
            }
        }

        // 2. Wrap and forward to legacy EventManager if available
        if (m_UnderlyingBus)
        {
            std::string eventName = ToString(event.Type);
            // Prefix to distinguish local bus events
            std::string wrapperName = "GameplayBus_" + eventName;
            
            auto legacyEvent = std::make_unique<Omnix::GameplayEvent>(
                wrapperName, 
                static_cast<uint32_t>(event.Source), 
                static_cast<uint32_t>(event.Target)
            );
            m_UnderlyingBus->queueEvent(std::move(legacyEvent));
        }
    }

    void GameplayEventBus::QueueEvent(GameplayEvent event)
    {
        event.SequenceID = ++m_NextSequenceID;
        m_EventQueue.push_back(std::move(event));
    }

    void GameplayEventBus::FlushEvents()
    {
        m_FrameEventCount = 0;
        
        // Process events in a loop to handle cascading events (events queued during processing)
        while (!m_EventQueue.empty())
        {
            std::vector<GameplayEvent> eventsToProcess = std::move(m_EventQueue);
            m_EventQueue.clear();

            // Deterministically sort by SequenceID
            std::stable_sort(eventsToProcess.begin(), eventsToProcess.end(), 
                [](const GameplayEvent& a, const GameplayEvent& b) {
                    return a.SequenceID < b.SequenceID;
                }
            );

            for (const auto& event : eventsToProcess)
            {
                Publish(event);
                m_FrameEventCount++;
                m_TotalEventCount++;
                m_LastEvent = event;
            }
        }
    }

    void GameplayEventBus::ClearQueue()
    {
        m_EventQueue.clear();
        m_NextSequenceID = 0;
    }

    void GameplayEventBus::ClearHandlers()
    {
        m_Handlers.clear();
    }

} // namespace eng::runtime
