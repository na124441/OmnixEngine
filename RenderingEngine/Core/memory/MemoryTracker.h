#pragma once

#if defined(ENG_ENABLE_MEMORY_TRACKING)

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <string>
#include <mutex>
#include <iostream>

namespace eng::core {

    class MemoryTracker {
    public:
        static void OnAllocate(void* ptr, std::size_t size, const char* tag) noexcept {
            std::lock_guard<std::mutex> lock(Mutex());
            TotalAllocatedVar() += size;
            if (TotalAllocatedVar() > PeakAllocatedVar()) PeakAllocatedVar() = TotalAllocatedVar();

            auto& info = PerTag()[tag];
            info.bytes += size;
            ++info.allocations;
            PtrSizeMap()[ptr] = size;
        }

        static void OnFree(void* ptr) noexcept {
            std::lock_guard<std::mutex> lock(Mutex());
            auto it = PtrSizeMap().find(ptr);
            if (it != PtrSizeMap().end()) {
                std::size_t sz = it->second;
                TotalAllocatedVar() -= sz;
                PtrSizeMap().erase(it);
            }
        }

        static std::size_t TotalAllocated() noexcept {
            std::lock_guard<std::mutex> lock(Mutex());
            return TotalAllocatedVar();
        }

        static std::size_t PeakAllocated() noexcept {
            std::lock_guard<std::mutex> lock(Mutex());
            return PeakAllocatedVar();
        }

        static void DumpReport() noexcept {
            std::lock_guard<std::mutex> lock(Mutex());
            std::cout << "[MemoryTracker] Total live: " << TotalAllocatedVar()
                << " bytes, peak: " << PeakAllocatedVar() << " bytes\n";
            std::cout << "[MemoryTracker] Per‑tag breakdown:\n";
            for (const auto& kv : PerTag()) {
                std::cout << "  " << kv.first << " : "
                    << kv.second.bytes << " bytes in "
                    << kv.second.allocations << " allocations\n";
            }
        }

    private:
        struct TagInfo {
            std::size_t bytes = 0;
            std::size_t allocations = 0;
        };

        static std::mutex& Mutex() {
            static std::mutex m;
            return m;
        }
        static std::size_t& TotalAllocatedVar() {
            static std::size_t v = 0;
            return v;
        }
        static std::size_t& PeakAllocatedVar() {
            static std::size_t v = 0;
            return v;
        }
        static std::unordered_map<std::string, TagInfo>& PerTag() {
            static std::unordered_map<std::string, TagInfo> map;
            return map;
        }
        static std::unordered_map<void*, std::size_t>& PtrSizeMap() {
            static std::unordered_map<void*, std::size_t> map;
            return map;
        }
    };

} // namespace eng::core

#else 

#define ENG_TRACK_ALLOC(ptr, size, tag) ((void)0)
#define ENG_TRACK_FREE(ptr)             ((void)0)

#endif 
