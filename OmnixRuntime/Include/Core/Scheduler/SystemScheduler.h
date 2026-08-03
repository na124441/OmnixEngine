#pragma once

#include "Core/Scheduler/IScheduler.h"
#include "Core/Scheduler/SystemGraph.h"
#include "Core/Job/JobSystem.h"
#include <queue>
#include <mutex>
#include <condition_variable>

namespace eng::core {

    class SystemScheduler : public IScheduler {
    public:
        SystemScheduler() = default;
        ~SystemScheduler() override = default;

        void Initialize() override {}
        void Shutdown() override;
        void Execute(std::function<void()>&& task) override;
        
        void RunPending();

        // Task Graph API
        void RegisterTask(const std::string& name, std::function<void()>&& task);
        bool AddDependency(const std::string& taskName, const std::string& dependsOnName);
        bool CompileGraph();
        
        // Concurrent graph execution on JobSystem
        void ExecuteGraph(eng::core::JobSystem& jobSystem);
        
        // Sequential graph execution fallback
        void ExecuteGraphSequential();

        void ClearGraph();

    private:
        std::queue<std::function<void()>> m_PendingTasks;
        std::mutex m_Mutex;

        SystemGraph m_Graph;
    };

} // namespace eng::core
