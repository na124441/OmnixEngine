#include "Runtime/Private/Editor/Widgets/ComponentWidgets.h"
#include "Runtime/Public/AssetRegistry.h"
#include "ThirdParty/imgui/imgui.h"
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

    bool ComponentWidgets::DrawMeshRenderer(MeshRendererComponent& component, EditorDirtyState& dirtyState) {
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

    bool ComponentWidgets::DrawBoxCollider(BoxColliderComponent& component, EditorDirtyState& dirtyState) {
        bool changed = false;
        float size[3] = { component.size.x, component.size.y, component.size.z };
        if (ImGui::DragFloat3("Size", size, 0.1f)) {
            component.size.x = size[0];
            component.size.y = size[1];
            component.size.z = size[2];
            dirtyState.MarkSceneDirty();
            changed = true;
        }
        float offset[3] = { component.offset.x, component.offset.y, component.offset.z };
        if (ImGui::DragFloat3("Offset", offset, 0.1f)) {
            component.offset.x = offset[0];
            component.offset.y = offset[1];
            component.offset.z = offset[2];
            dirtyState.MarkSceneDirty();
            changed = true;
        }
        if (ImGui::Checkbox("Is Trigger", &component.isTrigger)) {
            dirtyState.MarkSceneDirty();
            changed = true;
        }
        if (ImGui::Checkbox("Debug Draw", &component.debugDraw)) {
            dirtyState.MarkSceneDirty();
            changed = true;
        }
        if (changed) {
            eng::physics::ValidateBoxCollider(component);
        }
        return changed;
    }

    bool ComponentWidgets::DrawSphereCollider(SphereColliderComponent& component, EditorDirtyState& dirtyState) {
        bool changed = false;
        if (ImGui::DragFloat("Radius", &component.radius, 0.1f)) {
            dirtyState.MarkSceneDirty();
            changed = true;
        }
        float offset[3] = { component.offset.x, component.offset.y, component.offset.z };
        if (ImGui::DragFloat3("Offset", offset, 0.1f)) {
            component.offset.x = offset[0];
            component.offset.y = offset[1];
            component.offset.z = offset[2];
            dirtyState.MarkSceneDirty();
            changed = true;
        }
        if (ImGui::Checkbox("Is Trigger", &component.isTrigger)) {
            dirtyState.MarkSceneDirty();
            changed = true;
        }
        if (ImGui::Checkbox("Debug Draw", &component.debugDraw)) {
            dirtyState.MarkSceneDirty();
            changed = true;
        }
        if (changed) {
            eng::physics::ValidateSphereCollider(component);
        }
        return changed;
    }

    bool ComponentWidgets::DrawCapsuleCollider(CapsuleColliderComponent& component, EditorDirtyState& dirtyState) {
        bool changed = false;
        if (ImGui::DragFloat("Radius", &component.radius, 0.1f)) {
            dirtyState.MarkSceneDirty();
            changed = true;
        }
        if (ImGui::DragFloat("Height", &component.height, 0.1f)) {
            dirtyState.MarkSceneDirty();
            changed = true;
        }
        float offset[3] = { component.offset.x, component.offset.y, component.offset.z };
        if (ImGui::DragFloat3("Offset", offset, 0.1f)) {
            component.offset.x = offset[0];
            component.offset.y = offset[1];
            component.offset.z = offset[2];
            dirtyState.MarkSceneDirty();
            changed = true;
        }
        if (ImGui::Checkbox("Is Trigger", &component.isTrigger)) {
            dirtyState.MarkSceneDirty();
            changed = true;
        }
        if (ImGui::Checkbox("Debug Draw", &component.debugDraw)) {
            dirtyState.MarkSceneDirty();
            changed = true;
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

    bool ComponentWidgets::DrawMaterial(MaterialComponent& component, AssetRegistry& registry, EditorDirtyState& dirtyState) {
        bool changed = false;
        ImGui::Text("Material Handle: %llu", component.materialAssetHandle.value);
        
        if (component.materialAssetHandle.IsValid()) {
            const AssetMetadata* meta = registry.GetMetadata(component.materialAssetHandle);
            if (meta) {
                ImGui::Text("Source Path: %s", meta->sourcePath.c_str());
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

} // namespace eng::runtime
