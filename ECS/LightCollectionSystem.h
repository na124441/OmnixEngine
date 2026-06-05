//============================================================================
// LightCollectionSystem.h - Collects scene lights for rendering
//============================================================================

#pragma once

#include "SystemManager.h"
#include "ECSComponents.h"
#include "Coordinator.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>

namespace eng::runtime {

    struct RuntimeDirectionalLight {
        glm::vec3 direction;
        float intensity;
        glm::vec3 color;
        bool enabled;
    };

    struct RuntimePointLight {
        glm::vec3 position;
        float radius;
        glm::vec3 color;
        float intensity;
        bool enabled;
    };

    struct RuntimeAmbientLight {
        glm::vec3 color;
        float intensity;
        bool enabled;
    };

    struct SceneLightData {
        RuntimeDirectionalLight directionalLight;
        RuntimeAmbientLight ambientLight;
        std::vector<RuntimePointLight> pointLights;
    };

    class LightCollectionSystem : public System {
    public:
        SceneLightData CollectLights(Coordinator& coordinator) {
            SceneLightData data;
            
            // Set default fallbacks
            data.directionalLight.enabled = false;
            data.directionalLight.color = glm::vec3(1.0f, 1.0f, 1.0f);
            data.directionalLight.intensity = 1.0f;
            data.directionalLight.direction = glm::vec3(0.0f, -1.0f, 0.0f);
            
            data.ambientLight.enabled = false;
            data.ambientLight.color = glm::vec3(1.0f, 1.0f, 1.0f);
            data.ambientLight.intensity = 0.2f;

            // 1. Process directional lights
            for (Entity entity : m_Entities) {
                if (!coordinator.IsEntityAlive(entity)) continue;
                auto signature = coordinator.GetSignature(entity);
                
                if (signature.test(coordinator.GetComponentType<DirectionalLightComponent>())) {
                    const auto& dirComp = coordinator.GetComponent<DirectionalLightComponent>(entity);
                    if (dirComp.enabled) {
                        data.directionalLight.enabled = true;
                        data.directionalLight.color = glm::vec3(dirComp.color.x, dirComp.color.y, dirComp.color.z);
                        data.directionalLight.intensity = dirComp.intensity;
                        
                        if (signature.test(coordinator.GetComponentType<TransformComponent>())) {
                            const auto& tc = coordinator.GetComponent<TransformComponent>(entity);
                            glm::quat q(tc.rotation.w, tc.rotation.x, tc.rotation.y, tc.rotation.z);
                            data.directionalLight.direction = glm::normalize(q * glm::vec3(0.0f, 0.0f, -1.0f));
                        }
                        break;
                    }
                }
            }

            if (!data.directionalLight.enabled) {
                data.directionalLight.enabled = true;
            }

            // 2. Process ambient lights
            for (Entity entity : m_Entities) {
                if (!coordinator.IsEntityAlive(entity)) continue;
                auto signature = coordinator.GetSignature(entity);
                if (signature.test(coordinator.GetComponentType<AmbientLightComponent>())) {
                    const auto& ambComp = coordinator.GetComponent<AmbientLightComponent>(entity);
                    if (ambComp.enabled) {
                        data.ambientLight.enabled = true;
                        data.ambientLight.color = glm::vec3(ambComp.color.x, ambComp.color.y, ambComp.color.z);
                        data.ambientLight.intensity = ambComp.intensity;
                        break;
                    }
                }
            }

            if (!data.ambientLight.enabled) {
                data.ambientLight.enabled = true;
            }

            // 3. Process point lights
            for (Entity entity : m_Entities) {
                if (!coordinator.IsEntityAlive(entity)) continue;
                auto signature = coordinator.GetSignature(entity);
                if (signature.test(coordinator.GetComponentType<PointLightComponent>())) {
                    const auto& ptComp = coordinator.GetComponent<PointLightComponent>(entity);
                    if (ptComp.enabled) {
                        RuntimePointLight pt;
                        pt.enabled = true;
                        pt.color = glm::vec3(ptComp.color.x, ptComp.color.y, ptComp.color.z);
                        pt.intensity = ptComp.intensity;
                        pt.radius = ptComp.radius;
                        
                        pt.position = glm::vec3(0.0f);
                        if (signature.test(coordinator.GetComponentType<TransformComponent>())) {
                            const auto& tc = coordinator.GetComponent<TransformComponent>(entity);
                            pt.position = glm::vec3(tc.position.x, tc.position.y, tc.position.z);
                        }
                        
                        data.pointLights.push_back(pt);
                        if (data.pointLights.size() >= 16) {
                            break;
                        }
                    }
                }
            }

            return data;
        }

        std::shared_ptr<System> Clone() const override {
            auto clone = std::make_shared<LightCollectionSystem>();
            clone->m_Entities = this->m_Entities;
            return clone;
        }
    };

} // namespace eng::runtime
