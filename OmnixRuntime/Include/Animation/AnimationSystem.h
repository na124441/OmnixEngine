#pragma once

#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif

#include "Runtime/OmnixAnimFormat.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <vector>
#include <string>
#include <unordered_map>
#include <functional>
#include <algorithm>
#include <cmath>

namespace eng::animation {

    // -------------------------------------------------------------------------
    // 1. Skeletons & Bones
    // -------------------------------------------------------------------------
    struct TransformPose {
        glm::vec3 position{ 0.0f, 0.0f, 0.0f };
        glm::quat rotation{ 1.0f, 0.0f, 0.0f, 0.0f };
        glm::vec3 scale{ 1.0f, 1.0f, 1.0f };

        glm::mat4 ToMatrix() const {
            glm::mat4 T = glm::translate(glm::mat4(1.0f), position);
            glm::mat4 R = glm::toMat4(rotation);
            glm::mat4 S = glm::scale(glm::mat4(1.0f), scale);
            return T * R * S;
        }

        static TransformPose Lerp(const TransformPose& a, const TransformPose& b, float t) {
            TransformPose result;
            result.position = glm::mix(a.position, b.position, t);
            result.rotation = glm::slerp(a.rotation, b.rotation, t);
            result.scale = glm::mix(a.scale, b.scale, t);
            return result;
        }
    };

    struct Bone {
        std::string name;
        int parentIndex = -1;
        glm::mat4 inverseBindMatrix{ 1.0f };
        TransformPose localPose;
    };

    class Skeleton {
    public:
        Skeleton() = default;

        uint32_t AddBone(const std::string& name, int parentIndex, const glm::mat4& invBind = glm::mat4(1.0f)) {
            Bone bone;
            bone.name = name;
            bone.parentIndex = parentIndex;
            bone.inverseBindMatrix = invBind;
            uint32_t index = static_cast<uint32_t>(m_Bones.size());
            m_Bones.push_back(bone);
            m_NameToIndex[name] = index;
            return index;
        }

        uint32_t GetBoneCount() const { return static_cast<uint32_t>(m_Bones.size()); }
        const std::vector<Bone>& GetBones() const { return m_Bones; }

        int FindBoneIndex(const std::string& name) const {
            auto it = m_NameToIndex.find(name);
            if (it != m_NameToIndex.end()) return static_cast<int>(it->second);
            return -1;
        }

        int GetParentIndex(uint32_t boneIndex) const {
            if (boneIndex < m_Bones.size()) return m_Bones[boneIndex].parentIndex;
            return -1;
        }

        const glm::mat4& GetInverseBindMatrix(uint32_t boneIndex) const {
            return m_Bones[boneIndex].inverseBindMatrix;
        }

    private:
        std::vector<Bone> m_Bones;
        std::unordered_map<std::string, uint32_t> m_NameToIndex;
    };

    // -------------------------------------------------------------------------
    // 2. Animation Clip Sampling
    // -------------------------------------------------------------------------
    class AnimationClip {
    public:
        AnimationClip() = default;
        AnimationClip(const OmnixAnim& animData) : m_Anim(animData) {}

        void SetData(const OmnixAnim& animData) { m_Anim = animData; }
        const OmnixAnim& GetData() const { return m_Anim; }

        float GetDuration() const { return m_Anim.header.durationSeconds; }
        bool HasRootMotion() const { return m_Anim.header.hasRootMotion != 0; }

        glm::vec3 SamplePosition(size_t trackIdx, float time) const {
            if (trackIdx >= m_Anim.boneTracks.size()) return glm::vec3(0.0f);
            const auto& keys = m_Anim.boneTracks[trackIdx].positionKeys;
            if (keys.empty()) return glm::vec3(0.0f);
            if (keys.size() == 1 || time <= keys.front().time) return glm::vec3(keys.front().value.x, keys.front().value.y, keys.front().value.z);
            if (time >= keys.back().time) return glm::vec3(keys.back().value.x, keys.back().value.y, keys.back().value.z);

            for (size_t i = 0; i < keys.size() - 1; ++i) {
                if (time >= keys[i].time && time <= keys[i + 1].time) {
                    float factor = (time - keys[i].time) / (keys[i + 1].time - keys[i].time);
                    glm::vec3 p0(keys[i].value.x, keys[i].value.y, keys[i].value.z);
                    glm::vec3 p1(keys[i + 1].value.x, keys[i + 1].value.y, keys[i + 1].value.z);
                    return glm::mix(p0, p1, factor);
                }
            }
            return glm::vec3(keys.back().value.x, keys.back().value.y, keys.back().value.z);
        }

