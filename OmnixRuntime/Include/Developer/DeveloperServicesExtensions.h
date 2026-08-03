#pragma once

#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <memory>
#include <algorithm>

namespace eng::developer {

    // -------------------------------------------------------------------------
    // 1. CPU Profiler Subsystem
    // -------------------------------------------------------------------------
    struct CPUScopeMetric {
        std::string name;
        double durationMicroseconds = 0.0;
    };

    class CPUProfiler {
    public:
        void BeginScope(const std::string& name) {
            m_StartTimes[name] = std::chrono::high_resolution_clock::now();
        }

        void EndScope(const std::string& name) {
            auto it = m_StartTimes.find(name);
            if (it != m_StartTimes.end()) {
                auto end = std::chrono::high_resolution_clock::now();
                double duration = std::chrono::duration<double, std::micro>(end - it->second).count();
                m_Metrics[name] = duration;
                m_StartTimes.erase(it);
            }
        }

        double GetScopeDurationUs(const std::string& name) const {
            auto it = m_Metrics.find(name);
            return (it != m_Metrics.end()) ? it->second : 0.0;
        }

    private:
        std::unordered_map<std::string, std::chrono::high_resolution_clock::time_point> m_StartTimes;
        std::unordered_map<std::string, double> m_Metrics;
    };

    // -------------------------------------------------------------------------
    // 2. Memory Profiler Subsystem
    // -------------------------------------------------------------------------
    class MemoryProfiler {
    public:
        void TrackAllocation(void* ptr, size_t size) {
            if (ptr) {
                m_Allocations[ptr] = size;
                m_TotalAllocatedBytes += size;
            }
        }

        void TrackDeallocation(void* ptr) {
            auto it = m_Allocations.find(ptr);
            if (it != m_Allocations.end()) {
                m_TotalAllocatedBytes -= it->second;
                m_Allocations.erase(it);
            }
        }

        size_t GetTotalAllocatedBytes() const { return m_TotalAllocatedBytes; }
        size_t GetActiveAllocationCount() const { return m_Allocations.size(); }

        bool DetectLeaks() const {
            return !m_Allocations.empty();
        }

    private:
        std::unordered_map<void*, size_t> m_Allocations;
        size_t m_TotalAllocatedBytes = 0;
    };

    // -------------------------------------------------------------------------
    // 3. GPU Profiler Subsystem
    // -------------------------------------------------------------------------
    class GPUProfiler {
    public:
        void BeginSample(const std::string& passName) {
            m_PassStartMs[passName] = 0.0f;
        }

        void EndSample(const std::string& passName, float passDurationMs) {
            m_PassTimes[passName] = passDurationMs;
        }

        float GetPassTimeMs(const std::string& passName) const {
            auto it = m_PassTimes.find(passName);
            return (it != m_PassTimes.end()) ? it->second : 0.0f;
        }

    private:
        std::unordered_map<std::string, float> m_PassStartMs;
        std::unordered_map<std::string, float> m_PassTimes;
    };

    // -------------------------------------------------------------------------
    // 4. Debug Renderer Subsystem
    // -------------------------------------------------------------------------
    struct DebugPrimitive {
        enum class Type { Line, Sphere, Box } type;
        glm::vec3 startOrCenter{ 0.0f };
        glm::vec3 endOrExtents{ 0.0f };
        glm::vec4 color{ 1.0f };
        float radius = 1.0f;
    };

    class DebugRenderer {
    public:
        void DrawLine(const glm::vec3& start, const glm::vec3& end, const glm::vec4& color = glm::vec4(1.0f)) {
            DebugPrimitive prim;
            prim.type = DebugPrimitive::Type::Line;
            prim.startOrCenter = start;
            prim.endOrExtents = end;
            prim.color = color;
            m_Primitives.push_back(prim);
        }

        void DrawSphere(const glm::vec3& center, float radius, const glm::vec4& color = glm::vec4(1.0f)) {
            DebugPrimitive prim;
            prim.type = DebugPrimitive::Type::Sphere;
            prim.startOrCenter = center;
            prim.radius = radius;
            prim.color = color;
            m_Primitives.push_back(prim);
        }

        void DrawBox(const glm::vec3& min, const glm::vec3& max, const glm::vec4& color = glm::vec4(1.0f)) {
            DebugPrimitive prim;
            prim.type = DebugPrimitive::Type::Box;
            prim.startOrCenter = min;
            prim.endOrExtents = max;
            prim.color = color;
            m_Primitives.push_back(prim);
        }

        void Clear() { m_Primitives.clear(); }
        size_t GetPrimitiveCount() const { return m_Primitives.size(); }

    private:
        std::vector<DebugPrimitive> m_Primitives;
    };

    // -------------------------------------------------------------------------
    // 5. Developer Statistics Subsystem
    // -------------------------------------------------------------------------
    class DeveloperStats {
    public:
        void RecordFrame(float deltaTimeSeconds) {
            m_FrameTimes.push_back(deltaTimeSeconds);
            if (m_FrameTimes.size() > 60) {
                m_FrameTimes.erase(m_FrameTimes.begin());
            }
        }

        float GetFPS() const {
            if (m_FrameTimes.empty()) return 0.0f;
            float sum = 0.0f;
            for (float t : m_FrameTimes) sum += t;
            float avgTime = sum / m_FrameTimes.size();
            return (avgTime > 0.00001f) ? (1.0f / avgTime) : 0.0f;
        }

        float GetAvgFrameTimeMs() const {
            if (m_FrameTimes.empty()) return 0.0f;
            float sum = 0.0f;
            for (float t : m_FrameTimes) sum += t;
            return (sum / m_FrameTimes.size()) * 1000.0f;
        }

    private:
        std::vector<float> m_FrameTimes;
    };

    // -------------------------------------------------------------------------
    // 6. Asset Validator Subsystem
    // -------------------------------------------------------------------------
    class AssetValidator {
    public:
        bool ValidateAssetPath(const std::string& path) {
            if (path.empty()) {
                m_ErrorCount++;
                return false;
            }
            return true;
        }

        bool ValidateTexture(const std::string& texturePath) {
            if (texturePath.empty() || texturePath.find(".png") == std::string::npos && texturePath.find(".dds") == std::string::npos) {
                m_ErrorCount++;
                return false;
            }
            return true;
        }

        bool ValidateMesh(const std::string& meshPath) {
            if (meshPath.empty() || meshPath.find(".obj") == std::string::npos && meshPath.find(".fbx") == std::string::npos && meshPath.find(".bin") == std::string::npos) {
                m_ErrorCount++;
                return false;
            }
            return true;
        }

        uint32_t GetErrorCount() const { return m_ErrorCount; }

    private:
        uint32_t m_ErrorCount = 0;
    };

} // namespace eng::developer
