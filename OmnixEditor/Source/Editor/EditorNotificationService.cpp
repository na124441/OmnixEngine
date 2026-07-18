#include "Editor/EditorNotificationService.h"

#include "Core/Logging/Logger.h"
#include "ThirdParty/imgui/imgui.h"

#include <algorithm>

namespace eng::runtime {

    void EditorNotificationService::Info(const std::string& message) {
        Push(EditorNotificationType::Info, message);
    }

    void EditorNotificationService::Success(const std::string& message) {
        Push(EditorNotificationType::Success, message);
    }

    void EditorNotificationService::Warning(const std::string& message) {
        Push(EditorNotificationType::Warning, message);
    }

    void EditorNotificationService::Error(const std::string& message) {
        Push(EditorNotificationType::Error, message);
    }

    void EditorNotificationService::OnUpdate(float deltaTime) {
        for (auto& notification : m_Notifications) {
            notification.TimeRemaining -= deltaTime;
        }

        m_Notifications.erase(
            std::remove_if(m_Notifications.begin(), m_Notifications.end(),
                [](const EditorNotification& notification) {
                    return notification.TimeRemaining <= 0.0f;
                }),
            m_Notifications.end());
    }

    void EditorNotificationService::OnImGuiRender() {
        if (m_Notifications.empty()) {
            return;
        }

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImVec2 pos(viewport->WorkPos.x + viewport->WorkSize.x - 360.0f, viewport->WorkPos.y + 20.0f);
        ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.86f);

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                                 ImGuiWindowFlags_AlwaysAutoResize |
                                 ImGuiWindowFlags_NoSavedSettings |
                                 ImGuiWindowFlags_NoFocusOnAppearing |
                                 ImGuiWindowFlags_NoNav |
                                 ImGuiWindowFlags_NoMove;

        if (ImGui::Begin("Editor Notifications", nullptr, flags)) {
            for (const auto& notification : m_Notifications) {
                ImVec4 color(0.80f, 0.84f, 0.90f, 1.0f);
                const char* label = "Info";
                if (notification.Type == EditorNotificationType::Success) {
                    color = ImVec4(0.35f, 0.90f, 0.50f, 1.0f);
                    label = "Success";
                } else if (notification.Type == EditorNotificationType::Warning) {
                    color = ImVec4(1.00f, 0.76f, 0.30f, 1.0f);
                    label = "Warning";
                } else if (notification.Type == EditorNotificationType::Error) {
                    color = ImVec4(1.00f, 0.35f, 0.35f, 1.0f);
                    label = "Error";
                }

                ImGui::TextColored(color, "%s", label);
                ImGui::SameLine();
                ImGui::TextWrapped("%s", notification.Message.c_str());
            }
        }
        ImGui::End();
    }

    void EditorNotificationService::Push(EditorNotificationType type, const std::string& message) {
        m_Notifications.push_back({ type, message, 3.5f });

        if (type == EditorNotificationType::Error) {
            CORE_LOG_ERROR("[Editor] %s", message.c_str());
        } else if (type == EditorNotificationType::Warning) {
            CORE_LOG_WARN("[Editor] %s", message.c_str());
        } else {
            CORE_LOG_INFO("[Editor] %s", message.c_str());
        }
    }
}
