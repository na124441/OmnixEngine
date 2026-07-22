#pragma once

#include "Core/Engine/Layer.h"
#include <vector>
#include <memory>
#include <algorithm>

namespace eng::core {

    class LayerStack {
    public:
        LayerStack();
        ~LayerStack();

        LayerStack(const LayerStack&) = delete;
        LayerStack& operator=(const LayerStack&) = delete;
        LayerStack(LayerStack&&) noexcept = default;
        LayerStack& operator=(LayerStack&&) noexcept = default;

        void PushLayer(std::shared_ptr<Layer> layer);
        void PushOverlay(std::shared_ptr<Layer> overlay);
        void PopLayer(std::shared_ptr<Layer> layer);
        void PopOverlay(std::shared_ptr<Layer> overlay);

        void OnUpdate(float dt);
        void OnRender();
        void OnEvent(void* event);
        void Clear();

        std::vector<std::shared_ptr<Layer>>::iterator begin() { return m_Layers.begin(); }
        std::vector<std::shared_ptr<Layer>>::iterator end() { return m_Layers.end(); }
        std::vector<std::shared_ptr<Layer>>::reverse_iterator rbegin() { return m_Layers.rbegin(); }
        std::vector<std::shared_ptr<Layer>>::reverse_iterator rend() { return m_Layers.rend(); }

        std::vector<std::shared_ptr<Layer>>::const_iterator begin() const { return m_Layers.begin(); }
        std::vector<std::shared_ptr<Layer>>::const_iterator end() const { return m_Layers.end(); }
        std::vector<std::shared_ptr<Layer>>::const_reverse_iterator rbegin() const { return m_Layers.rbegin(); }
        std::vector<std::shared_ptr<Layer>>::const_reverse_iterator rend() const { return m_Layers.rend(); }

        size_t GetSize() const { return m_Layers.size(); }
        bool IsEmpty() const { return m_Layers.empty(); }
        unsigned int GetLayerInsertIndex() const { return m_LayerInsertIndex; }

    private:
        std::vector<std::shared_ptr<Layer>> m_Layers;
        unsigned int m_LayerInsertIndex = 0;
    };

} // namespace eng::core
