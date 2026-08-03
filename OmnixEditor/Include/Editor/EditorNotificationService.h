#pragma once

#include <string>
#include <vector>

namespace eng::runtime {

    enum class EditorNotificationType {
        Info,
        Success,
        Warning,
        Error
    };

    struct EditorNotification {
        EditorNotificationType Type = EditorNotificationType::Info;
        std::string Message;
        float TimeRemaining = 3.0f;
    };

    class EditorNotificationService {
    public:
        void Info(const std::string& message);
        void Success(const std::string& message);
        void Warning(const std::string& message);
        void Error(const std::string& message);

        void OnUpdate(float deltaTime);
        void OnImGuiRender();

    private:
        void Push(EditorNotificationType type, const std::string& message);

        std::vector<EditorNotification> m_Notifications;
    };
}
