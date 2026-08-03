#pragma once
#include <cstdint>
#include <string>

namespace eng::core {

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

    inline FrameStage StringToFrameStage(const std::string& str) {
        if (str == "FrameBegin")        return FrameStage::FrameBegin;
        if (str == "Input")             return FrameStage::Input;
        if (str == "Events")            return FrameStage::Events;
        if (str == "PreUpdate")         return FrameStage::PreUpdate;
        if (str == "Update")            return FrameStage::Update;
        if (str == "PostUpdate")        return FrameStage::PostUpdate;
        if (str == "Physics")           return FrameStage::Physics;
        if (str == "Animation")         return FrameStage::Animation;
        if (str == "RenderPreparation") return FrameStage::RenderPreparation;
        if (str == "Render")            return FrameStage::Render;
        if (str == "FrameEnd")          return FrameStage::FrameEnd;
        return FrameStage::COUNT;
    }

} // namespace eng::core
