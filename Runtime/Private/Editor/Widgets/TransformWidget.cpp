#include "Runtime/Private/Editor/Widgets/TransformWidget.h"
#include "Runtime/Public/Editor/EditorMath.h"
#include "ThirdParty/imgui/imgui.h"
#include <algorithm>

namespace eng::runtime {

    bool TransformWidget::Draw(TransformComponent& transform, EditorDirtyState& dirtyState) {
        bool changed = false;

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

        return changed;
    }

} // namespace eng::runtime
