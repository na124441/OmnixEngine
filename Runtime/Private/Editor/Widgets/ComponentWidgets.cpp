#include "Runtime/Private/Editor/Widgets/ComponentWidgets.h"
#include "Runtime/Public/AssetRegistry.h"
#include "Runtime/Public/OmnixMaterialFormat.h"
#include "ThirdParty/imgui/imgui.h"
#include "Runtime/Public/RuntimeContext.h"
#include "RenderingEngine/Runtime/engine/EngineLoop.h"
#include "Rendering/Core/Renderer.h"
#include "Rendering/Geometry/GeometryTypes.h"
#include "RenderingEngine/Renderer/scene/Material.h"
#include "Runtime/Public/Audio/AudioSystem.h"
#include "PhysicsValidation.h"
#include <algorithm>
#include <cstring>
#include <cmath>

namespace eng::runtime {

    bool ComponentWidgets::DrawName(NameComponent& component, EditorDirtyState& dirtyState) {
        bool changed = false;
        char buffer[128];
        std::strncpy(buffer, component.name.c_str(), sizeof(buffer));
        buffer[sizeof(buffer) - 1] = '\0';
        if (ImGui::InputText("Entity Name", buffer, sizeof(buffer))) {
            component.name = buffer;
            dirtyState.MarkSceneDirty();
            changed = true;
        }
        return changed;
    }

    bool ComponentWidgets::DrawTag(TagComponent& component, EditorDirtyState& dirtyState) {
        bool changed = false;
        char buffer[128];
        std::strncpy(buffer, component.tag.c_str(), sizeof(buffer));
        buffer[sizeof(buffer) - 1] = '\0';
        if (ImGui::InputText("Entity Tag", buffer, sizeof(buffer))) {
            component.tag = buffer;
            dirtyState.MarkSceneDirty();
            changed = true;
        }
        return changed;
    }

