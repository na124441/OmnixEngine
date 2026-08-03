#pragma once

#include <cstdint>
#include <thread>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <processthreadsapi.h>
#include <string>
#elif defined(__linux__)
#include <pthread.h>
#include <sched.h>
#elif defined(__APPLE__)
#include <pthread.h>
#endif

namespace eng::core {

    inline uint32_t GetHardwareThreadCount() {
        return static_cast<uint32_t>(std::thread::hardware_concurrency());
    }

    // Hint for job system: create N workers = hardware threads - 1 (reserve 1 for main thread)
    inline uint32_t RecommendedWorkerCount() {
        const uint32_t hc = GetHardwareThreadCount();
        return (hc > 1) ? hc - 1 : 1;
    }

    // Simple scoped thread wrapper for quick fire-and-forget tasks (used only in tests)
    class ScopedThread {
    public:
        template <typename Callable>
        explicit ScopedThread(Callable&& fn) : thread_(std::forward<Callable>(fn)) {}
        ~ScopedThread() { if (thread_.joinable()) thread_.join(); }
    private:
        std::thread thread_;
    };

    // Platform-independent thread naming helper
    inline void SetThreadName(std::thread& thread, const char* name) {
#if defined(_WIN32)
        std::string sName(name);
        std::wstring wName(sName.begin(), sName.end());
        SetThreadDescription(thread.native_handle(), wName.c_str());
#elif defined(__linux__)
        pthread_setname_np(thread.native_handle(), name);
#endif
    }

    inline void SetCurrentThreadName(const char* name) {
#if defined(_WIN32)
        std::string sName(name);
        std::wstring wName(sName.begin(), sName.end());
        SetThreadDescription(GetCurrentThread(), wName.c_str());
#elif defined(__linux__)
        pthread_setname_np(pthread_self(), name);
#elif defined(__APPLE__)
        pthread_setname_np(name);
#endif
    }

    // Platform-independent CPU affinity pinning helper
    inline bool SetThreadAffinity(std::thread& thread, uint32_t coreIndex) {
#if defined(_WIN32)
        DWORD_PTR mask = 1ULL << coreIndex;
        return SetThreadAffinityMask(thread.native_handle(), mask) != 0;
#elif defined(__linux__)
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(coreIndex, &cpuset);
        return pthread_setaffinity_np(thread.native_handle(), sizeof(cpu_set_t), &cpuset) == 0;
#else
        return false;
#endif
    }

    inline bool SetCurrentThreadAffinity(uint32_t coreIndex) {
#if defined(_WIN32)
        DWORD_PTR mask = 1ULL << coreIndex;
        return SetThreadAffinityMask(GetCurrentThread(), mask) != 0;
#elif defined(__linux__)
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(coreIndex, &cpuset);
        return pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) == 0;
#else
        return false;
#endif
    }

} // namespace eng::core
