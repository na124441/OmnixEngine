#include "Gameplay/UI/GameplayHUD.h"
#include "Runtime/RuntimeContext.h"
#include "Gameplay/GameplayEventBus.h"
#include "ECS/Coordinator.h"
#include "ECS/Public/IECSWorld.h"
#include "ThirdParty/imgui/imgui.h"

namespace eng::runtime {

    GameplayHUD::GameplayHUD() = default;
    GameplayHUD::~GameplayHUD() = default;

    void GameplayHUD::OnPlayStart()
    {
        m_Visible = true;
        m_Notification = HUDNotification{};
    }

    void GameplayHUD::OnPlayStop()
    {
        m_Notification = HUDNotification{};
    }

    void GameplayHUD::Update(float dt)
    {
        // Toggle visibility with F9
        if (ImGui::GetCurrentContext() != nullptr && ImGui::IsKeyPressed(ImGuiKey_F9))
        {
            m_Visible = !m_Visible;
        }

        if (m_Notification.TimeRemaining > 0.0f)
        {
            m_Notification.TimeRemaining -= dt;
            if (m_Notification.TimeRemaining < 0.0f)
            {
                m_Notification.TimeRemaining = 0.0f;
            }
        }
    }

    void GameplayHUD::Render(float vpX, float vpY, float vpW, float vpH)
    {
        if (!m_Visible || ImGui::GetCurrentContext() == nullptr) return;

        // Fallback for standalone mode
        if (vpW <= 0.0f || vpH <= 0.0f)
        {
            vpX = 0.0f;
            vpY = 0.0f;
            vpW = ImGui::GetIO().DisplaySize.x;
            vpH = ImGui::GetIO().DisplaySize.y;
        }

        RenderObjective(vpX, vpY, vpW, vpH);
        RenderInteractionPrompt(vpX, vpY, vpW, vpH);
        RenderHealth(vpX, vpY, vpW, vpH);
        RenderNotifications(vpX, vpY, vpW, vpH);
    }

    void GameplayHUD::SetVisible(bool visible)
    {
        m_Visible = visible;
    }

    bool GameplayHUD::IsVisible() const
    {
        return m_Visible;
    }

    void GameplayHUD::SetContext(const GameplayHUDContext& context)
    {
        m_Context = context;
    }

    void GameplayHUD::ShowNotification(const std::string& text, float duration)
    {
        m_Notification.Text = text;
        m_Notification.Duration = duration;
        m_Notification.TimeRemaining = duration;
    }

    const PlayerStateComponent* GameplayHUD::GetPlayerState() const
    {
        if (m_Context.PlayerState) return m_Context.PlayerState;
        if (!m_Context.ECS) return nullptr;

        auto& coordinator = m_Context.ECS->getCoordinator();
        auto playerTagType = coordinator.GetComponentType<PlayerTagComponent>();
        auto playerStateType = coordinator.GetComponentType<PlayerStateComponent>();

        for (Entity entity : coordinator.GetActiveEntities())
        {
            if (entity != 0 && coordinator.IsEntityAlive(entity))
            {
                auto sig = coordinator.GetSignature(entity);
                if (sig.test(playerTagType) && sig.test(playerStateType))
                {
                    return &coordinator.GetComponent<PlayerStateComponent>(entity);
                }
            }
        }

        return nullptr;
    }

    void GameplayHUD::RenderObjective(float vpX, float vpY, float vpW, float vpH)
    {
        if (!m_Context.GameState || !m_Context.Objectives) return;

        const std::string& activeID = m_Context.GameState->ActiveObjectiveID;
        if (activeID.empty()) return;

        const Objective* objective = m_Context.Objectives->GetObjective(activeID);
        if (!objective || objective->State != ObjectiveState::Active) return;

        ImGui::SetNextWindowPos(ImVec2(vpX + 20.0f, vpY + 20.0f));
        ImGui::SetNextWindowSize(ImVec2(350.0f, 150.0f));

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | 
                                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | 
                                 ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar |
                                 ImGuiWindowFlags_NoInputs;

        ImGui::Begin("##ObjectiveHUD", nullptr, flags);

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 pMin = ImGui::GetWindowPos();
        ImVec2 pMax = ImVec2(pMin.x + 320.0f, pMin.y + 100.0f);
        
        drawList->AddRectFilled(pMin, pMax, IM_COL32(20, 20, 25, 180), 6.0f);
        drawList->AddRect(pMin, pMax, IM_COL32(80, 80, 90, 120), 6.0f, 0, 1.0f);

        ImGui::SetCursorPos(ImVec2(15.0f, 10.0f));
        ImGui::TextColored(ImVec4(0.3f, 0.7f, 1.0f, 1.0f), "OBJECTIVE");

        ImGui::SetCursorPos(ImVec2(15.0f, 30.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 4.0f));
        
        ImGui::PushTextWrapPos(285.0f);
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%s", objective->Title.c_str());
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "%s", objective->Description.c_str());
        ImGui::PopTextWrapPos();
        
        ImGui::PopStyleVar();

