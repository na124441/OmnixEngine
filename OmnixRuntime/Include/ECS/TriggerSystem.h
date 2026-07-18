//============================================================================
// TriggerSystem.h - Trigger Volume Simulation System
//============================================================================

#pragma once

#include "ECS/SystemManager.h"
#include "ECS/ECSconfig.h"
#include "ECS/ECSComponents.h"
#include "ECS/Coordinator.h"
#include "Runtime/RuntimeContext.h"
#include "Runtime/World/ZoneEntityComponent.h"
#include "ECS/Public/IECSWorld.h"
#include "EventManagement/PhysicsEventTypes.h"
#include "EventManagement/EventManager.h"
#include "Gameplay/GameplayEvent.h"
#include "Gameplay/GameplayEventBus.h"
#include <unordered_set>
#include <vector>
#include <string>
#include <iostream>
#include <algorithm>
#include <cmath>

namespace eng::runtime {

struct TriggerPair {
    Entity triggerEntity;
    Entity otherEntity;

    bool operator==(const TriggerPair& other) const {
        return triggerEntity == other.triggerEntity && otherEntity == other.otherEntity;
    }
};

} // namespace eng::runtime

namespace std {
    template<>
    struct hash<eng::runtime::TriggerPair> {
        size_t operator()(const eng::runtime::TriggerPair& p) const {
            return (hash<uint32_t>()(p.triggerEntity) ^ (hash<uint32_t>()(p.otherEntity) << 1));
        }
    };
}

namespace eng::runtime {

class TriggerSystem : public System {
public:
    TriggerSystem() = default;

    bool IsTriggerActive(Entity triggerEntity) const {
        for (const auto& pair : m_CurrentOverlaps) {
            if (pair.triggerEntity == triggerEntity) {
                return true;
            }
        }
        return false;
    }

    bool IsDimensionsValid(const TriggerComponent& comp) const {
        if (comp.shapeType == TriggerShapeType::Box) {
            return comp.boxSize.x >= 0.01f && comp.boxSize.y >= 0.01f && comp.boxSize.z >= 0.01f;
        } else if (comp.shapeType == TriggerShapeType::Sphere) {
            return comp.sphereRadius >= 0.01f;
        } else if (comp.shapeType == TriggerShapeType::Capsule) {
            return comp.capsuleRadius >= 0.01f && comp.capsuleHeight >= 2.0f * comp.capsuleRadius;
        }
        return false;
    }

    const std::unordered_set<TriggerPair>& GetCurrentOverlaps() const {
        return m_CurrentOverlaps;
    }

    void ClearOverlaps() {
        m_PreviousOverlaps.clear();
        m_CurrentOverlaps.clear();
    }

