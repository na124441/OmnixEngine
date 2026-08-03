#pragma once

#include <vector>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <memory>
#include "Core/Job/Job.h"
#include "Core/Threading/SpinLock.h"
#include "Core/Platform/Thread.h"

namespace eng::core {

    class JobSystem {
    public:
        explicit JobSystem(uint32_t workerCount = 0);
        ~JobSystem();

        JobSystem(const JobSystem&) = delete;
        JobSystem& operator=(const JobSystem&) = delete;

        void Submit(const std::vector<Job>& jobs);
        void WaitAll();
        uint32_t GetWorkerCount() const noexcept { return m_WorkerCount; }

    private:
        struct Worker {
            eng::platform::Thread     thread;
            std::deque<Job>           localQueue;
            SpinLock                  queueLock;
            std::atomic<bool>         shouldExit{ false };
        };

        void WorkerThreadMain(uint32_t workerIdx);

        uint32_t                                  m_WorkerCount;
        std::vector<std::unique_ptr<Worker>>     m_Workers;
        std::atomic<uint32_t>                     m_JobsInFlight{ 0 };
        std::condition_variable                   m_JobDoneCV;
        std::mutex                                m_JobDoneMutex;
        std::condition_variable                   m_WakeCV;
    };

} // namespace eng::core