    bool ComponentWidgets::DrawLayer(LayerComponent& component, EditorDirtyState& dirtyState) {
        bool changed = false;
        int layerVal = static_cast<int>(component.layer);
        if (ImGui::InputInt("Layer ID", &layerVal)) {
            component.layer = static_cast<uint32_t>(std::max(0, layerVal));
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        char buffer[128];
        std::strncpy(buffer, component.layerName.c_str(), sizeof(buffer));
        buffer[sizeof(buffer) - 1] = '\0';
        if (ImGui::InputText("Layer Name", buffer, sizeof(buffer))) {
            component.layerName = buffer;
            dirtyState.MarkSceneDirty();
            changed = true;
        }
        return changed;
    }

    bool ComponentWidgets::DrawHealth(HealthComponent& component, EditorDirtyState& dirtyState) {
        bool changed = false;
        if (ImGui::DragFloat("Current Health", &component.current, 1.0f, 0.0f, component.max)) {
            dirtyState.MarkSceneDirty();
            changed = true;
        }
        if (ImGui::DragFloat("Max Health", &component.max, 1.0f, 1.0f, 10000.0f)) {
            component.current = std::min(component.current, component.max);
            dirtyState.MarkSceneDirty();
            changed = true;
        }
        return changed;
    }

    bool ComponentWidgets::DrawRigidBody(RigidBodyComponent& component, EditorDirtyState& dirtyState) {
        bool changed = false;
        if (ImGui::DragFloat("Mass", &component.mass, 0.1f, 0.001f, 10000.0f)) {
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        float vel[3] = { component.velocity.x, component.velocity.y, component.velocity.z };
        if (ImGui::DragFloat3("Velocity", vel, 0.1f)) {
            component.velocity.x = vel[0];
            component.velocity.y = vel[1];
            component.velocity.z = vel[2];
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        float angVel[3] = { component.angularVelocity.x, component.angularVelocity.y, component.angularVelocity.z };
        if (ImGui::DragFloat3("Angular Velocity", angVel, 0.1f)) {
            component.angularVelocity.x = angVel[0];
            component.angularVelocity.y = angVel[1];
            component.angularVelocity.z = angVel[2];
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        if (ImGui::Checkbox("Use Gravity", &component.useGravity)) {
            dirtyState.MarkSceneDirty();
            changed = true;
        }
        if (ImGui::Checkbox("Is Kinematic", &component.isKinematic)) {
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        if (ImGui::DragFloat("Linear Drag", &component.drag, 0.01f, 0.0f, 1.0f)) {
            dirtyState.MarkSceneDirty();
            changed = true;
        }
        if (ImGui::DragFloat("Angular Drag", &component.angularDrag, 0.01f, 0.0f, 1.0f)) {
            dirtyState.MarkSceneDirty();
            changed = true;
        }
        return changed;
    }

    bool ComponentWidgets::DrawCollider(ColliderComponent& component, EditorDirtyState& dirtyState) {
        bool changed = false;
        const char* colliderTypes[] = { "Box", "Sphere", "Capsule", "Mesh" };
        int currentType = static_cast<int>(component.type);
        if (ImGui::Combo("Collider Type", &currentType, colliderTypes, 4)) {
            component.type = static_cast<ColliderType>(currentType);
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        float center[3] = { component.center.x, component.center.y, component.center.z };
        if (ImGui::DragFloat3("Center Offset", center, 0.1f)) {
            component.center.x = center[0];
            component.center.y = center[1];
            component.center.z = center[2];
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        if (component.type == ColliderType::Box) {
            float size[3] = { component.size.x, component.size.y, component.size.z };
            if (ImGui::DragFloat3("Box Size", size, 0.1f)) {
                component.size.x = std::max(size[0], 0.001f);
                component.size.y = std::max(size[1], 0.001f);
                component.size.z = std::max(size[2], 0.001f);
                dirtyState.MarkSceneDirty();
                changed = true;
            }
        } else if (component.type == ColliderType::Sphere || component.type == ColliderType::Capsule) {
            if (ImGui::DragFloat("Radius", &component.radius, 0.1f, 0.001f, 1000.0f)) {
                dirtyState.MarkSceneDirty();
                changed = true;
            }
            if (component.type == ColliderType::Capsule) {
                if (ImGui::DragFloat("Height", &component.height, 0.1f, 0.001f, 1000.0f)) {
                    dirtyState.MarkSceneDirty();
                    changed = true;
                }
            }
        }

        if (ImGui::Checkbox("Is Trigger", &component.isTrigger)) {
            dirtyState.MarkSceneDirty();
            changed = true;
        }
        return changed;
    }

    bool ComponentWidgets::DrawMeshRenderer(MeshRendererComponent& component, EditorDirtyState& dirtyState, RuntimeContext* context) {
        bool changed = false;
        if (ImGui::Checkbox("Visible", &component.visible)) {
            dirtyState.MarkSceneDirty();
            changed = true;
        }
        if (ImGui::Checkbox("Cast Shadows", &component.castShadows)) {
            dirtyState.MarkSceneDirty();
            changed = true;
        }
        if (ImGui::Checkbox("Receive Shadows", &component.receiveShadows)) {
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        ImGui::TextDisabled("Mesh Asset ID: %u", component.meshID);
        ImGui::TextDisabled("Material Asset ID: %u", component.materialID);

        // Determine and show routing path
        std::string routeStr = "ConventionalCPU (Default)";
        if (context && context->renderer) {
            auto* engineLoop = dynamic_cast<eng::runtime::EngineLoop*>(context->renderer);
            if (engineLoop) {
                auto* sceneRenderer = engineLoop->GetSceneRenderer();
                if (sceneRenderer) {
                    uint32_t activeTier = sceneRenderer->GetActiveCapabilityTier();
                    eng::renderer::GeometryRenderingPath route = eng::renderer::GeometryRenderingPath::ConventionalCPU;
                    if (activeTier >= 1) {
                        if (activeTier >= 2) {
                            route = eng::renderer::GeometryRenderingPath::VirtualGeometry;
                        } else {
                            route = eng::renderer::GeometryRenderingPath::ConventionalGPUDriven;
                        }
                    }
                    routeStr = eng::renderer::GeometryRenderingPathToString(route);
                }
            }
        }
        ImGui::Text("Geometry Route: %s", routeStr.c_str());
        return changed;
    }

    bool ComponentWidgets::DrawPlayerController(PlayerControllerComponent& component, EditorDirtyState& dirtyState) {
        bool changed = false;
        if (ImGui::DragFloat("Move Speed", &component.moveSpeed, 0.1f, 0.0f, 100.0f)) {
            dirtyState.MarkSceneDirty();
            changed = true;
        }
        if (ImGui::DragFloat("Look Sensitivity", &component.lookSensitivity, 0.01f, 0.0f, 10.0f)) {
            dirtyState.MarkSceneDirty();
            changed = true;
        }
        return changed;
    }

    bool ComponentWidgets::DrawStaticBody(StaticBodyComponent& component, EditorDirtyState& dirtyState) {
        bool changed = false;
        if (ImGui::Checkbox("Enabled", &component.enabled)) {
            dirtyState.MarkSceneDirty();
            changed = true;
        }
        int layerVal = static_cast<int>(component.collisionLayer);
        if (ImGui::InputInt("Collision Layer", &layerVal)) {
            component.collisionLayer = std::max(0, layerVal);
            dirtyState.MarkSceneDirty();
            changed = true;
        }
        int maskVal = static_cast<int>(component.collisionMask);
        if (ImGui::InputInt("Collision Mask", &maskVal)) {
            component.collisionMask = static_cast<uint32_t>(maskVal);
            dirtyState.MarkSceneDirty();
            changed = true;
        }
        if (changed) {
            eng::physics::ValidateStaticBody(component);
        }
        return changed;
    }

    bool ComponentWidgets::DrawBoxCollider(BoxColliderComponent& component, EditorDirtyState& dirtyState, bool& outCommitted) {
        bool changed = false;
        outCommitted = false;
        float size[3] = { component.size.x, component.size.y, component.size.z };
        if (ImGui::DragFloat3("Size", size, 0.1f)) {
            component.size.x = std::max(size[0], 0.01f);
            component.size.y = std::max(size[1], 0.01f);
            component.size.z = std::max(size[2], 0.01f);
            dirtyState.MarkSceneDirty();
            changed = true;
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            outCommitted = true;
        }
        float offset[3] = { component.offset.x, component.offset.y, component.offset.z };
        if (ImGui::DragFloat3("Offset", offset, 0.1f)) {
            component.offset.x = offset[0];
            component.offset.y = offset[1];
            component.offset.z = offset[2];
            dirtyState.MarkSceneDirty();
            changed = true;
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            outCommitted = true;
        }
        if (ImGui::Checkbox("Is Trigger", &component.isTrigger)) {
            dirtyState.MarkSceneDirty();
            changed = true;
            outCommitted = true;
        }
        if (ImGui::Checkbox("Debug Draw", &component.debugDraw)) {
            dirtyState.MarkSceneDirty();
            changed = true;
            outCommitted = true;
        }
        if (changed) {
            eng::physics::ValidateBoxCollider(component);
        }
        return changed;
    }

    bool ComponentWidgets::DrawSphereCollider(SphereColliderComponent& component, EditorDirtyState& dirtyState, bool& outCommitted) {
        bool changed = false;
        outCommitted = false;
        float radius = component.radius;
        if (ImGui::DragFloat("Radius", &radius, 0.1f)) {
            component.radius = std::max(radius, 0.01f);
            dirtyState.MarkSceneDirty();
            changed = true;
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            outCommitted = true;
        }
        float offset[3] = { component.offset.x, component.offset.y, component.offset.z };
        if (ImGui::DragFloat3("Offset", offset, 0.1f)) {
            component.offset.x = offset[0];
            component.offset.y = offset[1];
            component.offset.z = offset[2];
            dirtyState.MarkSceneDirty();
            changed = true;
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            outCommitted = true;
        }
        if (ImGui::Checkbox("Is Trigger", &component.isTrigger)) {
            dirtyState.MarkSceneDirty();
            changed = true;
            outCommitted = true;
        }
        if (ImGui::Checkbox("Debug Draw", &component.debugDraw)) {
            dirtyState.MarkSceneDirty();
            changed = true;
            outCommitted = true;
        }
        if (changed) {
            eng::physics::ValidateSphereCollider(component);
        }
        return changed;
    }

    bool ComponentWidgets::DrawCapsuleCollider(CapsuleColliderComponent& component, EditorDirtyState& dirtyState, bool& outCommitted) {
        bool changed = false;
        outCommitted = false;
        float radius = component.radius;
        if (ImGui::DragFloat("Radius", &radius, 0.1f)) {
            component.radius = std::max(radius, 0.01f);
            dirtyState.MarkSceneDirty();
            changed = true;
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            outCommitted = true;
        }
        float height = component.height;
        if (ImGui::DragFloat("Height", &height, 0.1f)) {
            component.height = std::max(height, 0.01f);
            dirtyState.MarkSceneDirty();
            changed = true;
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            outCommitted = true;
        }
        float offset[3] = { component.offset.x, component.offset.y, component.offset.z };
        if (ImGui::DragFloat3("Offset", offset, 0.1f)) {
            component.offset.x = offset[0];
            component.offset.y = offset[1];
            component.offset.z = offset[2];
            dirtyState.MarkSceneDirty();
            changed = true;
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            outCommitted = true;
        }
        if (ImGui::Checkbox("Is Trigger", &component.isTrigger)) {
            dirtyState.MarkSceneDirty();
            changed = true;
            outCommitted = true;
        }
        if (ImGui::Checkbox("Debug Draw", &component.debugDraw)) {
            dirtyState.MarkSceneDirty();
            changed = true;
            outCommitted = true;
        }
        if (changed) {
            eng::physics::ValidateCapsuleCollider(component);
        }
        return changed;
    }

} // namespace eng::runtime

#include <filesystem>

namespace eng::runtime {

    bool ComponentWidgets::DrawRenderableMesh(RenderableMeshComponent& component, AssetRegistry& registry, EditorDirtyState& dirtyState) {
        bool changed = false;
        ImGui::Text("Mesh Handle: %llu", component.meshAssetHandle.value);
        
        if (component.meshAssetHandle.IsValid()) {
            const AssetMetadata* meta = registry.GetMetadata(component.meshAssetHandle);
            if (meta) {
                ImGui::Text("Source Path: %s", meta->sourcePath.c_str());
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Mesh: <Missing Asset>");
                ImGui::TextDisabled("Warning: Assigned mesh handle was not found in AssetRegistry.");
            }
        } else {
            ImGui::TextDisabled("Mesh: <No Asset Assigned>");
        }

        if (ImGui::Button("Select Mesh...")) {
            ImGui::OpenPopup("MeshPickerPopup");
        }

        if (ImGui::BeginPopup("MeshPickerPopup")) {
            const auto& assets = registry.GetAssets();
            for (const auto& [handle, meta] : assets) {
                if (meta.type == AssetType::Mesh) {
                    std::string filename = std::filesystem::path(meta.sourcePath).filename().string();
                    if (filename.empty()) filename = "Mesh_" + std::to_string(handle.value);
                    
                    if (ImGui::MenuItem(filename.c_str())) {
                        component.meshAssetHandle = handle;
                        dirtyState.MarkSceneDirty();
                        changed = true;
                    }
                }
            }
            ImGui::EndPopup();
        }
        return changed;
    }

    bool ComponentWidgets::DrawMaterial(MaterialComponent& component, AssetRegistry& registry, EditorDirtyState& dirtyState, RuntimeContext* context) {
        bool changed = false;
        ImGui::Text("Material Handle: %llu", component.materialAssetHandle.value);
        
        if (component.materialAssetHandle.IsValid()) {
            const AssetMetadata* meta = registry.GetMetadata(component.materialAssetHandle);
            if (meta) {
                ImGui::Text("Source Path: %s", meta->sourcePath.c_str());

                // ---- PBR Factors & Settings Buffers -----------------------------
                static char albedoBuf[512] = {};
                static char normalBuf[512] = {};
                static char metallicRoughnessBuf[512] = {};
                static char aoBuf[512] = {};
                static char emissiveBuf[512] = {};
                static float baseColor[4] = {0.65f, 0.65f, 0.65f, 1.0f};
                static float metallic = 0.0f;
                static float roughness = 0.6f;
                static float normalScale = 1.0f;
                static float emissiveStrength = 1.0f;
                static float clearcoatFactor = 0.0f;
                static float clearcoatRoughness = 0.0f;
                static int blendMode = 0;
                static int shadingModel = 0;
                static uint64_t lastHandle = 0;

                // Bind to live Material if it is currently cached in the renderer
                eng::renderer::Material* liveMaterial = nullptr;
                if (context && context->renderer) {
                    auto* engineLoop = dynamic_cast<eng::runtime::EngineLoop*>(context->renderer);
                    if (engineLoop) {
                        auto* sceneRenderer = engineLoop->GetSceneRenderer();
                        if (sceneRenderer) {
                            auto it = sceneRenderer->m_EcsMaterialCache.find(component.materialAssetHandle.value);
                            if (it != sceneRenderer->m_EcsMaterialCache.end()) {
                                liveMaterial = it->second;
                            }
                        }
                    }
                }

                if (lastHandle != component.materialAssetHandle.value) {
                    lastHandle = component.materialAssetHandle.value;
                    OmnixMaterial mat;
                    if (DeserializeMaterial(mat, meta->sourcePath)) {
                        std::strncpy(albedoBuf, mat.albedoTexturePath.c_str(), sizeof(albedoBuf) - 1);
                        std::strncpy(normalBuf, mat.normalTexturePath.c_str(), sizeof(normalBuf) - 1);
                        std::strncpy(metallicRoughnessBuf, mat.metallicRoughnessTexturePath.c_str(), sizeof(metallicRoughnessBuf) - 1);
                        std::strncpy(aoBuf, mat.aoTexturePath.c_str(), sizeof(aoBuf) - 1);
                        std::strncpy(emissiveBuf, mat.emissiveTexturePath.c_str(), sizeof(emissiveBuf) - 1);
                        baseColor[0] = mat.baseColorFactor.r;
                        baseColor[1] = mat.baseColorFactor.g;
                        baseColor[2] = mat.baseColorFactor.b;
                        baseColor[3] = mat.baseColorFactor.a;
                        metallic = mat.metallicFactor;
                        roughness = mat.roughnessFactor;
                        clearcoatFactor = mat.clearcoatFactor;
                        clearcoatRoughness = mat.clearcoatRoughness;
                        normalScale = mat.normalScale;
                        emissiveStrength = mat.emissiveStrength;
                        blendMode = static_cast<int>(mat.blendMode);
                        shadingModel = static_cast<int>(mat.shadingModel);
                    } else {
                        albedoBuf[0] = '\0';
                        normalBuf[0] = '\0';
                        metallicRoughnessBuf[0] = '\0';
                        aoBuf[0] = '\0';
                        emissiveBuf[0] = '\0';
                        baseColor[0] = 0.65f; baseColor[1] = 0.65f; baseColor[2] = 0.65f; baseColor[3] = 1.0f;
                        metallic = 0.0f;
                        roughness = 0.6f;
                        clearcoatFactor = 0.0f;
                        clearcoatRoughness = 0.0f;
                        normalScale = 1.0f;
                        emissiveStrength = 1.0f;
                        blendMode = 0;
                        shadingModel = 0;
                    }
                }

                // ---- Mock Preview Thumbnail ----
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.0f, 1.0f), "Material Preview");
                ImGui::BeginGroup();
                ImVec4 previewCol(baseColor[0], baseColor[1], baseColor[2], baseColor[3]);
                if (emissiveStrength > 1.0f) {
                    previewCol.x *= 1.2f;
                    previewCol.y *= 1.2f;
                    previewCol.z *= 1.2f;
                }
                ImGui::ColorButton("##MaterialPreviewColor", previewCol, ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, ImVec2(64, 64));
                ImGui::SameLine();
                ImGui::BeginGroup();
                ImGui::Text("Shading: %s", (shadingModel == 0) ? "Lit PBR" : "Unlit");
                ImGui::Text("Blend: %s", (blendMode == 0) ? "Opaque" : (blendMode == 1 ? "Mask" : "Blend"));
                ImGui::Text("Roughness: %.2f  Metallic: %.2f", roughness, metallic);
                ImGui::EndGroup();
                ImGui::EndGroup();
                ImGui::Spacing();
                ImGui::Separator();

                // ---- PBR Factors ------------------------------------------------
                ImGui::TextColored(ImVec4(0.7f, 0.85f, 1.0f, 1.0f), "Material Factors");
                if (ImGui::ColorEdit4("Base Color Factor##MatBaseColor", baseColor)) {
                    if (liveMaterial) {
                        liveMaterial->setAlbedoColor(glm::vec4(baseColor[0], baseColor[1], baseColor[2], baseColor[3]));
                    }
                    dirtyState.MarkSceneDirty();
                    changed = true;
                }
                if (ImGui::SliderFloat("Metallic Factor##MatMetallic", &metallic, 0.0f, 1.0f)) {
                    if (liveMaterial) {
                        liveMaterial->setMetallic(metallic);
                    }
                    dirtyState.MarkSceneDirty();
                    changed = true;
                }
                if (ImGui::SliderFloat("Roughness Factor##MatRoughness", &roughness, 0.0f, 1.0f)) {
                    if (liveMaterial) {
                        liveMaterial->setRoughness(roughness);
                    }
                    dirtyState.MarkSceneDirty();
                    changed = true;
                }
                if (ImGui::SliderFloat("Clearcoat Factor##MatClearcoat", &clearcoatFactor, 0.0f, 1.0f)) {
                    if (liveMaterial) {
                        liveMaterial->setClearcoatFactor(clearcoatFactor);
                    }
                    dirtyState.MarkSceneDirty();
                    changed = true;
                }
                if (ImGui::SliderFloat("Clearcoat Roughness##MatClearcoatRough", &clearcoatRoughness, 0.0f, 1.0f)) {
                    if (liveMaterial) {
                        liveMaterial->setClearcoatRoughness(clearcoatRoughness);
                    }
                    dirtyState.MarkSceneDirty();
                    changed = true;
                }
                if (ImGui::SliderFloat("Normal Scale##MatNormalScale", &normalScale, 0.0f, 5.0f)) {
                    if (liveMaterial) {
                        liveMaterial->setNormalScale(normalScale);
                    }
                    dirtyState.MarkSceneDirty();
                    changed = true;
                }
                if (ImGui::DragFloat("Emissive Strength##MatEmissiveStr", &emissiveStrength, 0.1f, 0.0f, 100.0f)) {
                    if (liveMaterial) {
                        liveMaterial->setEmissiveStrength(emissiveStrength);
                    }
                    dirtyState.MarkSceneDirty();
                    changed = true;
                }

                const char* blendModes[] = { "Opaque", "Mask", "Blend" };
                if (ImGui::Combo("Blend Mode##MatBlendMode", &blendMode, blendModes, IM_ARRAYSIZE(blendModes))) {
                    if (liveMaterial) {
                        liveMaterial->setBlendMode(static_cast<eng::renderer::MaterialBlendMode>(blendMode));
                    }
                    dirtyState.MarkSceneDirty();
                    changed = true;
                }

                const char* shadingModels[] = { "Lit", "Unlit" };
                if (ImGui::Combo("Shading Model##MatShadingModel", &shadingModel, shadingModels, IM_ARRAYSIZE(shadingModels))) {
                    if (liveMaterial) {
                        liveMaterial->setShadingModel(static_cast<eng::renderer::MaterialShadingModel>(shadingModel));
                    }
                    dirtyState.MarkSceneDirty();
                    changed = true;
                }

                // ---- Texture path fields ----------------------------------------
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.7f, 0.85f, 1.0f, 1.0f), "PNG Texture Paths");
                ImGui::TextDisabled("Drop a PNG into Assets/Textures/ then type its path below.");
                ImGui::Separator();

                ImGui::InputText("Albedo PNG##MatAlbedo", albedoBuf, sizeof(albedoBuf));
                ImGui::InputText("Normal PNG##MatNormal", normalBuf, sizeof(normalBuf));
                ImGui::InputText("Metallic-Roughness PNG##MatMR", metallicRoughnessBuf, sizeof(metallicRoughnessBuf));
                ImGui::InputText("AO PNG##MatAO", aoBuf, sizeof(aoBuf));
                ImGui::InputText("Emissive PNG##MatEmissive", emissiveBuf, sizeof(emissiveBuf));
                ImGui::TextDisabled("Example: Assets/Textures/brick_albedo.png");

                if (ImGui::Button("Save Material Settings")) {
                    OmnixMaterial mat;
                    DeserializeMaterial(mat, meta->sourcePath); // load existing data
                    mat.albedoTexturePath = albedoBuf;
                    mat.normalTexturePath = normalBuf;
                    mat.metallicRoughnessTexturePath = metallicRoughnessBuf;
                    mat.aoTexturePath = aoBuf;
                    mat.emissiveTexturePath = emissiveBuf;
                    mat.baseColorFactor = glm::vec4(baseColor[0], baseColor[1], baseColor[2], baseColor[3]);
                    mat.metallicFactor = metallic;
                    mat.roughnessFactor = roughness;
                    mat.clearcoatFactor = clearcoatFactor;
                    mat.clearcoatRoughness = clearcoatRoughness;
                    mat.normalScale = normalScale;
                    mat.emissiveStrength = emissiveStrength;
                    mat.blendMode = static_cast<uint32_t>(blendMode);
                    mat.shadingModel = static_cast<uint32_t>(shadingModel);
                    if (SerializeMaterial(mat, meta->sourcePath)) {
                        dirtyState.MarkSceneDirty();
                        changed = true;
                        // Reset cached handle so paths are re-read next frame
                        lastHandle = 0;
                    }
                }
                ImGui::SameLine();
                ImGui::TextDisabled("(hot reloads instantly on save)");
                // -----------------------------------------------------------------

            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Material: <Missing Asset>");
                ImGui::TextDisabled("Warning: Assigned material handle was not found in AssetRegistry.");
            }
        } else {
            ImGui::TextDisabled("Material: <No Asset Assigned>");
        }

        if (ImGui::Button("Select Material...")) {
            ImGui::OpenPopup("MaterialPickerPopup");
        }

        if (ImGui::BeginPopup("MaterialPickerPopup")) {
            const auto& assets = registry.GetAssets();
            for (const auto& [handle, meta] : assets) {
                if (meta.type == AssetType::Material) {
                    std::string filename = std::filesystem::path(meta.sourcePath).filename().string();
                    if (filename.empty()) filename = "Material_" + std::to_string(handle.value);
                    
                    if (ImGui::MenuItem(filename.c_str())) {
                        component.materialAssetHandle = handle;
                        dirtyState.MarkSceneDirty();
                        changed = true;
                    }
                }
            }
            ImGui::EndPopup();
        }
        return changed;
    }

    bool ComponentWidgets::DrawPlayerStart(PlayerStartComponent& component, EditorDirtyState& dirtyState) {
        bool changed = false;
        if (ImGui::Checkbox("Active", &component.active)) {
            dirtyState.MarkSceneDirty();
            changed = true;
        }
        return changed;
    }

    bool ComponentWidgets::DrawCharacterController(CharacterControllerComponent& component, EditorDirtyState& dirtyState) {
        bool changed = false;
        if (ImGui::DragFloat("Move Speed", &component.moveSpeed, 0.1f, 0.0f, 100.0f)) {
            dirtyState.MarkSceneDirty();
            changed = true;
        }
        if (ImGui::DragFloat("Sprint Speed", &component.sprintSpeed, 0.1f, 0.0f, 100.0f)) {
            dirtyState.MarkSceneDirty();
            changed = true;
        }
        if (ImGui::DragFloat("Mouse Sensitivity", &component.mouseSensitivity, 0.01f, 0.0f, 10.0f)) {
            dirtyState.MarkSceneDirty();
            changed = true;
        }
        if (ImGui::DragFloat("Gravity", &component.gravity, 0.1f, -100.0f, 0.0f)) {
            dirtyState.MarkSceneDirty();
            changed = true;
        }
        if (ImGui::DragFloat("Jump Velocity", &component.jumpVelocity, 0.1f, 0.0f, 50.0f)) {
            dirtyState.MarkSceneDirty();
            changed = true;
        }
        if (ImGui::DragFloat("Capsule Radius", &component.capsuleRadius, 0.05f, 0.01f, 10.0f)) {
            dirtyState.MarkSceneDirty();
            changed = true;
        }
        if (ImGui::DragFloat("Capsule Height", &component.capsuleHeight, 0.05f, 0.01f, 10.0f)) {
            dirtyState.MarkSceneDirty();
            changed = true;
        }
        if (ImGui::DragFloat("Ground Check Distance", &component.groundCheckDistance, 0.01f, 0.0f, 5.0f)) {
            dirtyState.MarkSceneDirty();
            changed = true;
        }
        if (ImGui::DragFloat("Skin Width", &component.skinWidth, 0.005f, 0.0f, 1.0f)) {
            dirtyState.MarkSceneDirty();
            changed = true;
        }
        if (ImGui::Checkbox("Enable Jump", &component.enableJump)) {
            dirtyState.MarkSceneDirty();
            changed = true;
        }
        return changed;
    }

    bool ComponentWidgets::DrawCamera(CameraComponent& component, EditorDirtyState& dirtyState) {
        bool changed = false;
        
        float fovDeg = component.fov;
        if (ImGui::DragFloat("FOV", &fovDeg, 1.0f, 1.0f, 179.0f)) {
            component.fov = fovDeg;
            dirtyState.MarkSceneDirty();
            changed = true;
        }
        
        if (ImGui::DragFloat("Near Plane", &component.nearPlane, 0.05f, 0.001f, 10.0f)) {
            dirtyState.MarkSceneDirty();
            changed = true;
        }
        if (ImGui::DragFloat("Far Plane", &component.farPlane, 10.0f, 10.0f, 10000.0f)) {
            dirtyState.MarkSceneDirty();
            changed = true;
        }
        if (ImGui::Checkbox("Is Main Camera", &component.isPrimary)) {
            dirtyState.MarkSceneDirty();
            changed = true;
        }
        
        if (ImGui::DragFloat("Exposure", &component.exposure, 0.05f, 0.0f, 10.0f)) {
            dirtyState.MarkSceneDirty();
            changed = true;
        }
        
        float localOffset[3] = { component.localOffset.x, component.localOffset.y, component.localOffset.z };
        if (ImGui::DragFloat3("Local Offset", localOffset, 0.05f)) {
            component.localOffset.x = localOffset[0];
            component.localOffset.y = localOffset[1];
            component.localOffset.z = localOffset[2];
            dirtyState.MarkSceneDirty();
            changed = true;
        }
        
        return changed;
    }

    bool ComponentWidgets::DrawInput(InputComponent& component, EditorDirtyState& dirtyState) {
        bool changed = false;
        if (ImGui::Checkbox("Enabled", &component.enabled)) {
            dirtyState.MarkSceneDirty();
            changed = true;
        }
        return changed;
    }

    bool ComponentWidgets::DrawTrigger(TriggerComponent& component, EditorDirtyState& dirtyState) {
        bool changed = false;
        
        if (ImGui::Checkbox("Enabled##Trigger", &component.enabled)) {
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        // Shape Type Combo
        const char* shapes[] = { "Box", "Sphere", "Capsule" };
        int shapeIdx = static_cast<int>(component.shapeType);
        if (ImGui::Combo("Shape Type", &shapeIdx, shapes, 3)) {
            component.shapeType = static_cast<TriggerShapeType>(shapeIdx);
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        // Offset
        float offsetArr[3] = { component.offset.x, component.offset.y, component.offset.z };
        if (ImGui::DragFloat3("Offset##Trigger", offsetArr, 0.05f)) {
            component.offset = { offsetArr[0], offsetArr[1], offsetArr[2] };
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        // Dimensions based on shape
        if (component.shapeType == TriggerShapeType::Box) {
            float sizeArr[3] = { component.boxSize.x, component.boxSize.y, component.boxSize.z };
            if (ImGui::DragFloat3("Box Size", sizeArr, 0.05f)) {
                component.boxSize = { std::max(0.01f, sizeArr[0]), std::max(0.01f, sizeArr[1]), std::max(0.01f, sizeArr[2]) };
                dirtyState.MarkSceneDirty();
                changed = true;
            }
            if (component.boxSize.x < 0.01f || component.boxSize.y < 0.01f || component.boxSize.z < 0.01f) {
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Warning: Box Size axes must be >= 0.01");
            }
        } else if (component.shapeType == TriggerShapeType::Sphere) {
            float radius = component.sphereRadius;
            if (ImGui::DragFloat("Radius##SphereTrigger", &radius, 0.05f)) {
                component.sphereRadius = std::max(0.01f, radius);
                dirtyState.MarkSceneDirty();
                changed = true;
            }
            if (component.sphereRadius < 0.01f) {
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Warning: Radius must be >= 0.01");
            }
        } else if (component.shapeType == TriggerShapeType::Capsule) {
            float radius = component.capsuleRadius;
            float height = component.capsuleHeight;
            bool capsChanged = false;
            if (ImGui::DragFloat("Radius##CapsuleTrigger", &radius, 0.05f)) {
                component.capsuleRadius = std::max(0.01f, radius);
                capsChanged = true;
            }
            if (ImGui::DragFloat("Height##CapsuleTrigger", &height, 0.05f)) {
                component.capsuleHeight = std::max(0.01f, height);
                capsChanged = true;
            }
            if (capsChanged) {
                dirtyState.MarkSceneDirty();
                changed = true;
            }
            if (component.capsuleRadius < 0.01f) {
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Warning: Radius must be >= 0.01");
            }
            if (component.capsuleHeight < 2.0f * component.capsuleRadius) {
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Warning: Height must be >= 2 * Radius (%.2f)", 2.0f * component.capsuleRadius);
            }
        }

        ImGui::Separator();

        // Event settings
        char eventNameBuf[128];
        snprintf(eventNameBuf, sizeof(eventNameBuf), "%s", component.eventName.c_str());
        if (ImGui::InputText("Event Name", eventNameBuf, sizeof(eventNameBuf))) {
            component.eventName = eventNameBuf;
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        if (ImGui::Checkbox("Fire Enter", &component.fireEnter)) {
            dirtyState.MarkSceneDirty();
            changed = true;
        }
        ImGui::SameLine();
        if (ImGui::Checkbox("Fire Stay", &component.fireStay)) {
            dirtyState.MarkSceneDirty();
            changed = true;
        }
        ImGui::SameLine();
        if (ImGui::Checkbox("Fire Exit", &component.fireExit)) {
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        return changed;
    }

    bool ComponentWidgets::DrawInteractable(InteractableComponent& component, EditorDirtyState& dirtyState) {
        bool changed = false;

        if (ImGui::Checkbox("Enabled##Interactable", &component.Enabled)) {
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        char promptTextBuf[128];
        snprintf(promptTextBuf, sizeof(promptTextBuf), "%s", component.PromptText.c_str());
        if (ImGui::InputText("Prompt Text", promptTextBuf, sizeof(promptTextBuf))) {
            component.PromptText = promptTextBuf;
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        if (component.PromptText.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Warning: Prompt Text is empty!");
        }

        if (ImGui::DragFloat("Interaction Radius", &component.InteractionRadius, 0.1f, 0.0f, 100.0f, "%.1f")) {
            if (component.InteractionRadius < 0.1f) {
                component.InteractionRadius = 0.1f;
            }
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        const char* typeNames[] = { "None", "Use", "Pickup", "Talk", "Inspect", "Open", "Activate" };
        int currentType = static_cast<int>(component.Type);
        if (ImGui::Combo("Interaction Type", &currentType, typeNames, IM_ARRAYSIZE(typeNames))) {
            component.Type = static_cast<InteractionType>(currentType);
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        return changed;
    }

    bool ComponentWidgets::DrawObjective(ObjectiveComponent& component, EditorDirtyState& dirtyState) {
        bool changed = false;

        char idBuf[128];
        snprintf(idBuf, sizeof(idBuf), "%s", component.ObjectiveID.c_str());
        if (ImGui::InputText("Objective ID", idBuf, sizeof(idBuf))) {
            component.ObjectiveID = idBuf;
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        if (component.ObjectiveID.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Warning: Objective ID is empty!");
        }

        char titleBuf[128];
        snprintf(titleBuf, sizeof(titleBuf), "%s", component.Title.c_str());
        if (ImGui::InputText("Title", titleBuf, sizeof(titleBuf))) {
            component.Title = titleBuf;
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        char descBuf[256];
        snprintf(descBuf, sizeof(descBuf), "%s", component.Description.c_str());
        if (ImGui::InputText("Description", descBuf, sizeof(descBuf))) {
            component.Description = descBuf;
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        const char* modeNames[] = { "None", "Interaction", "TriggerEnter" };
        int currentMode = static_cast<int>(component.CompletionMode);
        if (ImGui::Combo("Completion Mode", &currentMode, modeNames, IM_ARRAYSIZE(modeNames))) {
            component.CompletionMode = static_cast<ObjectiveCompletionMode>(currentMode);
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        if (ImGui::Checkbox("Starts Active", &component.StartsActive)) {
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        if (ImGui::Checkbox("Repeatable", &component.Repeatable)) {
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        ImGui::BeginDisabled();
        bool completed = component.Completed;
        ImGui::Checkbox("Completed (Runtime)", &completed);
        ImGui::EndDisabled();

        return changed;
    }

    bool ComponentWidgets::DrawAudioSource(AudioSourceComponent& component, EditorDirtyState& dirtyState, AudioSystem* audioSys) {
        bool changed = false;

        char clipPathBuf[256];
        snprintf(clipPathBuf, sizeof(clipPathBuf), "%s", component.ClipPath.c_str());
        if (ImGui::InputText("Clip Path", clipPathBuf, sizeof(clipPathBuf))) {
            component.ClipPath = clipPathBuf;
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        if (component.ClipPath.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Warning: Clip Path is empty!");
        } else if (!std::filesystem::exists(component.ClipPath)) {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Warning: Audio file does not exist!");
        }

        if (ImGui::Checkbox("Play On Start", &component.PlayOnStart)) {
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        if (ImGui::Checkbox("Loop", &component.Loop)) {
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        if (ImGui::SliderFloat("Volume", &component.Volume, 0.0f, 1.0f, "%.2f")) {
            if (component.Volume < 0.0f) component.Volume = 0.0f;
            else if (component.Volume > 1.0f) component.Volume = 1.0f;
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        if (ImGui::Checkbox("Is Playing", &component.IsPlaying)) {
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        if (audioSys && !component.ClipPath.empty() && std::filesystem::exists(component.ClipPath)) {
            if (ImGui::Button("Test Play Preview")) {
                audioSys->PlayOneShot(component.ClipPath, component.Volume);
            }
            ImGui::SameLine();
            if (ImGui::Button("Stop All Preview")) {
                audioSys->StopAllSounds();
            }
        }

        return changed;
    }

    bool ComponentWidgets::DrawDirectionalLight(DirectionalLightComponent& component, EditorDirtyState& dirtyState) {
        bool changed = false;

        if (ImGui::Checkbox("Enabled##DirectionalLight", &component.enabled)) {
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        float colArr[3] = { component.color.x, component.color.y, component.color.z };
        if (ImGui::ColorEdit3("Color##DirectionalLight", colArr)) {
            component.color = { std::clamp(colArr[0], 0.0f, 1.0f), std::clamp(colArr[1], 0.0f, 1.0f), std::clamp(colArr[2], 0.0f, 1.0f) };
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        float intensity = component.intensity;
        if (ImGui::DragFloat("Intensity##DirectionalLight", &intensity, 0.05f, 0.0f, 20.0f)) {
            component.intensity = std::max(0.0f, intensity);
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        if (ImGui::Checkbox("Cast Shadows##DirectionalLight", &component.castShadows)) {
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        if (component.castShadows) {
            ImGui::Indent();

            float distance = component.shadowDistance;
            if (ImGui::SliderFloat("Shadow Distance##DirectionalLight", &distance, 10.0f, 200.0f, "%.1f")) {
                component.shadowDistance = distance;
                dirtyState.MarkSceneDirty();
                changed = true;
            }

            float bias = component.shadowBias;
            if (ImGui::SliderFloat("Shadow Bias##DirectionalLight", &bias, 0.0001f, 0.01f, "%.4f")) {
                component.shadowBias = bias;
                dirtyState.MarkSceneDirty();
                changed = true;
            }

            float slopeBias = component.shadowSlopeBias;
            if (ImGui::SliderFloat("Shadow Slope Bias##DirectionalLight", &slopeBias, 0.0001f, 0.02f, "%.4f")) {
                component.shadowSlopeBias = slopeBias;
                dirtyState.MarkSceneDirty();
                changed = true;
            }

            float normalBias = component.shadowNormalBias;
            if (ImGui::DragFloat("Normal Bias##DirectionalLight", &normalBias, 0.0001f, 0.0f, 0.1f, "%.4f")) {
                component.shadowNormalBias = normalBias;
                dirtyState.MarkSceneDirty();
                changed = true;
            }

            float strength = component.shadowStrength;
            if (ImGui::SliderFloat("Shadow Strength##DirectionalLight", &strength, 0.0f, 1.0f, "%.2f")) {
                component.shadowStrength = strength;
                dirtyState.MarkSceneDirty();
                changed = true;
            }

            int kernelSize = component.pcfKernelSize;
            if (ImGui::SliderInt("PCF Kernel Size##DirectionalLight", &kernelSize, 1, 9)) {
                if (kernelSize > 1 && kernelSize % 2 == 0) {
                    kernelSize += 1;
                }
                component.pcfKernelSize = kernelSize;
                dirtyState.MarkSceneDirty();
                changed = true;
            }

            int resolution = component.shadowResolution;
            const char* resOptions[] = { "512", "1024", "2048", "4096" };
            int currentResIdx = 2; // Default 2048
            if (resolution == 512) currentResIdx = 0;
            else if (resolution == 1024) currentResIdx = 1;
            else if (resolution == 2048) currentResIdx = 2;
            else if (resolution == 4096) currentResIdx = 3;

            if (ImGui::Combo("Shadow Res##DirectionalLight", &currentResIdx, resOptions, IM_ARRAYSIZE(resOptions))) {
                if (currentResIdx == 0) component.shadowResolution = 512;
                else if (currentResIdx == 1) component.shadowResolution = 1024;
                else if (currentResIdx == 2) component.shadowResolution = 2048;
                else if (currentResIdx == 3) component.shadowResolution = 4096;
                dirtyState.MarkSceneDirty();
                changed = true;
            }

            ImGui::Unindent();
        }

        return changed;
    }

    bool ComponentWidgets::DrawPointLight(PointLightComponent& component, EditorDirtyState& dirtyState) {
        bool changed = false;

        if (ImGui::Checkbox("Enabled##PointLight", &component.enabled)) {
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        float colArr[3] = { component.color.x, component.color.y, component.color.z };
        if (ImGui::ColorEdit3("Color##PointLight", colArr)) {
            component.color = { std::clamp(colArr[0], 0.0f, 1.0f), std::clamp(colArr[1], 0.0f, 1.0f), std::clamp(colArr[2], 0.0f, 1.0f) };
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        float intensity = component.intensity;
        if (ImGui::DragFloat("Intensity##PointLight", &intensity, 0.1f, 0.0f, 100.0f)) {
            component.intensity = std::max(0.0f, intensity);
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        float radius = component.radius;
        if (ImGui::DragFloat("Radius##PointLight", &radius, 0.05f, 0.01f, 100.0f)) {
            component.radius = std::max(0.01f, radius);
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        if (ImGui::Checkbox("Cast Shadows##PointLight", &component.castShadows)) {
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        return changed;
    }

    bool ComponentWidgets::DrawSkyLight(SkyLightComponent& component, EditorDirtyState& dirtyState) {
        bool changed = false;

        if (ImGui::Checkbox("Enabled##SkyLight", &component.enabled)) {
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        float colArr[3] = { component.color.x, component.color.y, component.color.z };
        if (ImGui::ColorEdit3("Color##SkyLight", colArr)) {
            component.color = { std::clamp(colArr[0], 0.0f, 1.0f), std::clamp(colArr[1], 0.0f, 1.0f), std::clamp(colArr[2], 0.0f, 1.0f) };
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        float intensity = component.intensity;
        if (ImGui::DragFloat("Intensity##SkyLight", &intensity, 0.02f, 0.0f, 5.0f)) {
            component.intensity = std::max(0.0f, intensity);
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        const char* modes[] = { "Procedural", "HDR Cubemap" };
        int currentMode = component.mode;
        if (ImGui::Combo("Mode##SkyLight", &currentMode, modes, IM_ARRAYSIZE(modes))) {
            component.mode = currentMode;
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        if (component.mode == 1) {
            char envBuf[512];
            strncpy(envBuf, component.environmentPath.c_str(), sizeof(envBuf));
            envBuf[sizeof(envBuf) - 1] = '\0';
            if (ImGui::InputText("HDR File Path##SkyLight", envBuf, sizeof(envBuf))) {
                component.environmentPath = envBuf;
                dirtyState.MarkSceneDirty();
                changed = true;
            }
        }

        float rot = component.rotation;
        if (ImGui::SliderFloat("Rotation##SkyLight", &rot, 0.0f, 360.0f)) {
            component.rotation = rot;
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        float diffInt = component.diffuseIntensity;
        if (ImGui::SliderFloat("Diffuse Intensity##SkyLight", &diffInt, 0.0f, 10.0f)) {
            component.diffuseIntensity = diffInt;
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        float specInt = component.specularIntensity;
        if (ImGui::SliderFloat("Specular Intensity##SkyLight", &specInt, 0.0f, 10.0f)) {
            component.specularIntensity = specInt;
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        float expOffset = component.exposureOffset;
        if (ImGui::SliderFloat("Exposure Offset##SkyLight", &expOffset, -10.0f, 10.0f)) {
            component.exposureOffset = expOffset;
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        return changed;
    }

    bool ComponentWidgets::DrawSpotLight(SpotLightComponent& component, EditorDirtyState& dirtyState) {
        bool changed = false;

        if (ImGui::Checkbox("Enabled##SpotLight", &component.enabled)) {
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        float colArr[3] = { component.color.x, component.color.y, component.color.z };
        if (ImGui::ColorEdit3("Color##SpotLight", colArr)) {
            component.color = { std::clamp(colArr[0], 0.0f, 1.0f), std::clamp(colArr[1], 0.0f, 1.0f), std::clamp(colArr[2], 0.0f, 1.0f) };
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        float intensity = component.intensity;
        if (ImGui::DragFloat("Intensity##SpotLight", &intensity, 0.1f, 0.0f, 100.0f)) {
            component.intensity = std::max(0.0f, intensity);
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        float range = component.range;
        if (ImGui::DragFloat("Range##SpotLight", &range, 0.05f, 0.01f, 100.0f)) {
            component.range = std::max(0.01f, range);
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        float inner = component.innerConeAngle;
        float outer = component.outerConeAngle;
        bool anglesChanged = false;
        if (ImGui::DragFloat("Inner Angle##SpotLight", &inner, 0.5f, 0.0f, 89.0f)) {
            component.innerConeAngle = std::clamp(inner, 0.0f, outer);
            anglesChanged = true;
        }
        if (ImGui::DragFloat("Outer Angle##SpotLight", &outer, 0.5f, 0.0f, 90.0f)) {
            component.outerConeAngle = std::max(inner, std::clamp(outer, 0.0f, 90.0f));
            anglesChanged = true;
        }

        if (anglesChanged) {
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        if (ImGui::Checkbox("Cast Shadows##SpotLight", &component.castShadows)) {
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        return changed;
    }

    bool ComponentWidgets::DrawSimpleState(SimpleStateComponent& component, EditorDirtyState& dirtyState) {
        bool changed = false;

        static const char* stateNames[] = { "Inactive", "Active", "Completed", "Locked", "Unlocked" };
        int currentInitial = static_cast<int>(component.InitialState);
        if (ImGui::Combo("Initial State", &currentInitial, stateNames, IM_ARRAYSIZE(stateNames))) {
            component.InitialState = static_cast<SimpleObjectState>(currentInitial);
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        ImGui::BeginDisabled();
        int currentCurrent = static_cast<int>(component.CurrentState);
        ImGui::Combo("Current State (Runtime)", &currentCurrent, stateNames, IM_ARRAYSIZE(stateNames));
        ImGui::EndDisabled();

        if (ImGui::Checkbox("Reset On Play", &component.ResetOnPlay)) {
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        return changed;
    }

    bool ComponentWidgets::DrawActivatable(ActivatableComponent& component, EditorDirtyState& dirtyState) {
        bool changed = false;

        char actIdBuf[128];
        snprintf(actIdBuf, sizeof(actIdBuf), "%s", component.ActivationID.c_str());
        if (ImGui::InputText("Activation ID", actIdBuf, sizeof(actIdBuf))) {
            component.ActivationID = actIdBuf;
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        char targetIdBuf[128];
        snprintf(targetIdBuf, sizeof(targetIdBuf), "%s", component.TargetActivationID.c_str());
        if (ImGui::InputText("Target Activation ID", targetIdBuf, sizeof(targetIdBuf))) {
            component.TargetActivationID = targetIdBuf;
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        if (ImGui::Checkbox("Requires Unlocked", &component.RequiresUnlocked)) {
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        if (ImGui::Checkbox("One Shot", &component.OneShot)) {
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        ImGui::BeginDisabled();
        bool hasActivated = component.HasActivated;
        ImGui::Checkbox("Has Activated (Runtime)", &hasActivated);
        ImGui::EndDisabled();

        return changed;
    }

    bool ComponentWidgets::DrawDoor(DoorComponent& component, EditorDirtyState& dirtyState) {
        bool changed = false;

        static const char* modeNames[] = { "Instant", "Smooth" };
        int currentMode = static_cast<int>(component.OpenMode);
        if (ImGui::Combo("Open Mode", &currentMode, modeNames, IM_ARRAYSIZE(modeNames))) {
            component.OpenMode = static_cast<DoorOpenMode>(currentMode);
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        float closedPos[3] = { component.ClosedPosition.x, component.ClosedPosition.y, component.ClosedPosition.z };
        if (ImGui::DragFloat3("Closed Position", closedPos, 0.05f)) {
            component.ClosedPosition = Vector3(closedPos[0], closedPos[1], closedPos[2]);
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        float openOffset[3] = { component.OpenOffset.x, component.OpenOffset.y, component.OpenOffset.z };
        if (ImGui::DragFloat3("Open Offset", openOffset, 0.05f)) {
            component.OpenOffset = Vector3(openOffset[0], openOffset[1], openOffset[2]);
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        if (ImGui::DragFloat("Open Speed", &component.OpenSpeed, 0.05f, 0.01f, 100.0f)) {
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        ImGui::BeginDisabled();
        bool isOpen = component.IsOpen;
        ImGui::Checkbox("Is Open (Runtime)", &isOpen);
        ImGui::EndDisabled();

        return changed;
    }

    bool ComponentWidgets::DrawCheckpoint(CheckpointComponent& component, EditorDirtyState& dirtyState) {
        bool changed = false;

        char cpIdBuf[128];
        snprintf(cpIdBuf, sizeof(cpIdBuf), "%s", component.CheckpointID.c_str());
        if (ImGui::InputText("Checkpoint ID", cpIdBuf, sizeof(cpIdBuf))) {
            component.CheckpointID = cpIdBuf;
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        if (component.CheckpointID.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Warning: Checkpoint ID is empty!");
        }

        char cpNameBuf[128];
        snprintf(cpNameBuf, sizeof(cpNameBuf), "%s", component.CheckpointName.c_str());
        if (ImGui::InputText("Checkpoint Name", cpNameBuf, sizeof(cpNameBuf))) {
            component.CheckpointName = cpNameBuf;
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        if (component.CheckpointName.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Warning: Checkpoint Name is empty!");
        }

        if (ImGui::Checkbox("Activate On Trigger Enter", &component.ActivateOnTriggerEnter)) {
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        if (ImGui::Checkbox("One Shot", &component.OneShot)) {
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        bool hasActivated = component.HasActivated;
        ImGui::BeginDisabled();
        ImGui::Checkbox("Has Activated", &hasActivated);
        ImGui::EndDisabled();

        return changed;
    }

    bool ComponentWidgets::DrawBounds(BoundsComponent& component, EditorDirtyState& dirtyState) {
        bool changed = false;

        // Dirty status badge
        if (component.dirty) {
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.1f, 1.0f), "[Dirty — pending update]");
        } else {
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "[Up-to-date]");
        }

        ImGui::Spacing();
        ImGui::Text("Local AABB:");
        float lMin[3] = { component.localMin.x, component.localMin.y, component.localMin.z };
        float lMax[3] = { component.localMax.x, component.localMax.y, component.localMax.z };
        ImGui::BeginDisabled();
        ImGui::DragFloat3("Local Min##Bounds", lMin, 0.01f);
        ImGui::DragFloat3("Local Max##Bounds", lMax, 0.01f);
        ImGui::EndDisabled();

        ImGui::Spacing();
        ImGui::Text("World AABB:");
        float wMin[3] = { component.worldMin.x, component.worldMin.y, component.worldMin.z };
        float wMax[3] = { component.worldMax.x, component.worldMax.y, component.worldMax.z };
        ImGui::BeginDisabled();
        ImGui::DragFloat3("World Min##Bounds", wMin, 0.01f);
        ImGui::DragFloat3("World Max##Bounds", wMax, 0.01f);
        ImGui::EndDisabled();

        ImGui::Spacing();
        if (ImGui::Checkbox("Has Bounding Sphere", &component.hasSphere)) {
            component.dirty = true;
            dirtyState.MarkSceneDirty();
            changed = true;
        }

        if (component.hasSphere) {
            float sc[3] = { component.sphereCenter.x, component.sphereCenter.y, component.sphereCenter.z };
            if (ImGui::DragFloat3("Sphere Center (local)", sc, 0.01f)) {
                component.sphereCenter = { sc[0], sc[1], sc[2] };
                component.dirty = true;
                dirtyState.MarkSceneDirty();
                changed = true;
            }
            if (ImGui::DragFloat("Sphere Radius (local)", &component.sphereRadius, 0.01f, 0.001f, 1000.0f)) {
                component.dirty = true;
                dirtyState.MarkSceneDirty();
                changed = true;
            }
            ImGui::Spacing();
            ImGui::Text("World Sphere:");
            float wsc[3] = { component.worldSphereCenter.x, component.worldSphereCenter.y, component.worldSphereCenter.z };
            ImGui::BeginDisabled();
            ImGui::DragFloat3("World Center##Sphere", wsc, 0.01f);
            ImGui::DragFloat("World Radius##Sphere", &component.worldSphereRadius, 0.01f);
            ImGui::EndDisabled();
        }

        ImGui::Spacing();
        if (ImGui::Button("Force Refresh Bounds")) {
            component.dirty = true;
            changed = true;
        }

        return changed;
    }

} // namespace eng::runtime