    void FixedUpdate(RuntimeContext& context, float fixedDeltaTime) {
        // 1. Verify if we are in Play mode.
        bool isPlayMode = (context.mode == RuntimeMode::Game) ||
                          (context.mode == RuntimeMode::Editor && context.editorSimulationState == EditorSimulationState::Play);

        if (!isPlayMode) {
            ClearOverlaps();
            return;
        }

        Coordinator& coordinator = context.ecs->getCoordinator();

        // 2. Identify the player controller entity (or entities)
        std::vector<Entity> playerEntities;
        for (Entity ent : coordinator.GetActiveEntities()) {
            auto sig = coordinator.GetSignature(ent);
            if (sig.test(coordinator.GetComponentType<CharacterControllerComponent>()) &&
                sig.test(coordinator.GetComponentType<TransformComponent>())) {
                playerEntities.push_back(ent);
            }
        }

        // Shift current overlaps to previous
        m_PreviousOverlaps = m_CurrentOverlaps;
        m_CurrentOverlaps.clear();

        // 3. Check overlaps for all active trigger volumes
        for (Entity trigEnt : m_Entities) {
            if (!coordinator.IsEntityAlive(trigEnt)) continue;

            // Check if ZoneEntityComponent is attached and simulating is false
            auto signature = coordinator.GetSignature(trigEnt);
            if (signature.test(coordinator.GetComponentType<eng::runtime::ZoneEntityComponent>())) {
                const auto& zec = coordinator.GetComponent<eng::runtime::ZoneEntityComponent>(trigEnt);
                if (!zec.simulating) {
                    continue;
                }
            }

            const auto& trigComp = coordinator.GetComponent<TriggerComponent>(trigEnt);
            if (!trigComp.enabled || !IsDimensionsValid(trigComp)) {
                continue;
            }

            const auto& trigTrans = coordinator.GetComponent<TransformComponent>(trigEnt);

            for (Entity playEnt : playerEntities) {
                const auto& playTrans = coordinator.GetComponent<TransformComponent>(playEnt);
                const auto& playCCC = coordinator.GetComponent<CharacterControllerComponent>(playEnt);

                if (CheckOverlap(trigTrans, trigComp, playTrans, playCCC)) {
                    m_CurrentOverlaps.insert(TriggerPair{ trigEnt, playEnt });
                }
            }
        }

        // 4. Trace changes (Enter, Stay, Exit)
        for (const auto& pair : m_CurrentOverlaps) {
            bool wasOverlapping = (m_PreviousOverlaps.find(pair) != m_PreviousOverlaps.end());
            const auto& trigComp = coordinator.GetComponent<TriggerComponent>(pair.triggerEntity);

            if (!wasOverlapping) {
                // Enter event!
                if (trigComp.fireEnter) {
                    if (context.events) {
                        context.events->queueEvent(std::make_unique<Omnix::TriggerEnterEvent>(
                            pair.triggerEntity, pair.otherEntity, trigComp.eventName));
                    }
                    if (context.gameplayEventBus) {
                        eng::runtime::GameplayEvent gpEvent;
                        gpEvent.Type = eng::runtime::GameplayEventType::TriggerEnter;
                        gpEvent.Source = pair.otherEntity;
                        gpEvent.Target = pair.triggerEntity;
                        gpEvent.ObjectiveID = trigComp.eventName;
                        context.gameplayEventBus->QueueEvent(gpEvent);
                    }
                    std::cout << "[TriggerSystem] Enter overlap: Trigger entity " << pair.triggerEntity 
                              << " overlapped by Player entity " << pair.otherEntity 
                              << " (Event: " << trigComp.eventName << ")" << std::endl;
                }


            } else {
                // Stay event!
                if (trigComp.fireStay) {
                    if (context.events) {
                        context.events->queueEvent(std::make_unique<Omnix::TriggerStayEvent>(
                            pair.triggerEntity, pair.otherEntity, trigComp.eventName));
                    }
                }
            }
        }

        for (const auto& pair : m_PreviousOverlaps) {
            bool isStillOverlapping = (m_CurrentOverlaps.find(pair) != m_CurrentOverlaps.end());
            if (!isStillOverlapping) {
                // Exit event!
                // Check if trigger entity is still alive before accessing it
                if (coordinator.IsEntityAlive(pair.triggerEntity)) {
                    const auto& trigComp = coordinator.GetComponent<TriggerComponent>(pair.triggerEntity);
                    if (trigComp.fireExit) {
                        if (context.events) {
                            context.events->queueEvent(std::make_unique<Omnix::TriggerExitEvent>(
                                pair.triggerEntity, pair.otherEntity, trigComp.eventName));
                        }
                        if (context.gameplayEventBus) {
                            eng::runtime::GameplayEvent gpEvent;
                            gpEvent.Type = eng::runtime::GameplayEventType::TriggerExit;
                            gpEvent.Source = pair.otherEntity;
                            gpEvent.Target = pair.triggerEntity;
                            gpEvent.ObjectiveID = trigComp.eventName;
                            context.gameplayEventBus->QueueEvent(gpEvent);
                        }
                        std::cout << "[TriggerSystem] Exit overlap: Trigger entity " << pair.triggerEntity 
                                  << " exited by Player entity " << pair.otherEntity 
                                  << " (Event: " << trigComp.eventName << ")" << std::endl;
                    }
                }
            }
        }
    }

