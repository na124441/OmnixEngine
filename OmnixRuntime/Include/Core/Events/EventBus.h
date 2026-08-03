#pragma once

#include <unordered_map>
#include <typeindex>
#include <vector>
#include <functional>
#include <mutex>
#include <memory>

namespace eng::core {

    class IEventBusHandler {
    public:
        virtual ~IEventBusHandler() = default;
    };

    template<typename EventType>
    class EventBusHandler : public IEventBusHandler {
    public:
        using Callback = std::function<void(const EventType&)>;
        
        void AddCallback(Callback cb) {
            m_Callbacks.push_back(cb);
        }
        
        const std::vector<Callback>& GetCallbacks() const {
            return m_Callbacks;
        }

    private:
        std::vector<Callback> m_Callbacks;
    };

    /**
     * @class EventBus
     * @brief A generic type-safe event bus supporting synchronous immediate publish and deferred frame processing (T1.1.12, T1.1.13).
     */
    class EventBus {
    public:
        EventBus() = default;
        ~EventBus() = default;

        EventBus(const EventBus&) = delete;
        EventBus& operator=(const EventBus&) = delete;

        /**
         * @brief Subscribe a callback to be notified of events of type EventType.
         */
        template<typename EventType>
        void Subscribe(std::function<void(const EventType&)> callback) {
            std::lock_guard<std::mutex> lock(m_Mutex);
            auto& handler = m_Handlers[typeid(EventType)];
            if (!handler) {
                handler = std::make_unique<EventBusHandler<EventType>>();
            }
            static_cast<EventBusHandler<EventType>*>(handler.get())->AddCallback(callback);
        }

        /**
         * @brief Synchronously publish an event immediately to all subscribers (T1.1.12).
         */
        template<typename EventType>
        void PublishImmediate(const EventType& event) {
            std::vector<std::function<void(const EventType&)>> callbacks;
            {
                std::lock_guard<std::mutex> lock(m_Mutex);
                auto it = m_Handlers.find(typeid(EventType));
                if (it != m_Handlers.end() && it->second) {
                    callbacks = static_cast<EventBusHandler<EventType>*>(it->second.get())->GetCallbacks();
                }
            }
            for (const auto& cb : callbacks) {
                try {
                    cb(event);
                } catch (...) {}
            }
        }

        /**
         * @brief Defer an event, queueing it for dispatch during the next ProcessQueue frame call (T1.1.13).
         */
        template<typename EventType>
        void PublishDeferred(const EventType& event) {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_EventQueue.push_back([this, event]() {
                PublishImmediate<EventType>(event);
            });
        }

        /**
         * @brief Dispatches all deferred events in queue.
         */
        void ProcessQueue() {
            std::vector<std::function<void()>> localQueue;
            {
                std::lock_guard<std::mutex> lock(m_Mutex);
                localQueue = std::move(m_EventQueue);
                m_EventQueue.clear();
            }
            for (const auto& dispatch : localQueue) {
                try {
                    dispatch();
                } catch (...) {}
            }
        }

        /**
         * @brief Clears all subscribers and queues.
         */
        void Clear() {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_Handlers.clear();
            m_EventQueue.clear();
        }

    private:
        std::mutex m_Mutex;
        std::unordered_map<std::type_index, std::unique_ptr<IEventBusHandler>> m_Handlers;
        std::vector<std::function<void()>> m_EventQueue;
    };

} // namespace eng::core
