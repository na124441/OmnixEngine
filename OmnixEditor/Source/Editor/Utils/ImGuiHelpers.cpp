#include "Editor/Utils/ImGuiHelpers.h"
#include "ThirdParty/imgui/imgui.h"

namespace eng::runtime {

    namespace ImGuiHelpers {
        void LabelDisabled(const char* label, const char* text) {
            ImGui::Text("%s:", label);
            ImGui::SameLine();
            ImGui::TextDisabled("%s", text);
        }

        bool DragFloat(const char* label, float* v, float speed, float min, float max) {
            return ImGui::DragFloat(label, v, speed, min, max);
        }
    }

} // namespace eng::runtime
