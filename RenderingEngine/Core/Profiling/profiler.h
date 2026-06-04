#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include <mutex>
#include "Core/types/Result.h"

namespace eng::core {

    class Profiler;
    extern Profiler g_Profiler;

    /**
     * @class Profiler
     * @brief The profiler records per‑frame timing data.
     */
    class Profiler {
    public:
        struct Entry {
            const char* name;
            uint64_t    startNs;
            uint64_t    durationNs;
            uint64_t    gpuStartNs{ 0 };
            uint64_t    gpuEndNs{ 0 };
        };

        class Scope {
        public:
            Scope(const char* name, uint32_t& slotIdx, Profiler& profiler) noexcept
                : m_Profiler(profiler), m_SlotIdx(slotIdx)
            {
                using Clock = std::chrono::high_resolution_clock;
                if (m_SlotIdx == UINT32_MAX) {
                    m_SlotIdx = static_cast<uint32_t>(m_Profiler.m_Entries.size());
                    Entry e{};
                    e.name = name;
                    e.startNs = Clock::now().time_since_epoch().count();
                    m_Profiler.m_Entries.push_back(e);
                }
                else {
                    if (m_SlotIdx >= m_Profiler.m_Entries.size()) {
                        m_Profiler.m_Entries.resize(m_SlotIdx + 1);
                    }
                    Entry& e = m_Profiler.m_Entries[m_SlotIdx];
                    e.name = name;
                    e.startNs = Clock::now().time_since_epoch().count();
                }
            }

            ~Scope() noexcept
            {
                using Clock = std::chrono::high_resolution_clock;
                uint64_t endNs = Clock::now().time_since_epoch().count();
                if (m_SlotIdx < m_Profiler.m_Entries.size()) {
                    m_Profiler.m_Entries[m_SlotIdx].durationNs = endNs -
                        m_Profiler.m_Entries[m_SlotIdx].startNs;
                }
            }

        private:
            Profiler& m_Profiler;
            uint32_t& m_SlotIdx;
        };

        Profiler() = default;
        ~Profiler() = default;

        void BeginFrame(uint64_t frameIdx) noexcept
        {
            m_FrameIndex = frameIdx;
            m_Entries.clear();
        }

        void EndFrame() noexcept {}

        std::vector<Entry> GetEntries() const noexcept { return m_Entries; }
        uint64_t FrameIndex() const noexcept { return m_FrameIndex; }

    private:
        uint64_t                    m_FrameIndex{ 0 };
        std::vector<Entry>           m_Entries;
        std::mutex                   m_GPUQueryMutex;
    };

    inline Profiler g_Profiler;

} // namespace eng::core

#if defined(ENG_ENABLE_PROFILE)
#define ENG_PROFILE_SCOPE(name) \
        static uint32_t __profiler_idx = UINT32_MAX; \
        eng::core::Profiler::Scope __profiler_scope(name, __profiler_idx, ::eng::core::g_Profiler)
#else
#define ENG_PROFILE_SCOPE(name) ((void)0)
#endif
