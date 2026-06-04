#include "Runtime/Public/AllocationDiagnostics.h"
#include "Core/Logger.h"
#include <unordered_map>
#include <mutex>
#include <string>

namespace eng::runtime {

    struct AllocInfo {
        size_t size;
        size_t count;
    };

    static std::unordered_map<std::string, AllocInfo> g_Allocations;
    static std::mutex g_AllocMutex;

    void TrackAllocation(const char* name, size_t sizeBytes) {
        std::lock_guard<std::mutex> lock(g_AllocMutex);
        auto& info = g_Allocations[name];
        info.size += sizeBytes;
        info.count++;
        LOG_INFO("[Memory] Allocated %s : %zu bytes (Total: %zu bytes across %zu allocations)", name, sizeBytes, info.size, info.count);
    }

    void TrackDeallocation(const char* name, size_t sizeBytes) {
        std::lock_guard<std::mutex> lock(g_AllocMutex);
        auto it = g_Allocations.find(name);
        if (it != g_Allocations.end()) {
            auto& info = it->second;
            if (info.count > 0) {
                info.count--;
                if (info.size >= sizeBytes) {
                    info.size -= sizeBytes;
                } else {
                    info.size = 0;
                }
                LOG_INFO("[Memory] Deallocated %s : %zu bytes (Remaining: %zu bytes across %zu active)", name, sizeBytes, info.size, info.count);
                if (info.count == 0 && info.size == 0) {
                    g_Allocations.erase(it);
                }
            } else {
                LOG_WARN("[Memory] Underflow deallocation request for %s!", name);
            }
        } else {
            LOG_WARN("[Memory] Attempted to deallocate untracked system: %s!", name);
        }
    }

    void ReportMemoryLeaks() {
        std::lock_guard<std::mutex> lock(g_AllocMutex);
        if (g_Allocations.empty()) {
            LOG_INFO("[Memory] Leak check: 0 leaked allocations. Everything gets cleaned up cleanly!");
        } else {
            LOG_ERROR("[Memory] Leak check: Memory leaks detected!");
            size_t totalLeakedBytes = 0;
            for (const auto& [name, info] : g_Allocations) {
                LOG_ERROR("[Memory] Leaked system '%s' : %zu bytes across %zu active allocations", name.c_str(), info.size, info.count);
                totalLeakedBytes += info.size;
            }
            LOG_ERROR("[Memory] Total Leaked Bytes: %zu bytes", totalLeakedBytes);
        }
    }

} // namespace eng::runtime