    std::shared_ptr<System> Clone() const override {
        auto clone = std::make_shared<TriggerSystem>();
        clone->m_Entities = this->m_Entities;
        clone->m_PreviousOverlaps = this->m_PreviousOverlaps;
        clone->m_CurrentOverlaps = this->m_CurrentOverlaps;
        return clone;
    }

private:
    std::unordered_set<TriggerPair> m_PreviousOverlaps;
    std::unordered_set<TriggerPair> m_CurrentOverlaps;

    // Helper functions for geometric overlap
    static Vector3 ClosestPointOnSegment(const Vector3& A, const Vector3& B, const Vector3& Q) {
        Vector3 AB = { B.x - A.x, B.y - A.y, B.z - A.z };
        Vector3 AQ = { Q.x - A.x, Q.y - A.y, Q.z - A.z };
        float abLenSq = AB.x * AB.x + AB.y * AB.y + AB.z * AB.z;
        if (abLenSq < 1e-6f) return A;
        float t = (AQ.x * AB.x + AQ.y * AB.y + AQ.z * AB.z) / abLenSq;
        t = std::max(0.0f, std::min(1.0f, t));
        return { A.x + t * AB.x, A.y + t * AB.y, A.z + t * AB.z };
    }

    static bool OverlapSphereCapsule(const Vector3& sphereCenter, float sphereRadius, const Vector3& capA, const Vector3& capB, float capRadius) {
        Vector3 closest = ClosestPointOnSegment(capA, capB, sphereCenter);
        float dx = sphereCenter.x - closest.x;
        float dy = sphereCenter.y - closest.y;
        float dz = sphereCenter.z - closest.z;
        float distSq = dx * dx + dy * dy + dz * dz;
        float sumRadius = sphereRadius + capRadius;
        return distSq <= sumRadius * sumRadius;
    }

    static float SegmentSegmentDistanceSq(const Vector3& p1, const Vector3& q1, const Vector3& p2, const Vector3& q2, Vector3& c1, Vector3& c2) {
        Vector3 d1 = { q1.x - p1.x, q1.y - p1.y, q1.z - p1.z };
        Vector3 d2 = { q2.x - p2.x, q2.y - p2.y, q2.z - p2.z };
        Vector3 r = { p1.x - p2.x, p1.y - p2.y, p1.z - p2.z };
        float a = d1.x * d1.x + d1.y * d1.y + d1.z * d1.z;
        float e = d2.x * d2.x + d2.y * d2.y + d2.z * d2.z;
        float f = d2.x * r.x + d2.y * r.y + d2.z * r.z;

        float s = 0.0f, t = 0.0f;

        if (a <= 1e-6f && e <= 1e-6f) {
            c1 = p1;
            c2 = p2;
            Vector3 diff = { c1.x - c2.x, c1.y - c2.y, c1.z - c2.z };
            return diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
        }
        if (a <= 1e-6f) {
            s = 0.0f;
            t = f / e;
            t = std::max(0.0f, std::min(1.0f, t));
        } else {
            float c = d1.x * r.x + d1.y * r.y + d1.z * r.z;
            if (e <= 1e-6f) {
                t = 0.0f;
                s = -c / a;
                s = std::max(0.0f, std::min(1.0f, s));
            } else {
                float b = d1.x * d2.x + d1.y * d2.y + d1.z * d2.z;
                float denom = a * e - b * b;
                if (denom != 0.0f) {
                    s = std::max(0.0f, std::min(1.0f, (b * f - c * e) / denom));
                } else {
                    s = 0.0f;
                }
                t = (b * s + f) / e;
                if (t < 0.0f) {
                    t = 0.0f;
                    s = std::max(0.0f, std::min(1.0f, -c / a));
                } else if (t > 1.0f) {
                    t = 1.0f;
                    s = std::max(0.0f, std::min(1.0f, (b - c) / a));
                }
            }
        }

        c1 = { p1.x + s * d1.x, p1.y + s * d1.y, p1.z + s * d1.z };
        c2 = { p2.x + t * d2.x, p2.y + t * d2.y, p2.z + t * d2.z };
        Vector3 diff = { c1.x - c2.x, c1.y - c2.y, c1.z - c2.z };
        return diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
    }

