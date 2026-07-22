#pragma once

#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <algorithm>
#include <queue>
#include <cmath>

namespace eng::ai {

    // -------------------------------------------------------------------------
    // 1. Blackboard Parameters
    // -------------------------------------------------------------------------
    class Blackboard {
    public:
        void SetBool(const std::string& key, bool val) { m_Bools[key] = val; }
        bool GetBool(const std::string& key, bool defaultVal = false) const {
            auto it = m_Bools.find(key);
            return (it != m_Bools.end()) ? it->second : defaultVal;
        }

        void SetFloat(const std::string& key, float val) { m_Floats[key] = val; }
        float GetFloat(const std::string& key, float defaultVal = 0.0f) const {
            auto it = m_Floats.find(key);
            return (it != m_Floats.end()) ? it->second : defaultVal;
        }

        void SetVector(const std::string& key, const glm::vec3& val) { m_Vectors[key] = val; }
        glm::vec3 GetVector(const std::string& key, const glm::vec3& defaultVal = glm::vec3(0.0f)) const {
            auto it = m_Vectors.find(key);
            return (it != m_Vectors.end()) ? it->second : defaultVal;
        }

        void SetString(const std::string& key, const std::string& val) { m_Strings[key] = val; }
        std::string GetString(const std::string& key, const std::string& defaultVal = "") const {
            auto it = m_Strings.find(key);
            return (it != m_Strings.end()) ? it->second : defaultVal;
        }

        bool HasKey(const std::string& key) const {
            return (m_Bools.count(key) || m_Floats.count(key) || m_Vectors.count(key) || m_Strings.count(key));
        }

    private:
        std::unordered_map<std::string, bool> m_Bools;
        std::unordered_map<std::string, float> m_Floats;
        std::unordered_map<std::string, glm::vec3> m_Vectors;
        std::unordered_map<std::string, std::string> m_Strings;
    };

    // -------------------------------------------------------------------------
    // 2. NavMesh & A* Pathfinding
    // -------------------------------------------------------------------------
    struct NavNode {
        uint32_t id = 0;
        glm::vec3 position{ 0.0f, 0.0f, 0.0f };
        std::vector<uint32_t> neighbors;
    };

    class NavMesh {
    public:
        uint32_t AddNode(const glm::vec3& pos) {
            uint32_t id = static_cast<uint32_t>(m_Nodes.size());
            NavNode node;
            node.id = id;
            node.position = pos;
            m_Nodes.push_back(node);
            return id;
        }

        void ConnectNodes(uint32_t a, uint32_t b) {
            if (a < m_Nodes.size() && b < m_Nodes.size()) {
                m_Nodes[a].neighbors.push_back(b);
                m_Nodes[b].neighbors.push_back(a);
            }
        }

        size_t GetNodeCount() const { return m_Nodes.size(); }

        uint32_t FindNearestNode(const glm::vec3& pos) const {
            if (m_Nodes.empty()) return 0;
            uint32_t bestIdx = 0;
            float bestDistSq = glm::distance2(pos, m_Nodes[0].position);
            for (size_t i = 1; i < m_Nodes.size(); ++i) {
                float d2 = glm::distance2(pos, m_Nodes[i].position);
                if (d2 < bestDistSq) {
                    bestDistSq = d2;
                    bestIdx = static_cast<uint32_t>(i);
                }
            }
            return bestIdx;
        }

