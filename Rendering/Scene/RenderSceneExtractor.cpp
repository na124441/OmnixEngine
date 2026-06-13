#include "Core/pch.h"
#include "RenderSceneExtractor.h"
#include "RenderingEngine/Renderer/scene/RenderQueue.h"
#include "RenderingEngine/Core/Engine/EngineResources.h"
#include "RenderingEngine/Renderer/LightingUBO.h"
#include <glm/gtc/type_ptr.hpp>

#define OMNIX_DONT_DEFINE_GLOBAL_LOG_MACROS
#include "Core/World.h"
#include "ECS/Public/IECSWorld.h"
#include "ECS/ECSComponents.h"
#include "ECS/LightCollectionSystem.h"
#include "Runtime/Public/AssetRegistry.h"
#include "Runtime/Public/OmnixMaterialFormat.h"
#include "Runtime/Public/World/ZoneEntityComponent.h"
#include "Core/Logging/Logger.h"

namespace eng::renderer {

static std::unordered_map<uint32_t, glm::mat4> s_PreviousTransforms;

void RenderSceneExtractor::ExtractScene(
    eng::runtime::IECSWorld& world,
    eng::runtime::AssetRegistry* assetRegistry,
    const CameraComponent& activeCamera,
    const glm::mat4& cameraWorldMatrix,
    RenderScene& outRenderScene,
    bool useEditorDefaultLighting
)
{
    outRenderScene.meshInstances.clear();
    outRenderScene.directionalLights.clear();
    outRenderScene.pointLights.clear();
    outRenderScene.spotLights.clear();

    auto& coordinator = world.getCoordinator();
    const auto& entities = coordinator.GetActiveEntities();

    std::unordered_map<uint32_t, glm::mat4> currentTransforms;

    auto transformType = coordinator.GetComponentType<TransformComponent>();
    auto meshRendererType = coordinator.GetComponentType<MeshRendererComponent>();
    auto zoneEntityCompType = coordinator.GetComponentType<eng::runtime::ZoneEntityComponent>();
    auto renderableMeshType = coordinator.GetComponentType<RenderableMeshComponent>();
    auto materialType = coordinator.GetComponentType<MaterialComponent>();
    auto boundsType = coordinator.GetComponentType<BoundsComponent>();

    // 1. Loop over entities and extract render mesh instances
    for (Entity entity : entities) {
        if (entity == 0 || !coordinator.IsEntityAlive(entity)) {
            continue;
        }

        Signature sig = coordinator.GetSignature(entity);
        if (sig.test(transformType) && sig.test(meshRendererType)) {
            // Check if ZoneEntityComponent is attached and simulating is false
            if (sig.test(zoneEntityCompType)) {
                const auto& zec = coordinator.GetComponent<eng::runtime::ZoneEntityComponent>(entity);
                if (!zec.simulating) {
                    continue;
                }
            }

            auto& transform = coordinator.GetComponent<TransformComponent>(entity);
            auto& meshRenderer = coordinator.GetComponent<MeshRendererComponent>(entity);

            if (meshRenderer.visible) {
                if (transform.dirty) {
                    transform.worldMatrix = Matrix4x4::TRS(transform.position, transform.rotation, transform.scale);
                    transform.dirty = false;
                }

                RenderMeshInstance inst{};
                inst.entityID = static_cast<uint32_t>(entity);
                inst.flags = 0;
                inst.castShadows = meshRenderer.castShadows;

                // Copy transform matrix
                glm::mat4 m(1.0f);
                std::memcpy(glm::value_ptr(m), transform.worldMatrix.m, sizeof(float) * 16);
                inst.worldMatrix = m;
                currentTransforms[entity] = m;

                // Previous transform matrix lookup
                if (s_PreviousTransforms.find(entity) != s_PreviousTransforms.end()) {
                    inst.previousWorldMatrix = s_PreviousTransforms[entity];
                } else {
                    inst.previousWorldMatrix = m;
                }

                // Resolve handles
                if (sig.test(renderableMeshType)) {
                    const auto& rm = coordinator.GetComponent<RenderableMeshComponent>(entity);
                    inst.meshHandle = rm.meshAssetHandle;
                }
                if (sig.test(materialType)) {
                    const auto& mc = coordinator.GetComponent<MaterialComponent>(entity);
                    inst.materialHandle = mc.materialAssetHandle;
                }

                // Copy world bounds from BoundsComponent if available
                if (sig.test(boundsType)) {
                    const auto& bounds = coordinator.GetComponent<BoundsComponent>(entity);
                    inst.worldBounds.min.x = bounds.worldMin.x;
                    inst.worldBounds.min.y = bounds.worldMin.y;
                    inst.worldBounds.min.z = bounds.worldMin.z;
                    inst.worldBounds.max.x = bounds.worldMax.x;
                    inst.worldBounds.max.y = bounds.worldMax.y;
                    inst.worldBounds.max.z = bounds.worldMax.z;
                } else {
                    // Fallback unit box around the transform position
                    inst.worldBounds.min.x = transform.position.x - 0.5f;
                    inst.worldBounds.min.y = transform.position.y - 0.5f;
                    inst.worldBounds.min.z = transform.position.z - 0.5f;
                    inst.worldBounds.max.x = transform.position.x + 0.5f;
                    inst.worldBounds.max.y = transform.position.y + 0.5f;
                    inst.worldBounds.max.z = transform.position.z + 0.5f;
                }

                outRenderScene.meshInstances.push_back(inst);
            }
        }
    }

    // Save current transforms for the next frame
    s_PreviousTransforms = std::move(currentTransforms);

    // Derive camera position, target, and up from the cameraWorldMatrix.
    // This matrix is already the inverse of the correct view matrix (set by
    // EditorLayer in both Edit and Play modes), so no entity lookup is needed.
    glm::vec3 camPos = glm::vec3(cameraWorldMatrix[3]);
    glm::vec3 forwardDir = glm::vec3(cameraWorldMatrix * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f));
    glm::vec3 upDir = glm::vec3(cameraWorldMatrix * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f));
    glm::vec3 camTarget = camPos + forwardDir;
    glm::vec3 camUp = upDir;

    outRenderScene.camera.position = camPos;
    outRenderScene.camera.fov = activeCamera.fov;
    outRenderScene.camera.nearPlane = activeCamera.nearPlane;
    outRenderScene.camera.farPlane = activeCamera.farPlane;
    outRenderScene.camera.aspectRatio = activeCamera.aspectRatio;
    outRenderScene.camera.exposure = activeCamera.exposure;
    outRenderScene.camera.viewMatrix = glm::lookAt(camPos, camTarget, camUp);
    outRenderScene.camera.projectionMatrix = glm::perspective(
        glm::radians(activeCamera.fov),
        activeCamera.aspectRatio,
        activeCamera.nearPlane,
        activeCamera.farPlane
    );

    // 3. Extract lights
    bool sceneHasLights = false;
    auto directionalLightType = coordinator.GetComponentType<DirectionalLightComponent>();
    auto pointLightType = coordinator.GetComponentType<PointLightComponent>();
    auto skyLightType = coordinator.GetComponentType<SkyLightComponent>();
    auto spotLightType = coordinator.GetComponentType<SpotLightComponent>();

    for (Entity ent : coordinator.GetActiveEntities()) {
        if (!coordinator.IsEntityAlive(ent)) continue;
        auto sig = coordinator.GetSignature(ent);
        if (sig.test(directionalLightType) ||
            sig.test(pointLightType) ||
            sig.test(skyLightType) ||
            sig.test(spotLightType)) {
            sceneHasLights = true;
            break;
        }
    }

    if (useEditorDefaultLighting || !sceneHasLights) {
        DirectionalLightGPU dirLight{};
        dirLight.direction = glm::normalize(glm::vec3(-0.35f, -0.85f, -0.35f));
        dirLight.color = glm::vec3(1.0f, 0.96f, 0.86f);
        dirLight.intensity = 3.5f;
        outRenderScene.directionalLights.push_back(dirLight);

        outRenderScene.skyLight.color = glm::vec3(0.45f, 0.50f, 0.58f);
        outRenderScene.skyLight.intensity = 0.55f;
    } else {
        // Directional Lights
        for (Entity ent : coordinator.GetActiveEntities()) {
            if (!coordinator.IsEntityAlive(ent)) continue;
            auto sig = coordinator.GetSignature(ent);
            if (sig.test(directionalLightType)) {
                const auto& dirComp = coordinator.GetComponent<DirectionalLightComponent>(ent);
                if (dirComp.enabled) {
                    DirectionalLightGPU dirLight{};
                    dirLight.color = glm::vec3(dirComp.color.x, dirComp.color.y, dirComp.color.z);
                    dirLight.intensity = dirComp.intensity;
                    dirLight.direction = glm::vec3(0.0f, -1.0f, 0.0f);
                    if (sig.test(transformType)) {
                        const auto& tc = coordinator.GetComponent<TransformComponent>(ent);
                        glm::quat q(tc.rotation.w, tc.rotation.x, tc.rotation.y, tc.rotation.z);
                        dirLight.direction = glm::normalize(q * glm::vec3(0.0f, 0.0f, -1.0f));
                    }
                    dirLight.castShadows = dirComp.castShadows ? 1.0f : 0.0f;
                    dirLight.shadowBias = dirComp.shadowBias;
                    dirLight.shadowNormalBias = dirComp.shadowNormalBias;
                    dirLight.shadowStrength = dirComp.shadowStrength;
                    dirLight.shadowResolution = dirComp.shadowResolution;
                    outRenderScene.directionalLights.push_back(dirLight);
                }
            }
        }

        // Sky Light
        for (Entity ent : coordinator.GetActiveEntities()) {
            if (!coordinator.IsEntityAlive(ent)) continue;
            auto sig = coordinator.GetSignature(ent);
            if (sig.test(skyLightType)) {
                const auto& skyComp = coordinator.GetComponent<SkyLightComponent>(ent);
                if (skyComp.enabled) {
                    outRenderScene.skyLight.color = glm::vec3(skyComp.color.x, skyComp.color.y, skyComp.color.z);
                    outRenderScene.skyLight.intensity = skyComp.intensity;
                    break;
                }
            }
        }

        // Point Lights
        for (Entity ent : coordinator.GetActiveEntities()) {
            if (!coordinator.IsEntityAlive(ent)) continue;
            auto sig = coordinator.GetSignature(ent);
            if (sig.test(pointLightType)) {
                const auto& ptComp = coordinator.GetComponent<PointLightComponent>(ent);
                if (ptComp.enabled) {
                    PointLightGPU pt{};
                    pt.color = glm::vec3(ptComp.color.x, ptComp.color.y, ptComp.color.z);
                    pt.intensity = ptComp.intensity;
                    pt.radius = ptComp.radius;
                    pt.position = glm::vec3(0.0f);
                    if (sig.test(transformType)) {
                        const auto& tc = coordinator.GetComponent<TransformComponent>(ent);
                        pt.position = glm::vec3(tc.position.x, tc.position.y, tc.position.z);
                    }
                    outRenderScene.pointLights.push_back(pt);
                }
            }
        }

        // Spot Lights
        for (Entity ent : coordinator.GetActiveEntities()) {
            if (!coordinator.IsEntityAlive(ent)) continue;
            auto sig = coordinator.GetSignature(ent);
            if (sig.test(spotLightType)) {
                const auto& spotComp = coordinator.GetComponent<SpotLightComponent>(ent);
                if (spotComp.enabled) {
                    SpotLightGPU spot{};
                    spot.color = glm::vec3(spotComp.color.x, spotComp.color.y, spotComp.color.z);
                    spot.intensity = spotComp.intensity;
                    spot.range = spotComp.range;
                    spot.innerConeAngle = spotComp.innerConeAngle;
                    spot.outerConeAngle = spotComp.outerConeAngle;
                    spot.position = glm::vec3(0.0f);
                    spot.direction = glm::vec3(0.0f, 0.0f, -1.0f);
                    if (sig.test(transformType)) {
                        const auto& tc = coordinator.GetComponent<TransformComponent>(ent);
                        spot.position = glm::vec3(tc.position.x, tc.position.y, tc.position.z);
                        glm::quat q(tc.rotation.w, tc.rotation.x, tc.rotation.y, tc.rotation.z);
                        spot.direction = glm::normalize(q * glm::vec3(0.0f, 0.0f, -1.0f));
                    }
                    outRenderScene.spotLights.push_back(spot);
                }
            }
        }

        if (outRenderScene.skyLight.intensity == 0.0f) {
            outRenderScene.skyLight.color = glm::vec3(0.10f, 0.12f, 0.16f);
            outRenderScene.skyLight.intensity = 0.35f;
        }
    }
}

