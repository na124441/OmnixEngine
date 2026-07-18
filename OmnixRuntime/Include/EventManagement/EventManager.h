// ============================================================================
// EventManager.h - Central Event Dispatcher & Registry
// ============================================================================

#pragma once

#include "EventManagement/GameEvent.h"
#include "EventManagement/EventQueue.h"
#include <map>
#include <vector>
#include <functional>
#include <mutex>
#include <iostream>

namespace Omnix {

// ============================================================================
// EVENT MANAGER CLASS
// ============================================================================

class EventManager {
public:
    using ListenerCallback = std::function<void(const GameEvent*)>;

    EventManager() : nextListenerID(1) {}

    ~EventManager() {
        listeners.clear();
        eventQueue.clear();
    }

    EventManager(const EventManager&) = delete;
    EventManager& operator=(const EventManager&) = delete;

    // ────────────────────────────────────────────────────────────────────
    // Register/Unregister Listeners
    // ────────────────────────────────────────────────────────────────────

    ListenerHandle registerListener(EventType type, ListenerCallback callback) {
        if (!callback) {
            return {0};
        }

        std::lock_guard<std::mutex> lock(listenersMutex);
        ListenerHandle handle{nextListenerID++};
        auto& listenerList = listeners[static_cast<int>(type)];
        listenerList.emplace_back(handle.id, std::move(callback));
        return handle;
    }

    void unregisterListener(EventType type, const ListenerHandle& handle) {
        if (handle.id == 0) {
            return;
        }

        std::lock_guard<std::mutex> lock(listenersMutex);
        int typeKey = static_cast<int>(type);
        auto it = listeners.find(typeKey);
        if (it != listeners.end()) {
            auto& listenerList = it->second;
            listenerList.erase(
                std::remove_if(
                    listenerList.begin(), listenerList.end(),
                    [handle](const std::pair<uint32_t, ListenerCallback>& entry) {
                        return entry.first == handle.id;
                    }
                ),
                listenerList.end()
            );

            if (listenerList.empty()) {
                listeners.erase(it);
            }
        }
    }

    void unregisterAllListeners(EventType type) {
        std::lock_guard<std::mutex> lock(listenersMutex);
        listeners.erase(static_cast<int>(type));
    }

    size_t getListenerCount(EventType type) const {
        std::lock_guard<std::mutex> lock(listenersMutex);
        auto it = listeners.find(static_cast<int>(type));
        return (it != listeners.end()) ? it->second.size() : 0;
    }

    // ────────────────────────────────────────────────────────────────────
    // Event Queueing & Processing
    // ────────────────────────────────────────────────────────────────────

    void queueEvent(GameEvent::EventPtr event) {
        if (!event) {
            return;
        }
        eventQueue.push(std::move(event));
    }

    void dispatchImmediate(const GameEvent* event) {
        if (!event) {
            return;
        }
        dispatchToListeners(event);
    }

    size_t processQueue() {
        auto events = eventQueue.drainAll();
        size_t count = 0;
        for (auto& eventPtr : events) {
            if (eventPtr) {
                dispatchToListeners(eventPtr.get());
                count++;
            }
        }
        return count;
    }

    size_t processQueueWithBudget(uint64_t maxTimeNs) {
        auto startTime = std::chrono::high_resolution_clock::now();
        size_t eventCount = 0;
        while (!eventQueue.isEmpty()) {
            auto currentTime = std::chrono::high_resolution_clock::now();
            uint64_t elapsedNs =
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    currentTime - startTime
                ).count();

            if (elapsedNs > maxTimeNs) {
                break;
            }

            auto event = eventQueue.pop();
            if (event) {
                dispatchToListeners(event.get());
                eventCount++;
            }
        }
        return eventCount;
    }

    size_t getQueueSize() const {
        return eventQueue.size();
    }

private:
    // ────────────────────────────────────────────────────────────────────
    // Internal: Dispatch to Listeners
    // ────────────────────────────────────────────────────────────────────

    void dispatchToListeners(const GameEvent* event) {
        if (!event) {
            return;
        }

        int typeKey = static_cast<int>(event->getType());

        std::vector<ListenerCallback> callbacks;

        {
            std::lock_guard<std::mutex> lock(listenersMutex);
            auto it = listeners.find(typeKey);
            if (it == listeners.end()) {
                return;
            }

            callbacks.reserve(it->second.size());
            for (const auto& [id, callback] : it->second) {
                callbacks.push_back(callback);
            }
        }

        // Dispatch without holding lock
        for (const auto& callback : callbacks) {
            try {
                callback(event);
                if (event->isConsumed()) {
                    break;
                }
            } catch (const std::exception& e) {
                std::cerr << "Exception in event listener: " << e.what() << "\n";
            } catch (...) {
                std::cerr << "Unknown exception in event listener\n";
            }
        }
    }

    // ────────────────────────────────────────────────────────────────────
    // Private Data Members
    // ────────────────────────────────────────────────────────────────────

    using ListenerList = std::vector<std::pair<uint32_t, ListenerCallback>>;

    EventQueue eventQueue;
    std::map<int, ListenerList> listeners;
    mutable std::mutex listenersMutex;
    uint32_t nextListenerID;
};

} // namespace Omnix
