/*******************************************************************************************************************
 * @file  FrameMetrics.h
 * @brief Per-frame performance metrics and statistics tracking.
 *
 *        Collects timing data, resource usage, and performance counters for each frame.
 *        Used by the profiler, statistics display, and performance analysis tools.
 *
 *        Metrics collected:
 *          • CPU timing (frame time, subsystem times)
 *          • GPU timing (when available)
 *          • Resource usage (draw calls, triangles, memory)
 *          • Debug information (named timers, custom counters)
 *
 *        © 2024 Your Engine Project – all rights reserved.
 *******************************************************************************************************************/

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>
#include <chrono>
#include "core/types/Result.h"

namespace eng::runtime {

    /**
     * @struct FrameMetrics
     *
     * Comprehensive per-frame performance and resource usage metrics.
     *
     * This structure is populated throughout the frame execution and can be used for:
     *   - Real-time performance monitoring
     *   - Frame time analysis and optimization
     *   - Resource usage tracking
     *   - Debug visualization and profiling
     *
     * All times are stored in milliseconds for consistency and readability.
     */
    struct FrameMetrics {
        // ------------------------------------------------------------------------------------------------
        // CPU Timing Metrics
        // ------------------------------------------------------------------------------------------------

        /// Total frame time (start to present)
        double cpuFrameTimeMs = 0.0;

        /// Game simulation/update time
        double gameUpdateTimeMs = 0.0;

        /// Render scene building time (SceneBuilder)
        double renderSceneBuildTimeMs = 0.0;

        /// Visibility culling time
        double visibilityCullingTimeMs = 0.0;

        /// Frame graph building time
        double frameGraphBuildTimeMs = 0.0;

        /// GPU command recording time
        double gpuCommandRecordTimeMs = 0.0;

        /// Frame submission and synchronization time
        double gpuSubmitTimeMs = 0.0;

        /// Window event processing time
        double inputProcessingTimeMs = 0.0;

        // ------------------------------------------------------------------------------------------------
        // GPU Timing Metrics (filled by RHI after GPU completion)
        // ------------------------------------------------------------------------------------------------

        /// Total GPU frame time (from first to last command)
        double gpuTotalTimeMs = 0.0;

        /// Time spent in render passes
        double gpuRenderPassTimeMs = 0.0;

        /// Time spent in compute passes
        double gpuComputeTimeMs = 0.0;

        /// Time spent in copy/upload operations
        double gpuCopyTimeMs = 0.0;

        /// Presentation/wait time
        double gpuPresentTimeMs = 0.0;

        // ------------------------------------------------------------------------------------------------
        // Resource Usage Counters
        // ------------------------------------------------------------------------------------------------

        /// Number of draw calls issued this frame
        uint32_t drawCallCount = 0;

        /// Total number of triangles rendered
        uint32_t triangleCount = 0;

        /// Number of objects that passed culling
        uint32_t visibleObjectCount = 0;

        /// Number of objects culled (frustum, occlusion, etc.)
        uint32_t culledObjectCount = 0;

        /// Number of materials/shaders used
        uint32_t materialCount = 0;

        /// Number of textures bound
        uint32_t textureBindCount = 0;

        /// Number of buffer updates
        uint32_t bufferUpdateCount = 0;

        // ------------------------------------------------------------------------------------------------
        // Memory Usage Metrics
        // ------------------------------------------------------------------------------------------------

        /// Bytes allocated from frame linear allocator
        size_t frameAllocatorUsedBytes = 0;

        /// Bytes allocated from ring allocator
        size_t ringAllocatorUsedBytes = 0;

        /// Peak memory usage this frame
        size_t peakMemoryUsageBytes = 0;

        /// Number of temporary allocations
        uint32_t tempAllocationCount = 0;

        // ------------------------------------------------------------------------------------------------
        // Advanced/Subsystem Metrics
        // ------------------------------------------------------------------------------------------------

        /// Number of jobs executed (if using job system)
        uint32_t jobCount = 0;

        /// Number of asset loads initiated
        uint32_t assetLoadCount = 0;

        /// Number of shader recompilations
        uint32_t shaderRecompileCount = 0;

        /// Cache hit rates (if using caching systems)
        float textureCacheHitRate = 1.0f;
        float shaderCacheHitRate = 1.0f;

        // ------------------------------------------------------------------------------------------------
        // Debug and Custom Metrics
        // ------------------------------------------------------------------------------------------------

        /// Named timing sections for custom profiling
        struct NamedTimer {
            std::string name;
            double ms;

