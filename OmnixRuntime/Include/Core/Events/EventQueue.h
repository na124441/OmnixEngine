// ============================================================================
// EventQueue.h - Thread-Safe FIFO Event Buffer (HEADER-ONLY)
// ============================================================================

#pragma once

#include "Core/Events/GameEvent.h"
#include <queue>
#include <memory>
#include <mutex>
#include <vector>

namespace eng::core {

// ============================================================================
// EVENT QUEUE CLASS
// ============================================================================

class EventQueue {
public:
    using EventPtr = GameEvent::EventPtr;

    EventQueue() = default;
    ~EventQueue() = default;

    EventQueue(const EventQueue&) = delete;
    EventQueue& operator=(const EventQueue&) = delete;

    EventQueue(EventQueue&& other) noexcept
        : queue(std::move(other.queue)) {}

    EventQueue& operator=(EventQueue&& other) noexcept {
        if (this != &other) {
            queue = std::move(other.queue);
        }
        return *this;
    }

    void push(EventPtr event) {
        if (!event) return;
        std::lock_guard<std::mutex> lock(queueMutex);
        queue.push(std::move(event));
    }

    EventPtr pop() {
        std::lock_guard<std::mutex> lock(queueMutex);
        if (queue.empty()) {
            return nullptr;
        }
        EventPtr event = std::move(queue.front());
        queue.pop();
        return event;
    }

    std::vector<EventPtr> drainAll() {
        std::lock_guard<std::mutex> lock(queueMutex);
        std::vector<EventPtr> out;
        out.reserve(queue.size());
        while (!queue.empty()) {
            out.push_back(std::move(queue.front()));
            queue.pop();
        }
        return out;
    }

    GameEvent* peek() const {
        std::lock_guard<std::mutex> lock(queueMutex);
        if (queue.empty()) {
            return nullptr;
        }
        return queue.front().get();
    }

    bool isEmpty() const {
        std::lock_guard<std::mutex> lock(queueMutex);
        return queue.empty();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(queueMutex);
        return queue.size();
    }

    void clear() {
        std::lock_guard<std::mutex> lock(queueMutex);
        while (!queue.empty()) {
            queue.pop();
        }
    }

private:
    std::queue<EventPtr> queue;
    mutable std::mutex queueMutex;
};

} // namespace eng::core