        glm::quat SampleRotation(size_t trackIdx, float time) const {
            if (trackIdx >= m_Anim.boneTracks.size()) return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
            const auto& keys = m_Anim.boneTracks[trackIdx].rotationKeys;
            if (keys.empty()) return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
            if (keys.size() == 1 || time <= keys.front().time) return glm::quat(keys.front().value.w, keys.front().value.x, keys.front().value.y, keys.front().value.z);
            if (time >= keys.back().time) return glm::quat(keys.back().value.w, keys.back().value.x, keys.back().value.y, keys.back().value.z);

            for (size_t i = 0; i < keys.size() - 1; ++i) {
                if (time >= keys[i].time && time <= keys[i + 1].time) {
                    float factor = (time - keys[i].time) / (keys[i + 1].time - keys[i].time);
                    glm::quat q0(keys[i].value.w, keys[i].value.x, keys[i].value.y, keys[i].value.z);
                    glm::quat q1(keys[i + 1].value.w, keys[i + 1].value.x, keys[i + 1].value.y, keys[i + 1].value.z);
                    return glm::slerp(q0, q1, factor);
                }
            }
            return glm::quat(keys.back().value.w, keys.back().value.x, keys.back().value.y, keys.back().value.z);
        }

        void SamplePose(float time, const Skeleton& skeleton, std::vector<TransformPose>& outLocalPoses) const {
            outLocalPoses.resize(skeleton.GetBoneCount());
            for (size_t trackIdx = 0; trackIdx < m_Anim.boneTracks.size(); ++trackIdx) {
                const auto& track = m_Anim.boneTracks[trackIdx];
                int boneIdx = skeleton.FindBoneIndex(track.boneName);
                if (boneIdx >= 0 && static_cast<size_t>(boneIdx) < outLocalPoses.size()) {
                    outLocalPoses[boneIdx].position = SamplePosition(trackIdx, time);
                    outLocalPoses[boneIdx].rotation = SampleRotation(trackIdx, time);
                    outLocalPoses[boneIdx].scale = glm::vec3(1.0f);
                }
            }
        }

    private:
        OmnixAnim m_Anim;
    };

    // -------------------------------------------------------------------------
    // 3. Animation Player
    // -------------------------------------------------------------------------
    class AnimationPlayer {
    public:
        AnimationPlayer() = default;

        void Play(std::shared_ptr<AnimationClip> clip, bool loop = true, float playbackRate = 1.0f) {
            m_Clip = clip;
            m_Looping = loop;
            m_PlaybackRate = playbackRate;
            m_CurrentTime = 0.0f;
            m_Playing = (m_Clip != nullptr);
        }

        void Pause() { m_Playing = false; }
        void Resume() { if (m_Clip) m_Playing = true; }
        void Stop() { m_Playing = false; m_CurrentTime = 0.0f; }

        void Update(float deltaTime, const Skeleton& skeleton, std::vector<TransformPose>& outLocalPoses) {
            if (!m_Playing || !m_Clip) return;

            m_CurrentTime += deltaTime * m_PlaybackRate;
            float duration = m_Clip->GetDuration();

            if (duration > 0.0f) {
                if (m_Looping) {
                    m_CurrentTime = std::fmod(m_CurrentTime, duration);
                } else if (m_CurrentTime >= duration) {
                    m_CurrentTime = duration;
                    m_Playing = false;
                }
            }

            m_Clip->SamplePose(m_CurrentTime, skeleton, outLocalPoses);
        }

        float GetCurrentTime() const { return m_CurrentTime; }
        bool IsPlaying() const { return m_Playing; }
        bool IsLooping() const { return m_Looping; }

    private:
        std::shared_ptr<AnimationClip> m_Clip;
        float m_CurrentTime = 0.0f;
        float m_PlaybackRate = 1.0f;
        bool m_Looping = true;
        bool m_Playing = false;
    };

    // -------------------------------------------------------------------------
    // 4. Blend Trees
    // -------------------------------------------------------------------------
    class BlendTree {
    public:
        static void BlendPoses1D(
            const std::vector<TransformPose>& poseA,
            const std::vector<TransformPose>& poseB,
            float weight,
            std::vector<TransformPose>& outPose
        ) {
            weight = glm::clamp(weight, 0.0f, 1.0f);
            size_t count = std::min(poseA.size(), poseB.size());
            outPose.resize(count);

            for (size_t i = 0; i < count; ++i) {
                outPose[i] = TransformPose::Lerp(poseA[i], poseB[i], weight);
            }
        }
    };

    // -------------------------------------------------------------------------
    // 5. State Machines
    // -------------------------------------------------------------------------
    struct AnimState {
        std::string name;
        std::shared_ptr<AnimationClip> clip;
        bool looping = true;
    };

    struct AnimTransition {
        std::string fromState;
        std::string toState;
        float duration = 0.25f;
    };

    class AnimStateMachine {
    public:
        AnimStateMachine() = default;

        void AddState(const AnimState& state) {
            m_States[state.name] = state;
            if (m_ActiveState.empty()) {
                m_ActiveState = state.name;
            }
        }

        void AddTransition(const AnimTransition& transition) {
            m_Transitions.push_back(transition);
        }

        void SetState(const std::string& name) {
            if (m_States.find(name) != m_States.end()) {
                m_ActiveState = name;
            }
        }

        const std::string& GetActiveState() const { return m_ActiveState; }

    private:
        std::unordered_map<std::string, AnimState> m_States;
        std::vector<AnimTransition> m_Transitions;
        std::string m_ActiveState;
    };

