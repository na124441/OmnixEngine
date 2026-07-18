#pragma once

#include "Systems/Scheduler/Public/IScheduler.h"
#include "Runtime/ProfilingHooks.h"
#include <queue>
#include <mutex>

namespace eng::runtime {

    class SystemScheduler : public IScheduler {
    public:
        SystemScheduler() = default;
        ~SystemScheduler() override = default;

        void Initialize() override {
            // Log scheduler initialization if needed
        }

        void Shutdown() override {
            std::lock_guard<std::mutex> lock(m_Mutex);
            if (!m_Tasks.empty()) {
                // Warning printed during shutdown for integration safety validation
                // We use standard printf or stream since log can be shutting down,
                // but wait, Logger is still alive. We can call printf or Logger.
                // To be safe, let's output a warning.
                std::printf("[Scheduler] Warning: Dropped %zu pending tasks during Shutdown!\n", m_Tasks.size());
                while (!m_Tasks.empty()) {
                    m_Tasks.pop();
                }
            }
        }

        // Implementation of IScheduler::Execute
        void Execute(std::function<void()>&& task) override {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_Tasks.push(std::move(task));
        }

        void RunPending() {
            std::lock_guard<std::mutex> lock(m_Mutex);
            while (!m_Tasks.empty()) {
                auto task = std::move(m_Tasks.front());
                m_Tasks.pop();
                {
                    OMNIX_PROFILE_SCOPE("SchedulerTask");
                    task();
                }
            }
        }

    private:
        std::queue<std::function<void()>> m_Tasks;
        std::mutex m_Mutex;
    };

} // namespace eng::runtime
