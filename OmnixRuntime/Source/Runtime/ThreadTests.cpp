#include "Runtime/ThreadTests.h"
#include "Core/Platform/Thread.h"
#include "Core/Logger.h"
#include <cassert>
#include <atomic>
#include <chrono>

namespace eng::runtime {

    void RunThreadTests() {
        CORE_LOG_INFO("=== Running Kernel Platform Thread Tests ===");

        // Test 1: Simple thread execution and explicit Join
        {
            CORE_LOG_INFO("[Test] Spawning standard Thread with basic lambda execution...");
            std::atomic<bool> flag{ false };
            eng::platform::Thread thread([&flag]() {
                flag.store(true);
            });

            assert(thread.IsJoinable() && "Thread should report joinable!");
            thread.Join();
            assert(!thread.IsJoinable() && "Thread should not report joinable after Join!");
            assert(flag.load() && "Thread lambda failed to run or set flag!");
            CORE_LOG_INFO("  Standard Thread execution passed.");
        }

        // Test 2: Spawning thread with name and core affinity
        {
            CORE_LOG_INFO("[Test] Spawning Thread with name 'TestWorkerThread' and affinity 0...");
            std::atomic<bool> runStatus{ false };
            eng::platform::Thread thread("TestWorkerThread", 0, [&runStatus]() {
                runStatus.store(true);
            });

            thread.Join();
            assert(runStatus.load() && "Named and pinned thread failed to execute!");
            CORE_LOG_INFO("  Named and pinned thread execution passed.");
        }

        // Test 3: Move semantics
        {
            CORE_LOG_INFO("[Test] Testing Thread move semantics...");
            std::atomic<bool> flag{ false };
            eng::platform::Thread thread1([&flag]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                flag.store(true);
            });

            assert(thread1.IsJoinable());
            eng::platform::Thread thread2(std::move(thread1));
            assert(!thread1.IsJoinable() && "Moved-from thread should not be joinable!");
            assert(thread2.IsJoinable() && "Moved-to thread should be joinable!");

            thread2.Join();
            assert(flag.load() && "Moved thread execution failed!");
            CORE_LOG_INFO("  Thread move semantics passed.");
        }

        // Test 4: RAII auto-joining on destruction
        {
            CORE_LOG_INFO("[Test] Testing RAII auto-joining on destruction...");
            std::atomic<bool> flag{ false };
            {
                eng::platform::Thread thread([&flag]() {
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                    flag.store(true);
                });
                // Destructor gets called here, which should implicitly call Join()
            }
            assert(flag.load() && "RAII auto-join destructor failed to execute or wait for thread completion!");
            CORE_LOG_INFO("  RAII auto-join destructor passed.");
        }

        CORE_LOG_INFO("=== All Platform Thread Tests Passed Successfully ===");
    }

} // namespace eng::runtime
