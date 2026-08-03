#include "Core/Engine/LayerStack.h"
#include "Core/Logger.h"

namespace eng::core {

    LayerStack::LayerStack()
        : m_LayerInsertIndex(0) {}

    LayerStack::~LayerStack() {
        Clear();
    }

    void LayerStack::PushLayer(std::shared_ptr<Layer> layer) {
        if (!layer) return;
        m_Layers.emplace(m_Layers.begin() + m_LayerInsertIndex, layer);
        m_LayerInsertIndex++;
        layer->OnAttach();
        LOG_INFO("[LayerStack] Pushed Layer: %s (Stack Size: %zu)", layer->GetName().c_str(), m_Layers.size());
    }

    void LayerStack::PushOverlay(std::shared_ptr<Layer> overlay) {
        if (!overlay) return;
        m_Layers.emplace_back(overlay);
        overlay->OnAttach();
        LOG_INFO("[LayerStack] Pushed Overlay: %s (Stack Size: %zu)", overlay->GetName().c_str(), m_Layers.size());
    }

    void LayerStack::PopLayer(std::shared_ptr<Layer> layer) {
        if (!layer) return;
        auto it = std::find(m_Layers.begin(), m_Layers.begin() + m_LayerInsertIndex, layer);
        if (it != m_Layers.begin() + m_LayerInsertIndex) {
            layer->OnDetach();
            m_Layers.erase(it);
            m_LayerInsertIndex--;
            LOG_INFO("[LayerStack] Popped Layer: %s (Stack Size: %zu)", layer->GetName().c_str(), m_Layers.size());
        }
    }

    void LayerStack::PopOverlay(std::shared_ptr<Layer> overlay) {
        if (!overlay) return;
        auto it = std::find(m_Layers.begin() + m_LayerInsertIndex, m_Layers.end(), overlay);
        if (it != m_Layers.end()) {
            overlay->OnDetach();
            m_Layers.erase(it);
            LOG_INFO("[LayerStack] Popped Overlay: %s (Stack Size: %zu)", overlay->GetName().c_str(), m_Layers.size());
        }
    }

    void LayerStack::OnUpdate(float dt) {
        for (auto& layer : m_Layers) {
            if (layer && layer->IsEnabled()) {
                layer->OnUpdate(dt);
            }
        }
    }

    void LayerStack::OnRender() {
        for (auto& layer : m_Layers) {
            if (layer && layer->IsEnabled()) {
                layer->OnRender();
            }
        }
    }

    void LayerStack::OnEvent(void* event) {
        for (auto it = m_Layers.rbegin(); it != m_Layers.rend(); ++it) {
            if (*it && (*it)->IsEnabled()) {
                (*it)->OnEvent(event);
            }
        }
    }

    void LayerStack::Clear() {
        for (auto it = m_Layers.rbegin(); it != m_Layers.rend(); ++it) {
            if (*it) {
                (*it)->OnDetach();
            }
        }
        m_Layers.clear();
        m_LayerInsertIndex = 0;
    }

} // namespace eng::core
