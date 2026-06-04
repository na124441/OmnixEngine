#pragma once
#include <cstdint>

namespace eng::runtime {

    enum class FrameStage : uint8_t {
        FrameBegin = 0,
        Input,
        Events,
        PreUpdate,
        Update,
        PostUpdate,
        Physics,
        Animation,
        RenderPreparation,
        Render,
        FrameEnd,
        COUNT
    };

    inline const char* FrameStageToString(FrameStage stage) {
        switch (stage) {
            case FrameStage::FrameBegin:         return "FrameBegin";
            case FrameStage::Input:              return "Input";
            case FrameStage::Events:             return "Events";
            case FrameStage::PreUpdate:          return "PreUpdate";
            case FrameStage::Update:             return "Update";
            case FrameStage::PostUpdate:         return "PostUpdate";
            case FrameStage::Physics:            return "Physics";
            case FrameStage::Animation:          return "Animation";
            case FrameStage::RenderPreparation:  return "RenderPreparation";
            case FrameStage::Render:             return "Render";
            case FrameStage::FrameEnd:           return "FrameEnd";
            default:                             return "Unknown";
        }
    }

} // namespace eng::runtime
