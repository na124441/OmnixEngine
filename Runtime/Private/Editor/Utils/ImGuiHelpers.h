#pragma once

namespace eng::runtime {

    namespace ImGuiHelpers {
        void LabelDisabled(const char* label, const char* text);
        bool DragFloat(const char* label, float* v, float speed = 0.1f, float min = 0.0f, float max = 0.0f);
    }

} // namespace eng::runtime
