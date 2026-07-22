#include "Runtime/TimerTests.h"
#include "Core/Platform/Timer.h"
#include "Core/Logger.h"
#include <cassert>
#include <thread>
#include <chrono>

namespace eng::runtime {

    void RunTimerTests() {
        CORE_LOG_INFO("=== Running Kernel High-Resolution Timer Tests ===");

        eng::platform::Timer timer;
        auto startRes = timer.Start();
        assert(startRes == eng::core::ResultCode::Success && "Failed to start platform Timer!");

        uint64_t freq = timer.GetFrequency();
        CORE_LOG_INFO("[Test] Timer frequency reported: %llu Hz", freq);
        assert(freq > 0 && "Timer reported zero/invalid frequency!");

        uint64_t rawTicks = timer.GetRawTicks();
        CORE_LOG_INFO("[Test] Initial raw ticks: %llu", rawTicks);

        // Sleep for 50 milliseconds
        CORE_LOG_INFO("[Test] Sleeping for 50 milliseconds...");
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        double dt = timer.Tick();
        CORE_LOG_INFO("[Test] Ticked. Delta seconds reported: %f", dt);
        
        // Assert delta time is close to 0.05 seconds (within a reasonable boundary)
        assert(dt >= 0.01 && dt <= 0.15 && "Delta time is outside expected bounds (0.01 - 0.15s)!");

        // Sleep for another 20 milliseconds
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        double dt2 = timer.Tick();
        CORE_LOG_INFO("[Test] Ticked again. Delta seconds: %f", dt2);
        assert(dt2 >= 0.005 && dt2 <= 0.10 && "Second delta time is outside expected bounds!");

        CORE_LOG_INFO("=== All High-Resolution Timer Tests Passed Successfully ===");
    }

} // namespace eng::runtime
