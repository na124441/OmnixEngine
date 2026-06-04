#pragma once
#include <vector>
#include <thread>
#include <atomic>
#include <condition_variable>
#include "Job.h"
#include "core/threading/SpinLock.h"

namespace eng::runtime {

    /**
     * @class JobSystem
     *
     * A simple **work‑stealing thread pool**:
     *   - Each worker thread owns a local deque of `Job`s.
     *   - The main thread can push jobs to any worker (usually round‑robin).
     *   - When a worker runs out of local jobs it tries to steal from the back
     *     of another worker’s deque.
     *
     * The system is **lock‑free** for the common case (push/pop on the local
     * deque) and only uses a `SpinLock` when stealing.
     *
     * All APIs are *main‑thread safe* – you may push jobs from any thread,
     * but you must **not** destroy the `JobSystem` while workers are running.
     */
    class JobSystem {
    public:
        explicit JobSystem(uint32_t workerCount = Runtime::RecommendedWorkerCount());
        ~JobSystem();

        /** Submit a batch of jobs. The function returns immediately; jobs run asynchronously. */
        void Submit(const std::vector<Job>& jobs);

        /** Block until all submitted jobs have finished. */
        void WaitAll();

        /** Query the number of worker threads. */
        uint32_t GetWorkerCount() const noexcept { return m_WorkerCount; }

    private:
        struct Worker {
            std::thread               thread;
            std::deque<Job>           localQueue;
            SpinLock                  queueLock; // only used when another thread steals
            std::atomic<bool>         shouldExit{ false };
        };

        void WorkerThreadMain(uint32_t workerIdx);

        uint32_t               m_WorkerCount;
        std::vector<Worker>    m_Workers;
        std::atomic<uint32_t> m_JobsInFlight{ 0 };
        std::condition_variable m_JobDoneCV;
        std::mutex              m_JobDoneMutex;
    };

} // namespace eng::runtime
