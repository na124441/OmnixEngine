#include "Core/Job/JobSystem.h"
#include "Core/Threading/ThreadUtils.h"
#include <chrono>
#include <random>

namespace eng::core {

    JobSystem::JobSystem(uint32_t workerCount) {
        m_WorkerCount = workerCount;
        if (m_WorkerCount == 0) {
            m_WorkerCount = RecommendedWorkerCount();
        }

        m_Workers.resize(m_WorkerCount);
        for (uint32_t i = 0; i < m_WorkerCount; ++i) {
            m_Workers[i] = std::make_unique<Worker>();
            m_Workers[i]->shouldExit.store(false);
            m_Workers[i]->thread = eng::platform::Thread("WorkerThread_" + std::to_string(i), i, &JobSystem::WorkerThreadMain, this, i);
        }
    }

    JobSystem::~JobSystem() {
        for (uint32_t i = 0; i < m_WorkerCount; ++i) {
            m_Workers[i]->shouldExit.store(true);
        }

        m_WakeCV.notify_all();

        for (uint32_t i = 0; i < m_WorkerCount; ++i) {
            if (m_Workers[i]->thread.joinable()) {
                m_Workers[i]->thread.join();
            }
        }
    }

    void JobSystem::Submit(const std::vector<Job>& jobs) {
        if (jobs.empty()) return;

        m_JobsInFlight.fetch_add(static_cast<uint32_t>(jobs.size()), std::memory_order_release);

        static thread_local std::mt19937 generator(std::random_device{}());
        std::uniform_int_distribution<uint32_t> distribution(0, m_WorkerCount - 1);

        for (const auto& job : jobs) {
            uint32_t targetWorker = distribution(generator);
            auto& worker = *m_Workers[targetWorker];
            {
                std::lock_guard<SpinLock> lock(worker.queueLock);
                worker.localQueue.push_back(job);
            }
        }
        m_WakeCV.notify_all();
    }

    void JobSystem::WaitAll() {
        while (m_JobsInFlight.load(std::memory_order_acquire) > 0) {
            bool executedAny = false;
            for (uint32_t i = 0; i < m_WorkerCount; ++i) {
                Job job;
                bool gotJob = false;
                {
                    std::lock_guard<SpinLock> lock(m_Workers[i]->queueLock);
                    if (!m_Workers[i]->localQueue.empty()) {
                        job = m_Workers[i]->localQueue.back();
                        m_Workers[i]->localQueue.pop_back();
                        gotJob = true;
                    }
                }
                if (gotJob) {
                    if (job.task) {
                        try {
                            job.task();
                        } catch (...) {}
                    }
                    m_JobsInFlight.fetch_sub(1, std::memory_order_release);
                    executedAny = true;
                    break;
                }
            }

            if (!executedAny) {
                std::unique_lock<std::mutex> lock(m_JobDoneMutex);
                if (m_JobsInFlight.load(std::memory_order_acquire) > 0) {
                    m_JobDoneCV.wait_for(lock, std::chrono::microseconds(100));
                }
            }
        }
    }

    void JobSystem::WorkerThreadMain(uint32_t workerIdx) {
        auto& localWorker = *m_Workers[workerIdx];
        std::mt19937 generator(workerIdx);
        std::uniform_int_distribution<uint32_t> distribution(0, m_WorkerCount - 1);

        while (!localWorker.shouldExit.load(std::memory_order_relaxed)) {
            Job job;
            bool gotJob = false;

            // Try local queue
            {
                std::lock_guard<SpinLock> lock(localWorker.queueLock);
                if (!localWorker.localQueue.empty()) {
                    job = localWorker.localQueue.front();
                    localWorker.localQueue.pop_front();
                    gotJob = true;
                }
            }

            // Work stealing
            if (!gotJob) {
                uint32_t targetIdx = distribution(generator);
                if (targetIdx != workerIdx) {
                    auto& targetWorker = *m_Workers[targetIdx];
                    std::lock_guard<SpinLock> lock(targetWorker.queueLock);
                    if (!targetWorker.localQueue.empty()) {
                        job = targetWorker.localQueue.back();
                        targetWorker.localQueue.pop_back();
                        gotJob = true;
                    }
                }
            }

            if (gotJob) {
                if (job.task) {
                    try {
                        job.task();
                    } catch (...) {}
                }
                uint32_t remaining = m_JobsInFlight.fetch_sub(1, std::memory_order_acq_rel);
                if (remaining == 1) {
                    std::lock_guard<std::mutex> lock(m_JobDoneMutex);
                    m_JobDoneCV.notify_all();
                }
            } else {
                std::unique_lock<std::mutex> lock(m_JobDoneMutex);
                if (!localWorker.shouldExit.load(std::memory_order_relaxed) && m_JobsInFlight.load(std::memory_order_relaxed) == 0) {
                    m_WakeCV.wait_for(lock, std::chrono::milliseconds(1));
                }
            }
        }
    }

} // namespace eng::core