void RenderSceneExtractor::DebugPrint(const RenderScene& scene)
{
    CORE_LOG_INFO("--- RenderScene Extraction Debug Report ---");
    CORE_LOG_INFO("Mesh Instances: {}", scene.meshInstances.size());
    for (size_t i = 0; i < scene.meshInstances.size(); ++i) {
        const auto& inst = scene.meshInstances[i];
        CORE_LOG_INFO("  [{}] Entity: {}, MeshHandle: {}, MaterialHandle: {}", 
            i, inst.entityID, inst.meshHandle.value, inst.materialHandle.value);
    }
    CORE_LOG_INFO("Directional Lights: {}", scene.directionalLights.size());
    CORE_LOG_INFO("Point Lights: {}", scene.pointLights.size());
    CORE_LOG_INFO("Spot Lights: {}", scene.spotLights.size());
    CORE_LOG_INFO("Sky/Ambient Light: Color({}, {}, {}), Intensity: {}",
        scene.skyLight.color.r, scene.skyLight.color.g, scene.skyLight.color.b, scene.skyLight.intensity);
    CORE_LOG_INFO("Camera Position: ({}, {}, {})", 
        scene.camera.position.x, scene.camera.position.y, scene.camera.position.z);
    CORE_LOG_INFO("-------------------------------------------");
}