        bool FindPath(const glm::vec3& startPos, const glm::vec3& targetPos, std::vector<glm::vec3>& outPath) const {
            if (m_Nodes.empty()) return false;
            uint32_t startIdx = FindNearestNode(startPos);
            uint32_t targetIdx = FindNearestNode(targetPos);

            std::vector<float> gScore(m_Nodes.size(), 1e9f);
            std::vector<uint32_t> cameFrom(m_Nodes.size(), 0);
            std::vector<bool> openSet(m_Nodes.size(), false);

            gScore[startIdx] = 0.0f;
            openSet[startIdx] = true;

            auto Heuristic = [&](uint32_t idx) {
                return glm::distance(m_Nodes[idx].position, m_Nodes[targetIdx].position);
            };

            for (size_t iter = 0; iter < m_Nodes.size(); ++iter) {
                // Find node in openSet with lowest fScore = gScore + Heuristic
                int current = -1;
                float lowestF = 1e9f;
                for (size_t i = 0; i < m_Nodes.size(); ++i) {
                    if (openSet[i]) {
                        float f = gScore[i] + Heuristic(static_cast<uint32_t>(i));
                        if (f < lowestF) {
                            lowestF = f;
                            current = static_cast<int>(i);
                        }
                    }
                }

                if (current == -1) break; // Path not found
                if (static_cast<uint32_t>(current) == targetIdx) {
                    // Reconstruct path
                    outPath.clear();
                    uint32_t currNode = targetIdx;
                    while (currNode != startIdx) {
                        outPath.push_back(m_Nodes[currNode].position);
                        currNode = cameFrom[currNode];
                    }
                    outPath.push_back(m_Nodes[startIdx].position);
                    std::reverse(outPath.begin(), outPath.end());
                    return true;
                }

                openSet[current] = false;

                for (uint32_t neighbor : m_Nodes[current].neighbors) {
                    float tentativeG = gScore[current] + glm::distance(m_Nodes[current].position, m_Nodes[neighbor].position);
                    if (tentativeG < gScore[neighbor]) {
                        cameFrom[neighbor] = current;
                        gScore[neighbor] = tentativeG;
                        openSet[neighbor] = true;
                    }
                }
            }

            return false;
        }

    private:
        std::vector<NavNode> m_Nodes;
    };

    // -------------------------------------------------------------------------
    // 3. Behavior Tree Runtime
    // -------------------------------------------------------------------------
    enum class BTNodeStatus {
        Success,
        Failure,
        Running
    };

    class BTNode {
    public:
        virtual ~BTNode() = default;
        virtual BTNodeStatus Tick(Blackboard& blackboard) = 0;
    };

    class BTSequence : public BTNode {
    public:
        void AddChild(std::shared_ptr<BTNode> child) { m_Children.push_back(child); }

        BTNodeStatus Tick(Blackboard& blackboard) override {
            for (auto& child : m_Children) {
                BTNodeStatus status = child->Tick(blackboard);
                if (status != BTNodeStatus::Success) return status;
            }
            return BTNodeStatus::Success;
        }

    private:
        std::vector<std::shared_ptr<BTNode>> m_Children;
    };

    class BTSelector : public BTNode {
    public:
        void AddChild(std::shared_ptr<BTNode> child) { m_Children.push_back(child); }

        BTNodeStatus Tick(Blackboard& blackboard) override {
            for (auto& child : m_Children) {
                BTNodeStatus status = child->Tick(blackboard);
                if (status != BTNodeStatus::Failure) return status;
            }
            return BTNodeStatus::Failure;
        }

    private:
        std::vector<std::shared_ptr<BTNode>> m_Children;
    };

    class BTAction : public BTNode {
    public:
        using ActionFunc = std::function<BTNodeStatus(Blackboard&)>;
        BTAction(ActionFunc func) : m_Func(func) {}

        BTNodeStatus Tick(Blackboard& blackboard) override {
            return m_Func ? m_Func(blackboard) : BTNodeStatus::Failure;
        }

    private:
        ActionFunc m_Func;
    };

    class BTCondition : public BTNode {
    public:
        using ConditionFunc = std::function<BTNodeStatus(Blackboard&)>;
        BTCondition(ConditionFunc func) : m_Func(func) {}

        BTNodeStatus Tick(Blackboard& blackboard) override {
            return m_Func ? m_Func(blackboard) : BTNodeStatus::Failure;
        }

    private:
        ConditionFunc m_Func;
    };

    // -------------------------------------------------------------------------
    // 4. Environment Query System (EQS)
    // -------------------------------------------------------------------------
    struct EQSQueryPoint {
        glm::vec3 position;
        float score = 0.0f;
    };

