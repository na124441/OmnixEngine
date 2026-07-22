#pragma once
#include "Core/Engine/FrameStage.h"
#include "Core/Logger.h"

namespace eng::core {

    /**
     * @class RuntimeStageTracker
     * @brief Thread-safe or static state tracker for observing the active engine FrameStage pipeline phase.
     */
    class RuntimeStageTracker {
    public:
        static void SetCurrentStage(FrameStage stage) {
            s_CurrentStage = stage;
            LOG_DEBUG("[Runtime] Transitioning to FrameStage: %s", FrameStageToString(stage));
        }

        static FrameStage GetCurrentStage() {
            return s_CurrentStage;
        }

        static const char* GetCurrentStageName() {
            return FrameStageToString(s_CurrentStage);
        }

    private:
        inline static FrameStage s_CurrentStage = FrameStage::FrameEnd;
    };

} // namespace eng::core
