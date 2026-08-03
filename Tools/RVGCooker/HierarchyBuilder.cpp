#include "HierarchyBuilder.h"
#include "ClusterGroupBuilder.h"
#include "ClusterSimplifier.h"
#include "ClusterBuilder.h"
#include <algorithm>
#include <iostream>

namespace eng::cooker {

bool HierarchyBuilder::BuildHierarchy(
    const std::vector<CookedCluster>& leafClusters,
    uint32_t vertexLimit,
    uint32_t triangleLimit
) {
    m_Nodes.clear();
    m_AllClusters = leafClusters;

    std::vector<uint32_t> currentLevelNodeIds;
    for (uint32_t i = 0; i < leafClusters.size(); ++i) {
        HierarchyNode node;
        node.clusterId = i;
        node.geometricError = 0.0f;
        m_Nodes.push_back(node);
        currentLevelNodeIds.push_back(static_cast<uint32_t>(m_Nodes.size() - 1));
    }

    uint32_t level = 0;
    std::cout << "Starting hierarchy builder...\n";

    while (currentLevelNodeIds.size() > 1) {
        std::cout << "Level " << level << " has " << currentLevelNodeIds.size() << " nodes.\n";

        std::vector<CookedCluster> levelClusters;
        for (uint32_t nodeId : currentLevelNodeIds) {
            levelClusters.push_back(m_AllClusters[m_Nodes[nodeId].clusterId]);
        }

        ClusterGroupBuilder groupBuilder;
        if (!groupBuilder.GroupClusters(levelClusters, 4)) {
            std::cerr << "Failed to group clusters at level " << level << ".\n";
            return false;
        }

        const auto& groups = groupBuilder.GetGroups();
        std::vector<uint32_t> nextLevelNodeIds;

        for (const auto& group : groups) {
            ClusterSimplifier simplifier;
            std::vector<eng::renderer::PbrVertex> simplifiedVerts;
            std::vector<uint32_t> simplifiedIndices;
            float error = 0.0f;

            if (!simplifier.Simplify(group, levelClusters, simplifiedVerts, simplifiedIndices, error)) {
                std::cerr << "Failed to simplify cluster group.\n";
                return false;
            }

            float maxChildError = 0.0f;
            for (uint32_t localClusterIdx : group.clusterIndices) {
                uint32_t childNodeId = currentLevelNodeIds[localClusterIdx];
                maxChildError = glm::max(maxChildError, m_Nodes[childNodeId].geometricError);
            }
            float parentError = glm::max(maxChildError + 0.001f, error);

            ClusterBuilder builder;
            if (!builder.BuildClusters(simplifiedVerts, simplifiedIndices, vertexLimit, triangleLimit)) {
                std::cerr << "Failed to re-cluster simplified geometry.\n";
                return false;
            }

            const auto& parents = builder.GetClusters();
            for (const auto& p : parents) {
                uint32_t parentClusterId = static_cast<uint32_t>(m_AllClusters.size());
                m_AllClusters.push_back(p);

                HierarchyNode parentNode;
                parentNode.clusterId = parentClusterId;
                parentNode.geometricError = parentError;

                for (uint32_t localClusterIdx : group.clusterIndices) {
                    uint32_t childNodeId = currentLevelNodeIds[localClusterIdx];
                    parentNode.childNodeIds.push_back(childNodeId);
                }

                m_Nodes.push_back(parentNode);
                uint32_t parentNodeId = static_cast<uint32_t>(m_Nodes.size() - 1);
                
                for (uint32_t childNodeId : parentNode.childNodeIds) {
                    m_Nodes[childNodeId].parentNodeId = parentNodeId;
                }

                nextLevelNodeIds.push_back(parentNodeId);
            }
        }

        if (nextLevelNodeIds.size() >= currentLevelNodeIds.size()) {
            std::cout << "Simplification converged. Forcing final root collapse...\n";
            
            std::vector<eng::renderer::PbrVertex> mergedVerts;
            std::vector<uint32_t> mergedIndices;
            for (uint32_t nodeId : currentLevelNodeIds) {
                const auto& c = m_AllClusters[m_Nodes[nodeId].clusterId];
                uint32_t offset = static_cast<uint32_t>(mergedVerts.size());
                mergedVerts.insert(mergedVerts.end(), c.vertices.begin(), c.vertices.end());
                for (uint32_t idx : c.indices) {
                    mergedIndices.push_back(idx + offset);
                }
            }

            ClusterGroup mockGroup;
            mockGroup.clusterIndices = { 0 };
            
            std::vector<CookedCluster> mockAllClusters;
            CookedCluster tempMerged;
            tempMerged.vertices = mergedVerts;
            tempMerged.indices = mergedIndices;
            mockAllClusters.push_back(tempMerged);

            float totalError = 0.0f;
            while (mergedVerts.size() > vertexLimit || (mergedIndices.size() / 3) > triangleLimit) {
                std::vector<eng::renderer::PbrVertex> simpVerts;
                std::vector<uint32_t> simpIndices;
                float error = 0.0f;
                
                ClusterSimplifier simplifier;
                simplifier.Simplify(mockGroup, mockAllClusters, simpVerts, simpIndices, error, false);
                
                std::cout << "  Root Collapse Iteration: beforeVerts=" << mergedVerts.size()
                          << ", beforeTris=" << (mergedIndices.size() / 3)
                          << " -> afterVerts=" << simpVerts.size()
                          << ", afterTris=" << (simpIndices.size() / 3) << "\n";

                if (simpIndices.empty() || simpVerts.empty() || simpVerts.size() >= mergedVerts.size()) {
                    break; 
                }

                totalError += error;
                mergedVerts = simpVerts;
                mergedIndices = simpIndices;
                
                mockAllClusters[0].vertices = mergedVerts;
                mockAllClusters[0].indices = mergedIndices;
            }

            CookedCluster rootCluster;
            rootCluster.materialIndex = 0;
            rootCluster.vertices = mergedVerts;
            rootCluster.indices = mergedIndices;

            if (!rootCluster.vertices.empty()) {
                rootCluster.boundsMin = rootCluster.vertices[0].position;
                rootCluster.boundsMax = rootCluster.vertices[0].position;
                for (const auto& v : rootCluster.vertices) {
                    rootCluster.boundsMin = glm::min(rootCluster.boundsMin, v.position);
                    rootCluster.boundsMax = glm::max(rootCluster.boundsMax, v.position);
                }
                glm::vec3 center = (rootCluster.boundsMin + rootCluster.boundsMax) * 0.5f;
                float maxDistSq = 0.0f;
                for (const auto& v : rootCluster.vertices) {
                    glm::vec3 diff = v.position - center;
                    maxDistSq = glm::max(maxDistSq, glm::dot(diff, diff));
                }
                rootCluster.boundsSphere = glm::vec4(center, std::sqrt(maxDistSq));
            }
            rootCluster.coneAxis = glm::vec3(0, 1, 0);
            rootCluster.coneCutoff = -1.0f;

            uint32_t rootClusterId = static_cast<uint32_t>(m_AllClusters.size());
            m_AllClusters.push_back(rootCluster);

            HierarchyNode rootNode;
            rootNode.clusterId = rootClusterId;
            rootNode.geometricError = m_Nodes[currentLevelNodeIds[0]].geometricError + 0.1f + totalError;
            rootNode.childNodeIds = currentLevelNodeIds;

            m_Nodes.push_back(rootNode);
            uint32_t rootNodeId = static_cast<uint32_t>(m_Nodes.size() - 1);
            for (uint32_t childNodeId : rootNode.childNodeIds) {
                m_Nodes[childNodeId].parentNodeId = rootNodeId;
            }
            break;
        }

        currentLevelNodeIds = nextLevelNodeIds;
        level++;
    }

    std::cout << "Hierarchy built successfully: " << m_Nodes.size() << " total nodes, " 
              << level + 1 << " levels.\n";
    return true;
}

} // namespace eng::cooker
