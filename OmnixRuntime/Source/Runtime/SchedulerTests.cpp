#include "Runtime/SchedulerTests.h"
#include "Core/Scheduler/SystemScheduler.h"
#include "Core/Job/JobSystem.h"
#include "Core/Logger.h"
#include <atomic>
#include <vector>
#include <cassert>
#include <string>

namespace eng::runtime {

    void RunSchedulerTests() {
        CORE_LOG_INFO("=== Running Kernel System Scheduler & Task Graph Tests ===");

        // Test 1: Sequential DAG execution order A -> B -> C
        {
            CORE_LOG_INFO("[Test] Starting sequential graph execution order validation...");
            eng::core::SystemScheduler scheduler;
            std::vector<std::string> executionTrace;

            scheduler.RegisterTask("A", [&]() { executionTrace.push_back("A"); });
            scheduler.RegisterTask("B", [&]() { executionTrace.push_back("B"); });
            scheduler.RegisterTask("C", [&]() { executionTrace.push_back("C"); });

            bool dep1 = scheduler.AddDependency("B", "A"); // B depends on A
            bool dep2 = scheduler.AddDependency("C", "B"); // C depends on B

            assert(dep1 && dep2 && "Failed to add dependencies!");

            bool compiled = scheduler.CompileGraph();
            assert(compiled && "Graph compilation failed!");

            scheduler.ExecuteGraphSequential();

            assert(executionTrace.size() == 3 && "Execution trace size incorrect!");
            assert(executionTrace[0] == "A" && "Task A did not run first!");
            assert(executionTrace[1] == "B" && "Task B did not run second!");
            assert(executionTrace[2] == "C" && "Task C did not run third!");

            CORE_LOG_INFO("[Test] Sequential graph execution order validation passed.");
        }

        // Test 2: Concurrent graph execution on JobSystem
        {
            CORE_LOG_INFO("[Test] Starting concurrent graph execution...");
            eng::core::SystemScheduler scheduler;
            eng::core::JobSystem jobSystem(4);

            std::atomic<int> step{ 0 };
            std::atomic<bool> a_ran{ false };
            std::atomic<bool> b_ran{ false };
            std::atomic<bool> c_ran{ false };
            std::atomic<bool> d_ran{ false };

            // A must run before B and C.
            // B and C can run concurrently.
            // D must run after B and C.
            scheduler.RegisterTask("A", [&]() {
                assert(!b_ran.load() && !c_ran.load() && !d_ran.load() && "B, C, or D ran before A!");
                a_ran.store(true);
                step.fetch_add(1);
            });

            scheduler.RegisterTask("B", [&]() {
                assert(a_ran.load() && "A did not run before B!");
                assert(!d_ran.load() && "D ran before B!");
                b_ran.store(true);
                step.fetch_add(1);
            });

            scheduler.RegisterTask("C", [&]() {
                assert(a_ran.load() && "A did not run before C!");
                assert(!d_ran.load() && "D ran before C!");
                c_ran.store(true);
                step.fetch_add(1);
            });

            scheduler.RegisterTask("D", [&]() {
                assert(a_ran.load() && b_ran.load() && c_ran.load() && "A, B, or C did not run before D!");
                d_ran.store(true);
                step.fetch_add(1);
            });

            scheduler.AddDependency("B", "A");
            scheduler.AddDependency("C", "A");
            scheduler.AddDependency("D", "B");
            scheduler.AddDependency("D", "C");

            bool compiled = scheduler.CompileGraph();
            assert(compiled && "Concurrent graph compilation failed!");

            scheduler.ExecuteGraph(jobSystem);

            assert(step.load() == 4 && "Not all tasks ran in parallel execution!");
            assert(a_ran.load() && b_ran.load() && c_ran.load() && d_ran.load() && "All tasks should have finished!");

            CORE_LOG_INFO("[Test] Concurrent graph execution passed.");
        }

        // Test 3: Cycle detection (A -> B -> C -> A)
        {
            CORE_LOG_INFO("[Test] Starting cycle detection validation...");
            eng::core::SystemScheduler scheduler;

            scheduler.RegisterTask("A", []() {});
            scheduler.RegisterTask("B", []() {});
            scheduler.RegisterTask("C", []() {});

            scheduler.AddDependency("B", "A"); // B depends on A
            scheduler.AddDependency("C", "B"); // C depends on B
            scheduler.AddDependency("A", "C"); // A depends on C (Cycle!)

            bool compiled = scheduler.CompileGraph();
            assert(!compiled && "Scheduler compiled cycle graph successfully when it should have failed!");

            CORE_LOG_INFO("[Test] Cycle detection validation passed.");
        }

        CORE_LOG_INFO("=== All System Scheduler & Task Graph Tests Passed Successfully ===");
    }
}