    static bool OverlapCapsuleCapsule(const Vector3& cap1A, const Vector3& cap1B, float cap1Radius, const Vector3& cap2A, const Vector3& cap2B, float cap2Radius) {
        Vector3 c1, c2;
        float distSq = SegmentSegmentDistanceSq(cap1A, cap1B, cap2A, cap2B, c1, c2);
        float sumRadius = cap1Radius + cap2Radius;
        return distSq <= sumRadius * sumRadius;
    }

    static Vector3 TransformPoint(const Matrix4x4& mat, const Vector3& p) {
        float x = mat.m[0]*p.x + mat.m[4]*p.y + mat.m[8]*p.z + mat.m[12];
        float y = mat.m[1]*p.x + mat.m[5]*p.y + mat.m[9]*p.z + mat.m[13];
        float z = mat.m[2]*p.x + mat.m[6]*p.y + mat.m[10]*p.z + mat.m[14];
        float w = mat.m[3]*p.x + mat.m[7]*p.y + mat.m[11]*p.z + mat.m[15];
        if (std::abs(w) > 1e-6f) {
            x /= w; y /= w; z /= w;
        }
        return { x, y, z };
    }

    static Matrix4x4 MatrixInverse(const Matrix4x4& mat) {
        Matrix4x4 inv;
        const float* m = mat.m;
        float* out = inv.m;

        float a0 = m[0]*m[5] - m[1]*m[4];
        float a1 = m[0]*m[6] - m[2]*m[4];
        float a2 = m[0]*m[7] - m[3]*m[4];
        float a3 = m[1]*m[6] - m[2]*m[5];
        float a4 = m[1]*m[7] - m[3]*m[5];
        float a5 = m[2]*m[7] - m[3]*m[6];
        float b0 = m[8]*m[13] - m[9]*m[12];
        float b1 = m[8]*m[14] - m[10]*m[12];
        float b2 = m[8]*m[15] - m[11]*m[12];
        float b3 = m[9]*m[14] - m[10]*m[13];
        float b4 = m[9]*m[15] - m[11]*m[13];
        float b5 = m[10]*m[15] - m[11]*m[14];

        float det = a0*b5 - a1*b4 + a2*b3 + a3*b2 - a4*b1 + a5*b0;
        if (std::abs(det) < 1e-6f) {
            inv.SetIdentity();
            return inv;
        }

        float invDet = 1.0f / det;

        out[0] = (m[5]*b5 - m[6]*b4 + m[7]*b3) * invDet;
        out[1] = (-m[1]*b5 + m[2]*b4 - m[3]*b3) * invDet;
        out[2] = (m[13]*a5 - m[14]*a4 + m[15]*a3) * invDet;
        out[3] = (-m[9]*a5 + m[10]*a4 - m[11]*a3) * invDet;

        out[4] = (-m[4]*b5 + m[6]*b2 - m[7]*b1) * invDet;
        out[5] = (m[0]*b5 - m[2]*b2 + m[3]*b1) * invDet;
        out[6] = (-m[12]*a5 + m[14]*a2 - m[15]*a1) * invDet;
        out[7] = (m[8]*a5 - m[10]*a2 + m[11]*a1) * invDet;

        out[8] = (m[4]*b4 - m[5]*b2 + m[7]*b0) * invDet;
        out[9] = (-m[0]*b4 + m[1]*b2 - m[3]*b0) * invDet;
        out[10] = (m[12]*a4 - m[13]*a2 + m[15]*a0) * invDet;
        out[11] = (-m[8]*a4 + m[9]*a2 - m[11]*a0) * invDet;

        out[12] = (-m[4]*b3 + m[5]*b1 - m[6]*b0) * invDet;
        out[13] = (m[0]*b3 - m[1]*b1 + m[2]*b0) * invDet;
        out[14] = (-m[12]*a3 + m[13]*a1 - m[14]*a0) * invDet;
        out[15] = (m[8]*a3 - m[9]*a1 + m[10]*a0) * invDet;

        return inv;
    }

