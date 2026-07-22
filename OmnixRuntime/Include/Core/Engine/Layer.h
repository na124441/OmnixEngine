#pragma once

#include <string>
#include <memory>

namespace eng::core {

    class Layer {
    public:
        explicit Layer(const std::string& debugName = "Layer");
        virtual ~Layer();

        virtual void OnAttach() {}
        virtual void OnDetach() {}
        virtual void OnUpdate(float dt) { (void)dt; }
        virtual void OnRender() {}
        virtual void OnEvent(void* event) { (void)event; }

        const std::string& GetName() const { return m_DebugName; }
        bool IsEnabled() const { return m_Enabled; }
        void SetEnabled(bool enabled) { m_Enabled = enabled; }

    protected:
        std::string m_DebugName;
        bool m_Enabled = true;
    };

} // namespace eng::core