void RenderSceneExtractor::ExtractLighting(
    eng::runtime::World* world,
    bool useEditorDefaultLighting,
    uint32_t shadingMode,
    LightData& uboData,
    bool& lastFallbackActive
)
{
    // Editor Preview Lighting (Sunny preview fallback settings)
    glm::vec3 activeLightDir = glm::vec3(-0.35f, -0.85f, -0.35f);
    glm::vec3 activeLightCol = glm::vec3(1.0f, 0.96f, 0.86f);
    float activeLightIntensity = 3.5f;
    glm::vec3 activeAmbientCol = glm::vec3(0.45f, 0.50f, 0.58f);
    float activeAmbientIntensity = 0.55f;

    uboData.ambientColorIntensity = glm::vec4(activeAmbientCol, activeAmbientIntensity);
    uboData.directionalDirectionIntensity = glm::vec4(glm::normalize(activeLightDir), activeLightIntensity);
    uboData.directionalColor = glm::vec4(activeLightCol, 1.0f);
    uboData.pointLightCount = 0;
    uboData.shadingMode = shadingMode;

    bool sceneHasLights = false;
    if (world) {
        auto& coordinator = world->getCoordinator();
        for (Entity ent : coordinator.GetActiveEntities()) {
            if (!coordinator.IsEntityAlive(ent)) continue;
            auto sig = coordinator.GetSignature(ent);
            if (sig.test(coordinator.GetComponentType<DirectionalLightComponent>()) ||
                sig.test(coordinator.GetComponentType<PointLightComponent>()) ||
                sig.test(coordinator.GetComponentType<SkyLightComponent>()) ||
                sig.test(coordinator.GetComponentType<SpotLightComponent>())) {
                sceneHasLights = true;
                break;
            }
        }
    }

    if (!useEditorDefaultLighting && world && sceneHasLights) {
        auto& coordinator = world->getCoordinator();
        auto lightCollectionSys = coordinator.GetSystem<eng::runtime::LightCollectionSystem>();
        if (lightCollectionSys) {
            auto lightData = lightCollectionSys->CollectLights(coordinator);
            
            // Map directional light
            uboData.directionalDirectionIntensity = glm::vec4(lightData.directionalLight.direction, lightData.directionalLight.intensity);
            uboData.directionalColor = glm::vec4(lightData.directionalLight.color, 1.0f);
            
            // Map sky light
            uboData.ambientColorIntensity = glm::vec4(lightData.skyLight.color, lightData.skyLight.intensity);
            
            // Map point lights
            uboData.pointLightCount = static_cast<uint32_t>(lightData.pointLights.size());
            for (uint32_t i = 0; i < uboData.pointLightCount && i < 16; ++i) {
                const auto& pt = lightData.pointLights[i];
                uboData.pointPositionsRadius[i] = glm::vec4(pt.position, pt.radius);
                uboData.pointColorsIntensity[i] = glm::vec4(pt.color, pt.intensity);
            }

            // Map spot lights
            uboData.spotLightCount = static_cast<uint32_t>(lightData.spotLights.size());
            for (uint32_t i = 0; i < uboData.spotLightCount && i < 16; ++i) {
                const auto& sl = lightData.spotLights[i];
                uboData.spotPositionsRange[i] = glm::vec4(sl.position, sl.range);
                uboData.spotDirectionsIntensity[i] = glm::vec4(sl.direction, sl.intensity);
                uboData.spotColors[i] = glm::vec4(sl.color, std::cos(glm::radians(sl.innerConeAngle)));
                uboData.spotAngles[i] = glm::vec4(std::cos(glm::radians(sl.outerConeAngle)), 0.0f, 0.0f, 0.0f);
            }
        }
    }

    lastFallbackActive = useEditorDefaultLighting || !world || !sceneHasLights;
    if (!lastFallbackActive && world) {
        auto& coordinator = world->getCoordinator();
        if (!coordinator.GetSystem<eng::runtime::LightCollectionSystem>()) {
            lastFallbackActive = true;
        }
    }
}

} // namespace eng::renderer
