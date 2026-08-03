#pragma once

#include "Runtime/RuntimeContext.h"
#include <string>
#include <glm/glm.hpp>

namespace eng::runtime {

    struct MaterialData {
        std::string name = "DefaultMaterial";
        glm::vec4 albedoColor = glm::vec4(0.8f, 0.8f, 0.8f, 1.0f);
        float metallic = 0.0f;
        float roughness = 0.5f;
        float normalScale = 1.0f;
        float ao = 1.0f;
        glm::vec3 emissiveColor = glm::vec3(0.0f);
        
        std::string albedoTexturePath = "";
        std::string normalTexturePath = "";
        std::string metallicRoughnessTexturePath = "";
        std::string aoTexturePath = "";
    };

    class MaterialEditorPanel {
    public:
        void Initialize(RuntimeContext* context);
        void Render();

        bool IsOpen() const { return m_IsOpen; }
        void Open() { m_IsOpen = true; }
        void Close() { m_IsOpen = false; }
        void ToggleOpen() { m_IsOpen = !m_IsOpen; }

        void SetActiveMaterial(const MaterialData& material) { m_Material = material; }
        const MaterialData& GetActiveMaterial() const { return m_Material; }

    private:
        RuntimeContext* m_Context = nullptr;
        bool m_IsOpen = false;
        MaterialData m_Material;
    };

} // namespace eng::runtime