    // -------------------------------------------------------------------------
    // 6. Animation Graph
    // -------------------------------------------------------------------------
    class AnimGraph {
    public:
        AnimGraph() = default;

        void EvaluateGraph(
            float deltaTime,
            const Skeleton& skeleton,
            std::vector<glm::mat4>& outSkinningPalette
        ) {
            uint32_t boneCount = skeleton.GetBoneCount();
            outSkinningPalette.resize(boneCount, glm::mat4(1.0f));

            std::vector<glm::mat4> globalMatrices(boneCount, glm::mat4(1.0f));
            for (uint32_t i = 0; i < boneCount; ++i) {
                int parentIdx = skeleton.GetParentIndex(i);
                if (parentIdx >= 0 && static_cast<size_t>(parentIdx) < boneCount) {
                    globalMatrices[i] = globalMatrices[parentIdx];
                } else {
                    globalMatrices[i] = glm::mat4(1.0f);
                }
                outSkinningPalette[i] = globalMatrices[i] * skeleton.GetInverseBindMatrix(i);
            }
        }
    };

    // -------------------------------------------------------------------------
    // 7. Root Motion
    // -------------------------------------------------------------------------
    class RootMotionExtractor {
    public:
        static glm::vec3 ExtractDeltaPosition(
            const AnimationClip& clip,
            float prevTime,
            float currentTime,
            size_t rootTrackIdx = 0
        ) {
            if (!clip.HasRootMotion()) return glm::vec3(0.0f);
            glm::vec3 p0 = clip.SamplePosition(rootTrackIdx, prevTime);
            glm::vec3 p1 = clip.SamplePosition(rootTrackIdx, currentTime);
            return p1 - p0;
        }
    };

    // -------------------------------------------------------------------------
    // 8. Animation Events
    // -------------------------------------------------------------------------
    struct AnimEvent {
        float triggerTime = 0.0f;
        std::string name;
        std::function<void()> callback;
    };

    class AnimTimeline {
    public:
        void AddEvent(const AnimEvent& evt) {
            m_Events.push_back(evt);
        }

        void EvaluateEvents(float prevTime, float currentTime) {
            for (const auto& evt : m_Events) {
                if (evt.triggerTime >= prevTime && evt.triggerTime <= currentTime) {
                    if (evt.callback) evt.callback();
                }
            }
        }

    private:
        std::vector<AnimEvent> m_Events;
    };

    // -------------------------------------------------------------------------
    // 9. GPU Skinning
    // -------------------------------------------------------------------------
    struct SkinnedVertex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 uv;
        glm::uvec4 boneIndices{ 0, 0, 0, 0 };
        glm::vec4 boneWeights{ 1.0f, 0.0f, 0.0f, 0.0f };
    };

    struct SkinningPaletteSSBO {
        glm::mat4 boneMatrices[128];
    };

    // -------------------------------------------------------------------------
    // 10. Morph Targets
    // -------------------------------------------------------------------------
    struct MorphTarget {
        std::string name;
        std::vector<glm::vec3> vertexDisplacements;
    };

    class MorphTargetEvaluator {
    public:
        static void EvaluateMorphs(
            const std::vector<glm::vec3>& baseVertices,
            const std::vector<MorphTarget>& targets,
            const std::vector<float>& weights,
            std::vector<glm::vec3>& outVertices
        ) {
            outVertices = baseVertices;
            size_t targetCount = std::min(targets.size(), weights.size());

            for (size_t t = 0; t < targetCount; ++t) {
                float w = weights[t];
                if (std::abs(w) < 1e-5f) continue;
                const auto& disp = targets[t].vertexDisplacements;
                size_t vertCount = std::min(outVertices.size(), disp.size());
                for (size_t v = 0; v < vertCount; ++v) {
                    outVertices[v] += disp[v] * w;
                }
            }
        }
    };

    // -------------------------------------------------------------------------
    // 11. IK Solvers
    // -------------------------------------------------------------------------
    class TwoBoneIKSolver {
    public:
        static bool Solve(
            const glm::vec3& rootPos,
            const glm::vec3& targetPos,
            float upperLength,
            float lowerLength,
            glm::vec3& outKneePos
        ) {
            glm::vec3 dir = targetPos - rootPos;
            float dist = glm::length(dir);
            float maxLen = upperLength + lowerLength;
            if (dist > maxLen) {
                dist = maxLen * 0.999f;
            }

            float cosKnee = (upperLength * upperLength + lowerLength * lowerLength - dist * dist) / (2.0f * upperLength * lowerLength);
            cosKnee = glm::clamp(cosKnee, -1.0f, 1.0f);
            float kneeAngle = std::acos(cosKnee);

            glm::vec3 nDir = (dist > 0.001f) ? glm::normalize(dir) : glm::vec3(0.0f, 0.0f, 1.0f);
            outKneePos = rootPos + nDir * (upperLength * std::cos(kneeAngle * 0.5f));
            return true;
        }
    };

} // namespace eng::animation
