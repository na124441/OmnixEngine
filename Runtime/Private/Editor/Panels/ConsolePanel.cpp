#include "Runtime/Private/Editor/Panels/ConsolePanel.h"
#include "ThirdParty/imgui/imgui.h"

namespace eng::runtime {

    void ConsolePanel::Initialize(RuntimeContext* context) {
        m_Context = context;
    }

    void ConsolePanel::Render() {
        ImGui::Begin("Console");
        ImGui::Text("Engine logs and diagnostics...");
        ImGui::End();
    }

} // namespace eng::runtime
