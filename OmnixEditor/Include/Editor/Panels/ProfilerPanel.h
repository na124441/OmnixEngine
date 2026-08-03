#pragma once

#include "Runtime/RuntimeContext.h"
#include <vector>
#include <string>

namespace eng::runtime {

    class ProfilerPanel {
    public:
        void Initialize(RuntimeContext* context);
        void Render();

        bool IsOpen() const { return m_IsOpen; }
        void Open() { m_IsOpen = true; }
        void Close() { m_IsOpen = false; }
        void ToggleOpen() { m_IsOpen = !m_IsOpen; }

    private:
        RuntimeContext* m_Context = nullptr;
        bool m_IsOpen = true;

        static constexpr size_t HISTORY_SIZE = 120;
        float m_FrameTimeHistory[HISTORY_SIZE] = { 0.0f };
        float m_FpsHistory[HISTORY_SIZE] = { 0.0f };
        size_t m_HistoryOffset = 0;
        float m_MaxFrameTime = 33.3f;
    };

} // namespace eng::runtime