    class EQSSolver {
    public:
        static glm::vec3 FindBestCoverPosition(
            const glm::vec3& agentPos,
            const glm::vec3& enemyPos,
            float searchRadius = 15.0f,
            int gridResolution = 5
        ) {
            glm::vec3 bestPos = agentPos;
            float bestScore = -1e9f;

            float step = (searchRadius * 2.0f) / gridResolution;
            for (int x = -gridResolution; x <= gridResolution; ++x) {
                for (int z = -gridResolution; z <= gridResolution; ++z) {
                    glm::vec3 candidatePos = agentPos + glm::vec3(x * step, 0.0f, z * step);
                    float distToEnemy = glm::distance(candidatePos, enemyPos);
                    float distToAgent = glm::distance(candidatePos, agentPos);

                    // Score prefer points further from enemy and closer to agent
                    float score = distToEnemy * 1.5f - distToAgent * 0.5f;
                    if (score > bestScore) {
                        bestScore = score;
                        bestPos = candidatePos;
                    }
                }
            }
            return bestPos;
        }
    };

    // -------------------------------------------------------------------------
    // 5. Steering Behaviors
    // -------------------------------------------------------------------------
    class SteeringBehaviors {
    public:
        static glm::vec3 Seek(
            const glm::vec3& currentPos,
            const glm::vec3& currentVel,
            const glm::vec3& targetPos,
            float maxSpeed
        ) {
            glm::vec3 desiredVel = glm::normalize(targetPos - currentPos) * maxSpeed;
            return desiredVel - currentVel;
        }

        static glm::vec3 Flee(
            const glm::vec3& currentPos,
            const glm::vec3& currentVel,
            const glm::vec3& targetPos,
            float maxSpeed
        ) {
            glm::vec3 desiredVel = glm::normalize(currentPos - targetPos) * maxSpeed;
            return desiredVel - currentVel;
        }

        static glm::vec3 Arrive(
            const glm::vec3& currentPos,
            const glm::vec3& currentVel,
            const glm::vec3& targetPos,
            float maxSpeed,
            float slowingRadius = 5.0f
        ) {
            glm::vec3 offset = targetPos - currentPos;
            float dist = glm::length(offset);
            if (dist < 0.001f) return -currentVel;

            float speed = (dist < slowingRadius) ? maxSpeed * (dist / slowingRadius) : maxSpeed;
            glm::vec3 desiredVel = (offset / dist) * speed;
            return desiredVel - currentVel;
        }
    };

    // -------------------------------------------------------------------------
    // 6. Crowd Control & Flocking
    // -------------------------------------------------------------------------
    class CrowdController {
    public:
        static glm::vec3 ComputeFlockingVelocity(
            size_t agentIdx,
            const std::vector<glm::vec3>& positions,
            const std::vector<glm::vec3>& velocities,
            float neighborRadius = 10.0f,
            float separationWeight = 1.5f,
            float alignmentWeight = 1.0f,
            float cohesionWeight = 1.0f
        ) {
            if (agentIdx >= positions.size() || positions.size() <= 1) return glm::vec3(0.0f);

            glm::vec3 separation(0.0f);
            glm::vec3 alignment(0.0f);
            glm::vec3 centerOfMass(0.0f);
            int neighborCount = 0;

            const glm::vec3& myPos = positions[agentIdx];

            for (size_t i = 0; i < positions.size(); ++i) {
                if (i == agentIdx) continue;
                float dist = glm::distance(myPos, positions[i]);
                if (dist > 0.001f && dist < neighborRadius) {
                    separation += glm::normalize(myPos - positions[i]) / dist;
                    alignment += velocities[i];
                    centerOfMass += positions[i];
                    neighborCount++;
                }
            }

            if (neighborCount == 0) return glm::vec3(0.0f);

            alignment /= static_cast<float>(neighborCount);
            centerOfMass /= static_cast<float>(neighborCount);
            glm::vec3 cohesion = centerOfMass - myPos;

            return (separation * separationWeight) + (alignment * alignmentWeight) + (cohesion * cohesionWeight);
        }
    };

} // namespace eng::ai