        ImGui::End();
    }

    void GameplayHUD::RenderInteractionPrompt(float vpX, float vpY, float vpW, float vpH)
    {
        if (!m_Context.InteractionPrompt) return;

        const auto& prompt = *m_Context.InteractionPrompt;
        if (!prompt.Visible) return;

        ImGui::SetNextWindowPos(ImVec2(vpX + vpW * 0.5f - 150.0f, vpY + vpH * 0.7f));
        ImGui::SetNextWindowSize(ImVec2(300.0f, 60.0f));
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | 
                                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | 
                                 ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar |
                                 ImGuiWindowFlags_NoInputs;
        ImGui::Begin("##InteractPromptHUD", nullptr, flags);
        
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 pMin = ImGui::GetWindowPos();
        ImVec2 pMax = ImVec2(pMin.x + 300.0f, pMin.y + 60.0f);
        drawList->AddRectFilled(pMin, pMax, IM_COL32(20, 20, 25, 200), 8.0f);
        drawList->AddRect(pMin, pMax, IM_COL32(100, 100, 120, 150), 8.0f, 0, 1.5f);
        
        std::string promptStr = "[E] " + prompt.Text;
        ImVec2 textSize = ImGui::CalcTextSize(promptStr.c_str());
        ImGui::SetCursorPos(ImVec2((300.0f - textSize.x) * 0.5f, (60.0f - textSize.y) * 0.5f));
        ImGui::TextColored(ImVec4(0.9f, 0.9f, 1.0f, 1.0f), "%s", promptStr.c_str());
        
        ImGui::End();
    }

    void GameplayHUD::RenderHealth(float vpX, float vpY, float vpW, float vpH)
    {
        const PlayerStateComponent* playerState = GetPlayerState();
        if (!playerState) return;

        ImGui::SetNextWindowPos(ImVec2(vpX + 20.0f, vpY + vpH - 80.0f));
        ImGui::SetNextWindowSize(ImVec2(350.0f, 60.0f));
        
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | 
                                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | 
                                 ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar |
                                 ImGuiWindowFlags_NoInputs;
                                 
        ImGui::Begin("##HealthHUD", nullptr, flags);
        
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 pMin = ImGui::GetWindowPos();
        ImVec2 pMax = ImVec2(pMin.x + 300.0f, pMin.y + 50.0f);
        drawList->AddRectFilled(pMin, pMax, IM_COL32(20, 20, 25, 180), 6.0f);
        drawList->AddRect(pMin, pMax, IM_COL32(80, 80, 90, 120), 6.0f, 0, 1.0f);

        float healthVal = playerState->Health;
        if (healthVal < 0.0f) healthVal = 0.0f;
        if (healthVal > playerState->MaxHealth) healthVal = playerState->MaxHealth;
        
        if (!playerState->IsAlive || healthVal <= 0.0f)
        {
            ImGui::SetCursorPos(ImVec2(15.0f, 15.0f));
            ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "HP [ DEAD ]");
        }
        else
        {
            ImGui::SetCursorPos(ImVec2(15.0f, 8.0f));
            ImGui::TextColored(ImVec4(0.9f, 0.9f, 1.0f, 1.0f), "HP: %d / %d", 
                               static_cast<int>(healthVal), static_cast<int>(playerState->MaxHealth));
                               
            float pct = healthVal / playerState->MaxHealth;
            if (pct < 0.0f) pct = 0.0f;
            if (pct > 1.0f) pct = 1.0f;
            
            ImVec2 barMin(pMin.x + 15.0f, pMin.y + 32.0f);
            ImVec2 barMax(pMin.x + 285.0f, pMin.y + 40.0f);
            drawList->AddRectFilled(barMin, barMax, IM_COL32(40, 40, 45, 255), 3.0f);
            
            if (pct > 0.0f)
            {
                ImVec2 fillMax(barMin.x + (barMax.x - barMin.x) * pct, barMax.y);
                ImU32 barColor = IM_COL32(230, 60, 60, 255); // Red
                if (pct > 0.5f) {
                    barColor = IM_COL32(60, 220, 100, 255); // Green
                } else if (pct > 0.25f) {
                    barColor = IM_COL32(230, 180, 60, 255); // Orange/Yellow
                }
                drawList->AddRectFilled(barMin, fillMax, barColor, 3.0f);
            }
        }
        
        ImGui::End();
    }

    void GameplayHUD::RenderNotifications(float vpX, float vpY, float vpW, float vpH)
    {
        if (m_Notification.TimeRemaining <= 0.0f) return;

        ImGui::SetNextWindowPos(ImVec2(vpX + vpW * 0.5f - 200.0f, vpY + vpH * 0.35f));
        ImGui::SetNextWindowSize(ImVec2(400.0f, 80.0f));
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | 
                                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | 
                                 ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar |
                                 ImGuiWindowFlags_NoInputs;
        ImGui::Begin("##NotificationHUD", nullptr, flags);
        
        float alpha = m_Notification.TimeRemaining / 0.5f;
        if (alpha > 1.0f) alpha = 1.0f;
        
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 pMin = ImGui::GetWindowPos();
        ImVec2 pMax = ImVec2(pMin.x + 400.0f, pMin.y + 80.0f);
        
        ImU32 backdropColor = IM_COL32(15, 15, 20, static_cast<int>(200 * alpha));
        ImU32 borderColor = IM_COL32(100, 100, 120, static_cast<int>(150 * alpha));
        
        drawList->AddRectFilled(pMin, pMax, backdropColor, 8.0f);
        drawList->AddRect(pMin, pMax, borderColor, 8.0f, 0, 1.5f);
        
        ImVec2 textSize = ImGui::CalcTextSize(m_Notification.Text.c_str());
        ImGui::SetCursorPos(ImVec2((400.0f - textSize.x) * 0.5f, (80.0f - textSize.y) * 0.5f));
        
        ImGui::TextColored(ImVec4(1.0f, 0.84f, 0.0f, alpha), "%s", m_Notification.Text.c_str());
        
        ImGui::End();
    }

    void GameplayHUD::ClearNotifications()
    {
        m_Notification = HUDNotification{};
    }

} // namespace eng::runtime
