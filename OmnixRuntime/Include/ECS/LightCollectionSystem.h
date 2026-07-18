//============================================================================
// LightCollectionSystem.h - Collects scene lights for rendering
//============================================================================

#pragma once

#include "ECS/SystemManager.h"
#include "ECS/ECSComponents.h"
#include "ECS/Coordinator.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>

namespace eng::runtime {

    struct RuntimeDirectionalLight {
        glm::vec3 direction;
        float intensity;
        glm::vec3 color;
        bool enabled;
        float temperature;
        uint32_t layerMask;
    };

    struct RuntimePointLight {
        glm::vec3 position;
        float radius;
        glm::vec3 color;
        float intensity;
        bool enabled;
        float temperature;
        uint32_t layerMask;
        float sourceRadius;
    };

    struct RuntimeSkyLight {
        glm::vec3 color;
        float intensity;
        bool enabled;
        std::string environmentPath = "";
        float rotation = 0.0f;
        float diffuseIntensity = 1.0f;
        float specularIntensity = 1.0f;
        float exposureOffset = 0.0f;
        int mode = 0; // 0 = Procedural, 1 = HDR Cubemap
    };

    struct RuntimeSpotLight {
        glm::vec3 position;
        float range;
        glm::vec3 direction;
        float intensity;
        glm::vec3 color;
        float innerConeAngle;
        float outerConeAngle;
        bool enabled;
        float temperature;
        uint32_t layerMask;
        float sourceRadius;
    };

    struct SceneLightData {
        RuntimeDirectionalLight directionalLight;
        RuntimeSkyLight skyLight;
        std::vector<RuntimePointLight> pointLights;
        std::vector<RuntimeSpotLight> spotLights;
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
            data.directionalLight.temperature = 6500.0f;
            data.directionalLight.layerMask = 0xFFFFFFFF;
            
            data.skyLight.enabled = false;
            data.skyLight.color = glm::vec3(1.0f, 1.0f, 1.0f);
            data.skyLight.intensity = 0.2f;

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
                        data.directionalLight.temperature = dirComp.temperature;
                        data.directionalLight.layerMask = dirComp.layerMask;
                        
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

            // 2. Process sky lights
            for (Entity entity : m_Entities) {
                if (!coordinator.IsEntityAlive(entity)) continue;
                auto signature = coordinator.GetSignature(entity);
                if (signature.test(coordinator.GetComponentType<SkyLightComponent>())) {
                    const auto& skyComp = coordinator.GetComponent<SkyLightComponent>(entity);
                    if (skyComp.enabled) {
                        data.skyLight.enabled = true;
                        data.skyLight.color = glm::vec3(skyComp.color.x, skyComp.color.y, skyComp.color.z);
                        data.skyLight.intensity = skyComp.intensity;
                        data.skyLight.environmentPath = skyComp.environmentPath;
                        data.skyLight.rotation = skyComp.rotation;
                        data.skyLight.diffuseIntensity = skyComp.diffuseIntensity;
                        data.skyLight.specularIntensity = skyComp.specularIntensity;
                        data.skyLight.exposureOffset = skyComp.exposureOffset;
                        data.skyLight.mode = skyComp.mode;
                        break;
                    }
                }
            }

            if (!data.skyLight.enabled) {
                data.skyLight.enabled = true;
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
                        pt.temperature = ptComp.temperature;
                        pt.layerMask = ptComp.layerMask;
                        pt.sourceRadius = ptComp.sourceRadius;
                        
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

            // 4. Process spot lights
            for (Entity entity : m_Entities) {
                if (!coordinator.IsEntityAlive(entity)) continue;
                auto signature = coordinator.GetSignature(entity);
                if (signature.test(coordinator.GetComponentType<SpotLightComponent>())) {
                    const auto& spotComp = coordinator.GetComponent<SpotLightComponent>(entity);
                    if (spotComp.enabled) {
                        RuntimeSpotLight spot;
                        spot.enabled = true;
                        spot.color = glm::vec3(spotComp.color.x, spotComp.color.y, spotComp.color.z);
                        spot.intensity = spotComp.intensity;
                        spot.range = spotComp.range;
                        spot.innerConeAngle = spotComp.innerConeAngle;
                        spot.outerConeAngle = spotComp.outerConeAngle;
                        spot.temperature = spotComp.temperature;
                        spot.layerMask = spotComp.layerMask;
                        spot.sourceRadius = spotComp.sourceRadius;
                        
                        spot.position = glm::vec3(0.0f);
                        spot.direction = glm::vec3(0.0f, 0.0f, -1.0f);
                        if (signature.test(coordinator.GetComponentType<TransformComponent>())) {
                            const auto& tc = coordinator.GetComponent<TransformComponent>(entity);
                            spot.position = glm::vec3(tc.position.x, tc.position.y, tc.position.z);
                            glm::quat q(tc.rotation.w, tc.rotation.x, tc.rotation.y, tc.rotation.z);
                            spot.direction = glm::normalize(q * glm::vec3(0.0f, 0.0f, -1.0f));
                        }
                        
                        data.spotLights.push_back(spot);
                        if (data.spotLights.size() >= 16) {
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
