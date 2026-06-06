#include "Runtime/Private/Editor/Widgets/TransformWidget.h"
#include "Runtime/Public/Editor/EditorMath.h"
#include "ThirdParty/imgui/imgui.h"
#include <algorithm>

namespace eng::runtime {

    bool TransformWidget::Draw(TransformComponent& transform, EditorDirtyState& dirtyState, bool& outCommitted) {
        bool changed = false;
        outCommitted = false;

        // Position
        float pos[3] = { transform.position.x, transform.position.y, transform.position.z };
        if (ImGui::DragFloat3("Position", pos, 0.1f)) {
            Vector3 newPos(pos[0], pos[1], pos[2]);
            if (IsFiniteVec3(newPos)) {
                transform.position = newPos;
                transform.dirty = true;
                dirtyState.MarkSceneDirty();
                changed = true;
            }
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            outCommitted = true;
        }

        // Rotation (Euler degrees)
        Vector3 euler = QuaternionToEuler(transform.rotation);
        float rot[3] = { euler.x, euler.y, euler.z };
        if (ImGui::DragFloat3("Rotation", rot, 1.0f, -360.0f, 360.0f)) {
            Vector3 newRot(rot[0], rot[1], rot[2]);
            if (IsFiniteVec3(newRot)) {
                transform.rotation = EulerToQuaternion(newRot.x, newRot.y, newRot.z);
                transform.dirty = true;
                dirtyState.MarkSceneDirty();
                changed = true;
            }
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            outCommitted = true;
        }

        // Scale (clamped to 0.001f)
        float scl[3] = { transform.scale.x, transform.scale.y, transform.scale.z };
        if (ImGui::DragFloat3("Scale", scl, 0.1f)) {
            Vector3 newScale(std::max(scl[0], 0.001f), std::max(scl[1], 0.001f), std::max(scl[2], 0.001f));
            if (IsFiniteVec3(newScale)) {
                transform.scale = newScale;
                transform.dirty = true;
                dirtyState.MarkSceneDirty();
                changed = true;
            }
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            outCommitted = true;
        }

        // Reset and Copy/Paste Buttons
        ImGui::Spacing();
        if (ImGui::Button("Reset Transform")) {
            transform.position = Vector3(0.0f, 0.0f, 0.0f);
            transform.rotation = Quaternion(0.0f, 0.0f, 0.0f, 1.0f);
            transform.scale = Vector3(1.0f, 1.0f, 1.0f);
            transform.dirty = true;
            dirtyState.MarkSceneDirty();
            changed = true;
            outCommitted = true;
        }
        ImGui::SameLine();
        
        static Vector3 clipboardPos(0.0f, 0.0f, 0.0f);
        static Quaternion clipboardRot(0.0f, 0.0f, 0.0f, 1.0f);
        static Vector3 clipboardScale(1.0f, 1.0f, 1.0f);
        static bool hasClipboard = false;

        if (ImGui::Button("Copy")) {
            clipboardPos = transform.position;
            clipboardRot = transform.rotation;
            clipboardScale = transform.scale;
            hasClipboard = true;
        }
        ImGui::SameLine();
        if (!hasClipboard) {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button("Paste")) {
            transform.position = clipboardPos;
            transform.rotation = clipboardRot;
            transform.scale = clipboardScale;
            transform.dirty = true;
            dirtyState.MarkSceneDirty();
            changed = true;
            outCommitted = true;
        }
        if (!hasClipboard) {
            ImGui::EndDisabled();
        }

        return changed;
    }

} // namespace eng::runtime
