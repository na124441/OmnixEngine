#include "TimerTests.h"
#include "Core/Timer.h"
#include "Core/Logging/Logger.h"

#include <thread>
#include <chrono>

namespace eng::core {

    bool RunTimerTests() noexcept {
        LOG_INFO("================================================================================");
        LOG_INFO("                             RUNNING TIMER TESTS                                ");
        LOG_INFO("================================================================================");

        // -----------------------------------------------------------------------------
        // Test 1 — Init and Initial State Test
        // -----------------------------------------------------------------------------
        LOG_INFO("[TimerTest] Running Test 1: Initialization State...");
        Timer::Init();
        double initialElapsed = Timer::GetElapsedSeconds();
        double initialDelta = Timer::GetDeltaSeconds();

        // Check initialization state (both should be close to 0)
        if (initialElapsed > 0.1 || initialDelta != 0.0) {
            LOG_ERROR("[TimerTest] Test 1 FAILED: Init() did not reset timer properly (Elapsed: %f, Delta: %f)", initialElapsed, initialDelta);
            return false;
        }
        LOG_INFO("[TimerTest] Test 1 Passed.");

        // -----------------------------------------------------------------------------
        // Test 2 — Elapsed Time Test
        // -----------------------------------------------------------------------------
        LOG_INFO("[TimerTest] Running Test 2: Elapsed Time Measurement...");
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        double elapsedAfterSleep = Timer::GetElapsedSeconds();
        if (elapsedAfterSleep < 0.04 || elapsedAfterSleep > 0.1) {
            LOG_ERROR("[TimerTest] Test 2 FAILED: Elapsed time not recorded accurately (Elapsed: %f)", elapsedAfterSleep);
            return false;
        }
        LOG_INFO("[TimerTest] Test 2 Passed.");

        // -----------------------------------------------------------------------------
        // Test 3 — Delta Time Test
        // -----------------------------------------------------------------------------
        LOG_INFO("[TimerTest] Running Test 3: Delta Time Measurement...");
        Timer::Update(); // First update to set lastTime to current time

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        Timer::Update(); // Second update to measure the delta

        double deltaAfterSleep = Timer::GetDeltaSeconds();
        if (deltaAfterSleep < 0.04 || deltaAfterSleep > 0.1) {
            LOG_ERROR("[TimerTest] Test 3 FAILED: Delta time not recorded accurately (Delta: %f)", deltaAfterSleep);
            return false;
        }
        LOG_INFO("[TimerTest] Test 3 Passed.");

        LOG_INFO("================================================================================");
        LOG_INFO("                          ALL TIMER TESTS PASSED                                ");
        LOG_INFO("================================================================================");
        return true;
    }

} // namespace eng::core