    static bool CheckOverlap(const TransformComponent& trigTrans, const TriggerComponent& trig, const TransformComponent& playTrans, const CharacterControllerComponent& ccc) {
        float playRadius = ccc.capsuleRadius;
        float playHeight = ccc.capsuleHeight;
        float playHalfH = (playHeight - 2.0f * playRadius) * 0.5f;
        if (playHalfH < 0.001f) playHalfH = 0.001f;

        Vector3 playA_world = { playTrans.position.x, playTrans.position.y + playRadius, playTrans.position.z };
        Vector3 playB_world = { playTrans.position.x, playTrans.position.y + playHeight - playRadius, playTrans.position.z };

        if (trig.shapeType == TriggerShapeType::Sphere) {
            Vector3 sphereCenter = TransformPoint(trigTrans.worldMatrix, trig.offset);
            float scaleMax = std::max({ std::abs(trigTrans.scale.x), std::abs(trigTrans.scale.y), std::abs(trigTrans.scale.z) });
            float sphereRadius = trig.sphereRadius * scaleMax;

            return OverlapSphereCapsule(sphereCenter, sphereRadius, playA_world, playB_world, playRadius);
        }
        else if (trig.shapeType == TriggerShapeType::Capsule) {
            float trigRadius = trig.capsuleRadius;
            float trigHeight = trig.capsuleHeight;
            float trigHalfH = (trigHeight - 2.0f * trigRadius) * 0.5f;
            if (trigHalfH < 0.001f) trigHalfH = 0.001f;

            Vector3 localA = { trig.offset.x, trig.offset.y - trigHalfH, trig.offset.z };
            Vector3 localB = { trig.offset.x, trig.offset.y + trigHalfH, trig.offset.z };

            Vector3 trigA_world = TransformPoint(trigTrans.worldMatrix, localA);
            Vector3 trigB_world = TransformPoint(trigTrans.worldMatrix, localB);

            float scaleXZ = std::max(std::abs(trigTrans.scale.x), std::abs(trigTrans.scale.z));
            float trigRadiusWorld = trigRadius * scaleXZ;

            return OverlapCapsuleCapsule(trigA_world, trigB_world, trigRadiusWorld, playA_world, playB_world, playRadius);
        }
        else if (trig.shapeType == TriggerShapeType::Box) {
            Matrix4x4 invWorld = MatrixInverse(trigTrans.worldMatrix);

            Vector3 playA_local = TransformPoint(invWorld, playA_world) - trig.offset;
            Vector3 playB_local = TransformPoint(invWorld, playB_world) - trig.offset;

            Vector3 halfExtents = { trig.boxSize.x * 0.5f, trig.boxSize.y * 0.5f, trig.boxSize.z * 0.5f };

            const int numSamples = 15;
            for (int i = 0; i <= numSamples; ++i) {
                float t = static_cast<float>(i) / static_cast<float>(numSamples);
                Vector3 sample = {
                    playA_local.x + t * (playB_local.x - playA_local.x),
                    playA_local.y + t * (playB_local.y - playA_local.y),
                    playA_local.z + t * (playB_local.z - playA_local.z)
                };

                Vector3 closest = {
                    std::max(-halfExtents.x, std::min(halfExtents.x, sample.x)),
                    std::max(-halfExtents.y, std::min(halfExtents.y, sample.y)),
                    std::max(-halfExtents.z, std::min(halfExtents.z, sample.z))
                };

                float dx = sample.x - closest.x;
                float dy = sample.y - closest.y;
                float dz = sample.z - closest.z;
                float distSq = dx * dx + dy * dy + dz * dz;

                if (distSq <= playRadius * playRadius) {
                    return true;
                }
            }
        }
        return false;
    }
};

} // namespace eng::runtime
