#include "Runtime/EngineRuntime.h"
#include "Core/Logger.h"
#include <chrono>

namespace eng::runtime {

    // Global diagnostic statistics
    struct RuntimeDiagnostics {
        uint64_t frameCount = 0;
        double totalFrameTimeMs = 0.0;
        double minFrameTimeMs = 10000.0;
        double maxFrameTimeMs = 0.0;
    };

    static RuntimeDiagnostics g_Diagnostics;

    void LogFrameDiagnostics(double dtSeconds) {
        double dtMs = dtSeconds * 1000.0;
        g_Diagnostics.frameCount++;
        g_Diagnostics.totalFrameTimeMs += dtMs;
        g_Diagnostics.minFrameTimeMs = (dtMs < g_Diagnostics.minFrameTimeMs) ? dtMs : g_Diagnostics.minFrameTimeMs;
        g_Diagnostics.maxFrameTimeMs = (dtMs > g_Diagnostics.maxFrameTimeMs) ? dtMs : g_Diagnostics.maxFrameTimeMs;

        if (g_Diagnostics.frameCount % 300 == 0) {
            double avg = g_Diagnostics.totalFrameTimeMs / static_cast<double>(g_Diagnostics.frameCount);
            LOG_DEBUG("[Diagnostics] Frame Stats over last 300 frames - Avg: %.2f ms (%.1f FPS), Min: %.2f ms, Max: %.2f ms",
                      avg, 1000.0 / avg, g_Diagnostics.minFrameTimeMs, g_Diagnostics.maxFrameTimeMs);
            
            // Reset interval counters
            g_Diagnostics.minFrameTimeMs = 10000.0;
            g_Diagnostics.maxFrameTimeMs = 0.0;
        }
    }

} // namespace eng::runtime
