#include "Core/Scheduler/SystemScheduler.h"
#include "Runtime/ProfilingHooks.h"
#include <iostream>

namespace eng::core {

    void SystemScheduler::Shutdown() {
        std::lock_guard<std::mutex> lock(m_Mutex);
        if (!m_PendingTasks.empty()) {
            std::printf("[Scheduler] Warning: Dropped %zu pending tasks during Shutdown!\n", m_PendingTasks.size());
            while (!m_PendingTasks.empty()) {
                m_PendingTasks.pop();
            }
        }
        m_Graph.Clear();
    }

    void SystemScheduler::Execute(std::function<void()>&& task) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_PendingTasks.push(std::move(task));
    }

    void SystemScheduler::RunPending() {
        std::lock_guard<std::mutex> lock(m_Mutex);
        while (!m_PendingTasks.empty()) {
            auto task = std::move(m_PendingTasks.front());
            m_PendingTasks.pop();
            {
                OMNIX_PROFILE_SCOPE("SchedulerTask");
                task();
            }
        }
    }

    void SystemScheduler::RegisterTask(const std::string& name, std::function<void()>&& task) {
        m_Graph.AddTask(name, std::move(task));
    }

    bool SystemScheduler::AddDependency(const std::string& taskName, const std::string& dependsOnName) {
        return m_Graph.AddDependency(taskName, dependsOnName);
    }

    bool SystemScheduler::CompileGraph() {
        return m_Graph.Compile();
    }

    void SystemScheduler::ExecuteGraphSequential() {
        if (!m_Graph.Compile()) return;

        const auto& order = m_Graph.GetTopologicalOrder();
        const auto& nodes = m_Graph.GetNodes();

        for (const auto& name : order) {
            const auto& node = nodes.at(name);
            if (node.callback) {
                OMNIX_PROFILE_SCOPE("SchedulerTaskGraphSequential");
                node.callback();
            }
        }
    }

    void SystemScheduler::ExecuteGraph(eng::core::JobSystem& jobSystem) {
        if (!m_Graph.Compile()) {
            ExecuteGraphSequential();
            return;
        }

        const auto& nodes = m_Graph.GetNodes();
        if (nodes.empty()) return;

        std::unordered_map<std::string, std::atomic<int>> remainingDependencies;
        for (const auto& [name, node] : nodes) {
            remainingDependencies[name].store(static_cast<int>(node.dependencies.size()), std::memory_order_relaxed);
        }

        std::atomic<uint32_t> completedTasks{ 0 };
        uint32_t totalTasks = static_cast<uint32_t>(nodes.size());

        std::mutex waitMutex;
        std::condition_variable waitCV;

        std::function<void(const std::string&)> submitTaskNode;
        submitTaskNode = [&](const std::string& nodeName) {
            Job job;
            job.task = [&, nodeName]() {
                const auto& node = nodes.at(nodeName);
                if (node.callback) {
                    node.callback();
                }

                for (const auto& dependent : node.dependents) {
                    int prev = remainingDependencies[dependent].fetch_sub(1, std::memory_order_acq_rel);
                    if (prev == 1) {
                        submitTaskNode(dependent);
                    }
                }

                uint32_t prevCompleted = completedTasks.fetch_add(1, std::memory_order_acq_rel);
                if (prevCompleted + 1 == totalTasks) {
                    std::lock_guard<std::mutex> lock(waitMutex);
                    waitCV.notify_all();
                }
            };

            jobSystem.Submit({ job });
        };

        for (const auto& [name, node] : nodes) {
            if (node.dependencies.empty()) {
                submitTaskNode(name);
            }
        }

        std::unique_lock<std::mutex> lock(waitMutex);
        while (completedTasks.load(std::memory_order_acquire) < totalTasks) {
            waitCV.wait_for(lock, std::chrono::microseconds(100));
        }
    }

    void SystemScheduler::ClearGraph() {
        m_Graph.Clear();
    }

} // namespace eng::core
