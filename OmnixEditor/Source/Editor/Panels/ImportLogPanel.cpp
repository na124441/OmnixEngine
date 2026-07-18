#include "Editor/Panels/ImportLogPanel.h"
#include "Editor/AssetImportService.h"
#include "ThirdParty/imgui/imgui.h"

namespace eng::runtime {

    void ImportLogPanel::Initialize(RuntimeContext* context) {
        m_Context = context;
    }

    void ImportLogPanel::Render() {
        ImGui::Begin("Import Log");

        if (ImGui::Button("Clear Log")) {
            AssetImportService::ClearLogs();
        }

        ImGui::Separator();

        ImGui::BeginChild("LogEntriesList");
        const auto& entries = AssetImportService::GetLogEntries();

        for (const auto& entry : entries) {
            ImVec4 color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // Info: White
            const char* severityStr = "[INFO]";
            if (entry.severity == ImportLogSeverity::Warning) {
                color = ImVec4(1.0f, 0.8f, 0.2f, 1.0f); // Warning: Yellow
                severityStr = "[WARNING]";
            } else if (entry.severity == ImportLogSeverity::Error) {
                color = ImVec4(1.0f, 0.2f, 0.2f, 1.0f); // Error: Red
                severityStr = "[ERROR]";
            }

            ImGui::TextColored(color, "[%s] %s %s", entry.timestamp.c_str(), severityStr, entry.message.c_str());
        }

        ImGui::EndChild();
        ImGui::End();
    }

} // namespace eng::runtime