            NamedTimer(const std::string& n, double time) : name(n), ms(time) {}
        };
        std::vector<NamedTimer> namedTimers;

        /// Custom integer counters for engine-specific metrics
        struct CustomCounter {
            std::string name;
            int64_t value;

            CustomCounter(const std::string& n, int64_t val) : name(n), value(val) {}
        };
        std::vector<CustomCounter> customCounters;

        /// Custom floating-point metrics
        struct CustomMetric {
            std::string name;
            double value;

            CustomMetric(const std::string& n, double val) : name(n), value(val) {}
        };
        std::vector<CustomMetric> customMetrics;

        // ------------------------------------------------------------------------------------------------
        // Utility Methods
        // ------------------------------------------------------------------------------------------------

        /**
         * @brief Reset all metrics to zero/initial state.
         * Called at the beginning of each frame.
         */
        void Reset() noexcept
        {
            // CPU timing
            cpuFrameTimeMs = 0.0;
            gameUpdateTimeMs = 0.0;
            renderSceneBuildTimeMs = 0.0;
            visibilityCullingTimeMs = 0.0;
            frameGraphBuildTimeMs = 0.0;
            gpuCommandRecordTimeMs = 0.0;
            gpuSubmitTimeMs = 0.0;
            inputProcessingTimeMs = 0.0;

            // GPU timing
            gpuTotalTimeMs = 0.0;
            gpuRenderPassTimeMs = 0.0;
            gpuComputeTimeMs = 0.0;
            gpuCopyTimeMs = 0.0;
            gpuPresentTimeMs = 0.0;

            // Resource usage
            drawCallCount = 0;
            triangleCount = 0;
            visibleObjectCount = 0;
            culledObjectCount = 0;
            materialCount = 0;
            textureBindCount = 0;
            bufferUpdateCount = 0;

            // Memory usage
            frameAllocatorUsedBytes = 0;
            ringAllocatorUsedBytes = 0;
            peakMemoryUsageBytes = 0;
            tempAllocationCount = 0;

            // Advanced metrics
            jobCount = 0;
            assetLoadCount = 0;
            shaderRecompileCount = 0;
            textureCacheHitRate = 1.0f;
            shaderCacheHitRate = 1.0f;

            // Debug data
            namedTimers.clear();
            customCounters.clear();
            customMetrics.clear();
        }

        /**
         * @brief Add a named timer for custom profiling.
         * @param name Timer name (should be static string for performance)
         * @param ms Time in milliseconds
         */
        void AddNamedTimer(const char* name, double ms) noexcept
        {
            if (name && ms >= 0.0) {
                namedTimers.emplace_back(name, ms);
            }
        }

        /**
         * @brief Add a custom integer counter.
         * @param name Counter name
         * @param value Counter value
         */
        void AddCustomCounter(const char* name, int64_t value) noexcept
        {
            if (name) {
                customCounters.emplace_back(name, value);
            }
        }

        /**
         * @brief Add a custom floating-point metric.
         * @param name Metric name
         * @param value Metric value
         */
        void AddCustomMetric(const char* name, double value) noexcept
        {
            if (name) {
                customMetrics.emplace_back(name, value);
            }
        }

        /**
         * @brief Get FPS based on current frame time.
         * @return Frames per second (0 if frame time is 0)
         */
        double GetFPS() const noexcept
        {
            if (cpuFrameTimeMs > 0.0) {
                return 1000.0 / cpuFrameTimeMs;
            }
            return 0.0;
        }

        /**
         * @brief Get average frame time over multiple frames.
         * @param history Vector of recent frame times
         * @return Average frame time in milliseconds
         */
        static double GetAverageFrameTime(const std::vector<double>& history) noexcept
        {
            if (history.empty()) return 0.0;

            double sum = 0.0;
            for (double time : history) {
                sum += time;
            }
            return sum / static_cast<double>(history.size());
        }

        /**
         * @brief Get frame time variance/stability metric.
         * @param history Vector of recent frame times
         * @return Variance of frame times (lower is better)
         */
        static double GetFrameTimeVariance(const std::vector<double>& history) noexcept
        {
            if (history.size() < 2) return 0.0;

            double avg = GetAverageFrameTime(history);
            double variance = 0.0;

            for (double time : history) {
                double diff = time - avg;
                variance += diff * diff;
            }

            return variance / static_cast<double>(history.size() - 1);
        }

