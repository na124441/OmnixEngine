#include "Runtime/JobTests.h"
#include "Core/Job/JobSystem.h"
#include "Core/Job/Fiber.h"
#include "Core/Logger.h"
#include <atomic>
#include <vector>
#include <cassert>

namespace eng::runtime {

    struct FiberTestContext {
        std::atomic<uint32_t> counter{ 0 };
        eng::core::Fiber* fiberPtr = nullptr;
    };

    static void TestFiberFunc(void* userData) {
        FiberTestContext* ctx = static_cast<FiberTestContext*>(userData);
        ctx->counter.fetch_add(1); // counter becomes 1
        ctx->fiberPtr->Yield();    // yields back to main
        ctx->counter.fetch_add(2); // counter becomes 3
    }

    void RunJobTests() {
        CORE_LOG_INFO("=== Running Kernel Job System & Fiber Tests ===");

        // 1. JobSystem Basic Test
        {
            CORE_LOG_INFO("[Test] Starting JobSystem parallel execution...");
            eng::core::JobSystem js(4);
            std::atomic<uint32_t> counter{ 0 };
            std::vector<eng::core::Job> jobs;
            for (int i = 0; i < 100; ++i) {
                jobs.push_back({ [&counter]() {
                    counter.fetch_add(1, std::memory_order_relaxed);
                } });
            }
            js.Submit(jobs);
            js.WaitAll();

            assert(counter.load() == 100 && "JobSystem parallel execution failed!");
            CORE_LOG_INFO("[Test] JobSystem parallel execution passed.");
        }

        // 2. JobSystem Nested/Recursive Test
        {
            CORE_LOG_INFO("[Test] Starting JobSystem nested execution...");
            eng::core::JobSystem js(2);
            std::atomic<uint32_t> innerCounter{ 0 };
            std::atomic<uint32_t> innerFinished{ 0 };

            std::vector<eng::core::Job> outerJobs;
            outerJobs.push_back({ [&js, &innerCounter, &innerFinished]() {
                std::vector<eng::core::Job> innerJobs;
                for (int i = 0; i < 10; ++i) {
                    innerJobs.push_back({ [&innerCounter, &innerFinished]() {
                        innerCounter.fetch_add(1);
                        innerFinished.fetch_add(1);
                    } });
                }
                js.Submit(innerJobs);
                // Worker thread yields/waits for inner jobs without blocking the main-thread WaitAll
                while (innerFinished.load() < 10) {
                    std::this_thread::yield();
                }
            } });

            js.Submit(outerJobs);
            js.WaitAll();

            assert(innerCounter.load() == 10 && "JobSystem nested execution failed!");
            CORE_LOG_INFO("[Test] JobSystem nested execution passed.");
        }

        // 3. Fiber Test
        {
            CORE_LOG_INFO("[Test] Starting Fiber switching...");
            FiberTestContext ctx;
            eng::core::Fiber fib(TestFiberFunc, &ctx);
            ctx.fiberPtr = &fib;

            assert(ctx.counter.load() == 0 && "Fiber started prematurely!");
            
            // Switch to fiber
            fib.SwitchTo();
            assert(ctx.counter.load() == 1 && "Fiber failed to yield or execute!");

            // Switch back to fiber to complete it
            fib.SwitchTo();
            assert(ctx.counter.load() == 3 && "Fiber failed to resume or execute finalizer!");

            CORE_LOG_INFO("[Test] Fiber switching passed.");
        }

        CORE_LOG_INFO("=== All Job System & Fiber Tests Passed Successfully ===");
    }
}
