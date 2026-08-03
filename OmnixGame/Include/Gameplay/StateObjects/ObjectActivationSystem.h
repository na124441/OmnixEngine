#pragma once

#include "Gameplay/GameplayEvent.h"
#include <string>

#include "Runtime/RuntimeContext.h"

namespace eng::runtime {

    class ObjectActivationSystem
    {
    public:
        ObjectActivationSystem() = default;
        ~ObjectActivationSystem() = default;

        void Initialize(RuntimeContext* context);
        void OnPlayStart();
        void OnPlayStop();
        void Update(float dt);

        void OnGameplayEvent(const GameplayEvent& event);

        // Getters for diagnostics
        size_t GetStateObjectsCount() const;
        size_t GetActivatableObjectsCount() const;
        size_t GetDoorsCount() const;
        std::string GetLastActivationID() const { return m_LastActivationID; }
        std::string GetLastActivationError() const { return m_LastActivationError; }

        void DrawDiagnosticsGUI();

    private:
        void ActivateByID(const std::string& activationID);
        void ActivateEntity(uint32_t entity);

        RuntimeContext* m_Context = nullptr;
        std::string m_LastActivationID = "None";
        std::string m_LastActivationError = "None";
    };

} // namespace eng::runtime