        /**
         * @brief Format metrics as a human-readable string.
         * @return Formatted string with key metrics
         */
        std::string ToString() const
        {
            std::string result;
            result.reserve(512); // Reserve space to avoid frequent reallocations

            result += "Frame Metrics:\n";
            result += "  CPU Time: " + std::to_string(cpuFrameTimeMs) + " ms (" +
                std::to_string(GetFPS()) + " FPS)\n";
            result += "  Draw Calls: " + std::to_string(drawCallCount) + "\n";
            result += "  Triangles: " + std::to_string(triangleCount) + "\n";
            result += "  Visible Objects: " + std::to_string(visibleObjectCount) + "\n";
            result += "  Culled Objects: " + std::to_string(culledObjectCount) + "\n";

            if (gpuTotalTimeMs > 0.0) {
                result += "  GPU Time: " + std::to_string(gpuTotalTimeMs) + " ms\n";
            }

            if (!namedTimers.empty()) {
                result += "  Named Timers:\n";
                for (const auto& timer : namedTimers) {
                    result += "    " + timer.name + ": " + std::to_string(timer.ms) + " ms\n";
                }
            }

            return result;
        }
    };

    // ------------------------------------------------------------------------------------------------
    // Frame Metrics History (for trend analysis)
    // ------------------------------------------------------------------------------------------------

    /**
     * @class FrameMetricsHistory
     *
     * Maintains a rolling history of frame metrics for performance analysis.
     * Useful for calculating averages, detecting performance spikes, etc.
     */
    class FrameMetricsHistory {
    public:
        explicit FrameMetricsHistory(size_t maxFrames = 120) // 2 seconds at 60 FPS
            : m_MaxFrames(maxFrames)
        {
            m_FrameTimes.reserve(maxFrames);
            m_FPSHistory.reserve(maxFrames);
        }

        void AddFrame(const FrameMetrics& metrics)
        {
            m_FrameTimes.push_back(metrics.cpuFrameTimeMs);
            m_FPSHistory.push_back(metrics.GetFPS());

            // Maintain fixed history size
            if (m_FrameTimes.size() > m_MaxFrames) {
                m_FrameTimes.erase(m_FrameTimes.begin());
                m_FPSHistory.erase(m_FPSHistory.begin());
            }

            // Update statistics
            UpdateStatistics();
        }

        double GetAverageFrameTime() const { return m_AverageFrameTime; }
        double GetAverageFPS() const { return m_AverageFPS; }
        double GetMinFrameTime() const { return m_MinFrameTime; }
        double GetMaxFrameTime() const { return m_MaxFrameTime; }
        double GetFrameTimeVariance() const { return m_FrameTimeVariance; }

        size_t GetHistorySize() const { return m_FrameTimes.size(); }
        size_t GetMaxHistorySize() const { return m_MaxFrames; }

        const std::vector<double>& GetFrameTimes() const { return m_FrameTimes; }
        const std::vector<double>& GetFPSHistory() const { return m_FPSHistory; }

    private:
        void UpdateStatistics()
        {
            if (m_FrameTimes.empty()) {
                m_AverageFrameTime = 0.0;
                m_AverageFPS = 0.0;
                m_MinFrameTime = 0.0;
                m_MaxFrameTime = 0.0;
                m_FrameTimeVariance = 0.0;
                return;
            }

            // Calculate average frame time
            double sum = 0.0;
            for (double time : m_FrameTimes) {
                sum += time;
            }
            m_AverageFrameTime = sum / static_cast<double>(m_FrameTimes.size());

            // Calculate average FPS
            m_AverageFPS = m_AverageFrameTime > 0.0 ? 1000.0 / m_AverageFrameTime : 0.0;

            // Find min/max
            m_MinFrameTime = m_FrameTimes[0];
            m_MaxFrameTime = m_FrameTimes[0];
            for (double time : m_FrameTimes) {
                if (time < m_MinFrameTime) m_MinFrameTime = time;
                if (time > m_MaxFrameTime) m_MaxFrameTime = time;
            }

            // Calculate variance
            double varianceSum = 0.0;
            for (double time : m_FrameTimes) {
                double diff = time - m_AverageFrameTime;
                varianceSum += diff * diff;
            }
            m_FrameTimeVariance = varianceSum / static_cast<double>(m_FrameTimes.size());
        }

        std::vector<double> m_FrameTimes;
        std::vector<double> m_FPSHistory;
        size_t m_MaxFrames;

        double m_AverageFrameTime = 0.0;
        double m_AverageFPS = 0.0;
        double m_MinFrameTime = 0.0;
        double m_MaxFrameTime = 0.0;
        double m_FrameTimeVariance = 0.0;
    };

} // namespace eng::runtime
