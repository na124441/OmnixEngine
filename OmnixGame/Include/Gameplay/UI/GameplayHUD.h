#pragma once

#include "Gameplay/UI/GameplayHUDContext.h"
#include "Gameplay/UI/HUDNotification.h"

namespace eng::runtime {

    class GameplayHUD
    {
    public:
        GameplayHUD();
        ~GameplayHUD();

        void OnPlayStart();
        void OnPlayStop();

        void Update(float dt);
        void Render(float vpX = 0.0f, float vpY = 0.0f, float vpW = 0.0f, float vpH = 0.0f);

        void SetVisible(bool visible);
        bool IsVisible() const;

        void SetContext(const GameplayHUDContext& context);

        void ShowNotification(const std::string& text, float duration = 2.0f);
        void ClearNotifications();

    private:
        void RenderObjective(float vpX, float vpY, float vpW, float vpH);
        void RenderInteractionPrompt(float vpX, float vpY, float vpW, float vpH);
        void RenderHealth(float vpX, float vpY, float vpW, float vpH);
        void RenderNotifications(float vpX, float vpY, float vpW, float vpH);
        
        const PlayerStateComponent* GetPlayerState() const;

        bool m_Visible = true;
        GameplayHUDContext m_Context;
        HUDNotification m_Notification;
    };

} // namespace eng::runtime
