#include "Core/pch.h"
#include "Runtime/GPUSceneTests.h"
#include "Rendering/GPUScene/GPUScene.h"
#include "Rendering/Core/Renderer.h"
#include "Core/Engine/Log.h"
#include "Rendering/Geometry/Assets/RVGRegistry.h"
#include "Rendering/Geometry/Streaming/RVGPageStreamingManager.h"
#include <filesystem>
#include <thread>

namespace eng::renderer {

bool RunGPUSceneTests(EngineResources& eng, GPUScene& scene, Renderer* renderer) noexcept {
    LOG_INFO("================================================================================");
    LOG_INFO("                           RUNNING GPU SCENE TESTS                              ");
    LOG_INFO("================================================================================");

    // Test 1: Basic Instance Allocation & Updates
    LOG_INFO("[GPUSceneTest] Test 1: Basic allocation & updates...");
    GPUGeometryInstance inst1{};
    inst1.model = glm::mat4(2.0f);
    inst1.objectID = 101;
    inst1.flags = GPUInstanceFlags_Visible;

    GPUSceneInstanceHandle h1 = scene.CreateInstance(inst1);
    if (!h1.IsValid() || h1.index != 0 || h1.generation != 0) {
        LOG_ERROR("[GPUSceneTest] Test 1 Failed: Initial handle allocation incorrect.");
        return false;
    }
    if (!scene.IsInstanceValid(h1)) {
        LOG_ERROR("[GPUSceneTest] Test 1 Failed: Handle not valid after creation.");
        return false;
    }

    GPUGeometryInstance inst2{};
    inst2.model = glm::mat4(3.0f);
    inst2.objectID = 101;
    inst2.flags = GPUInstanceFlags_Visible | GPUInstanceFlags_CastShadow;
    scene.UpdateInstance(h1, inst2);

    auto diag1 = scene.GetDiagnostics();
    if (diag1.activeSlots != 1) {
        LOG_ERROR("[GPUSceneTest] Test 1 Failed: Active slot count should be 1.");
        return false;
    }
    LOG_INFO("[GPUSceneTest] Test 1 Passed.");

    // Test 2: Recycling & Generation Validation
    LOG_INFO("[GPUSceneTest] Test 2: Recycling & Generation Validation...");
    scene.DestroyInstance(h1);
    if (scene.IsInstanceValid(h1)) {
        LOG_ERROR("[GPUSceneTest] Test 2 Failed: Handle remains valid after destruction.");
        return false;
    }

    // Try to update using stale handle
    scene.UpdateInstance(h1, inst2);
    auto diag2 = scene.GetDiagnostics();
    if (diag2.staleHandleErrors != 1) {
        LOG_ERROR("[GPUSceneTest] Test 2 Failed: Stale handle update error should have been tracked.");
        return false;
    }

    // Allocate again, it should recycle index 0 but generation should be 1
    GPUSceneInstanceHandle h2 = scene.CreateInstance(inst1);
    if (h2.index != 0 || h2.generation != 1) {
        LOG_ERROR("[GPUSceneTest] Test 2 Failed: Recycling failed or generation not incremented.");
        return false;
    }
    LOG_INFO("[GPUSceneTest] Test 2 Passed.");

    // Test 3: Entity Lookup
    LOG_INFO("[GPUSceneTest] Test 3: Entity Lookup...");
    scene.RegisterEntityInstance(42, h2);
    GPUSceneInstanceHandle lookup = scene.GetEntityInstance(42);
    if (lookup != h2) {
        LOG_ERROR("[GPUSceneTest] Test 3 Failed: Entity-to-instance lookup failed.");
        return false;
    }
    scene.UnregisterEntityInstance(42);
    lookup = scene.GetEntityInstance(42);
    if (lookup.IsValid()) {
        LOG_ERROR("[GPUSceneTest] Test 3 Failed: Lookup returned handle after unregistering.");
        return false;
    }
    LOG_INFO("[GPUSceneTest] Test 3 Passed.");

    // Test 4: Material Overrides
    LOG_INFO("[GPUSceneTest] Test 4: Material Overrides...");
    std::vector<uint32_t> overrides = { 5, 8, 12 };
    scene.SetInstanceMaterialOverrides(h2, overrides);

    auto diag3 = scene.GetDiagnostics();
    if (diag3.materialOverrideCount != 3) {
        LOG_ERROR("[GPUSceneTest] Test 4 Failed: Expected 3 material override entries.");
        return false;
    }

    scene.ClearInstanceMaterialOverrides(h2);
    LOG_INFO("[GPUSceneTest] Test 4 Passed.");

    // Test 5: Dynamic Growth
    LOG_INFO("[GPUSceneTest] Test 5: Dynamic Growth...");
    std::vector<GPUSceneInstanceHandle> handles;
    for (int i = 0; i < 200; ++i) {
        GPUGeometryInstance temp{};
        temp.objectID = 1000 + i;
        handles.push_back(scene.CreateInstance(temp));
    }

    auto diag4 = scene.GetDiagnostics();
    if (diag4.activeSlots < 200) {
        LOG_ERROR("[GPUSceneTest] Test 5 Failed: Growth count smaller than allocated count.");
        return false;
    }

    // Clean up
    for (auto h : handles) {
        scene.DestroyInstance(h);
    }
    scene.DestroyInstance(h2);

    // Test 6: RVG Loading & Metadata Verification
    LOG_INFO("[GPUSceneTest] Test 6: RVG Loading & Metadata Verification...");
    std::string testRvgPath = "Assets/Models/sphere.rvg";
    if (std::filesystem::exists(testRvgPath)) {
        uint32_t rvgIndex = RVGRegistry::Get().LoadAsset(eng, testRvgPath);
        if (rvgIndex == UINT32_MAX) {
            LOG_ERROR("[GPUSceneTest] Test 6 Failed: Failed to load cooked sphere.rvg");
            return false;
        }

        const RVGAsset* asset = RVGRegistry::Get().GetAsset(rvgIndex);
        if (!asset) {
            LOG_ERROR("[GPUSceneTest] Test 6 Failed: GetAsset returned null");
            return false;
        }

        LOG_INFO(("[GPUSceneTest] Loaded sphere.rvg stats - Nodes: " + std::to_string(asset->GetNodeCount()) + 
                  ", Clusters: " + std::to_string(asset->GetClusterCount()) + 
                  ", Pages: " + std::to_string(asset->GetPageCount())).c_str());

        if (asset->GetNodeCount() == 0 || asset->GetClusterCount() == 0 || asset->GetPageCount() == 0) {
            LOG_ERROR("[GPUSceneTest] Test 6 Failed: RVG asset contains empty metadata lists.");
            return false;
        }

        // Verify GPU metadata buffers are successfully allocated
        if (RVGRegistry::Get().GetAssetTableBuffer() == VK_NULL_HANDLE ||
            RVGRegistry::Get().GetNodesBuffer() == VK_NULL_HANDLE ||
            RVGRegistry::Get().GetClustersBuffer() == VK_NULL_HANDLE ||
            RVGRegistry::Get().GetPageDescBuffer() == VK_NULL_HANDLE ||
            RVGRegistry::Get().GetResidentGeometryBuffer() == VK_NULL_HANDLE) {
            LOG_ERROR("[GPUSceneTest] Test 6 Failed: RVG Registry GPU buffers are null.");
            return false;
        }

        LOG_INFO("[GPUSceneTest] Test 6 Passed.");

        // Test 7: RVG Node Hierarchy Traversal Verification
        LOG_INFO("[GPUSceneTest] Test 7: RVG Node Hierarchy Traversal Verification...");
        const auto& nodes = asset->GetNodes();
        uint32_t nodeCount = asset->GetNodeCount();
        uint32_t rootIndex = nodeCount - 1;

        const auto& rootNode = nodes[rootIndex];
        LOG_INFO(("[GPUSceneTest] Root Node 40 details - clusterId: " + std::to_string(rootNode.clusterId) + 
                  ", geometricError: " + std::to_string(rootNode.geometricError) + 
                  ", parentNodeId: " + std::to_string(rootNode.parentNodeId) + 
                  ", childCount: " + std::to_string(rootNode.childCount) +
                  ", children: [" + std::to_string(rootNode.childNodeIds[0]) + ", " + 
                  std::to_string(rootNode.childNodeIds[1]) + ", " + 
                  std::to_string(rootNode.childNodeIds[2]) + ", " + 
                  std::to_string(rootNode.childNodeIds[3]) + "]").c_str());

        std::vector<bool> visited(nodeCount, false);
        std::vector<uint32_t> stack;
        stack.push_back(rootIndex);

        bool hierarchyValid = true;
        while (!stack.empty()) {
            uint32_t nodeIdx = stack.back();
            stack.pop_back();

            if (nodeIdx >= nodeCount) {
                LOG_ERROR(("[GPUSceneTest] Test 7 Failed: Node index out of range: " + std::to_string(nodeIdx)).c_str());
                hierarchyValid = false;
                break;
            }

            if (visited[nodeIdx]) {
                LOG_ERROR(("[GPUSceneTest] Test 7 Failed: Cycle detected in hierarchy at node: " + std::to_string(nodeIdx)).c_str());
                hierarchyValid = false;
                break;
            }

            visited[nodeIdx] = true;
            const auto& node = nodes[nodeIdx];
            
            // Validate child connections
            uint32_t childLimit = node.childCount > 4 ? 4 : node.childCount;
            for (uint32_t c = 0; c < childLimit; ++c) {
                uint32_t childId = node.childNodeIds[c];
                if (childId >= nodeCount) {
                    LOG_ERROR(("[GPUSceneTest] Test 7 Failed: Node " + std::to_string(nodeIdx) + " has invalid child index " + std::to_string(childId)).c_str());
                    hierarchyValid = false;
                    break;
                }
                stack.push_back(childId);
            }
            if (!hierarchyValid) break;
        }

        if (!hierarchyValid) {
            return false;
        }

        LOG_INFO("[GPUSceneTest] Test 7 Passed: Successfully traversed tree hierarchy from root to leaves without cycles or out-of-bounds nodes.");

        // Test 8: RVG Virtual Geometry Page Streaming Validation (Milestone G9)
        LOG_INFO("[GPUSceneTest] Test 8: Virtual Geometry Page Streaming Validation...");
        uint32_t assetID = 0;
        uint32_t totalPages = asset->GetPageCount();
        if (totalPages > 1) {
            uint32_t testPageID = 1;
            
            // Check initial page state (should be Unloaded)
            GeometryPageState initialState = RVGPageStreamingManager::Get().GetPageState(assetID, testPageID);
            LOG_INFO(("[GPUSceneTest] Test 8: Initial page state is " + std::string(GetPageStateName(initialState))).c_str());
            if (initialState != GeometryPageState::Unloaded) {
                LOG_ERROR("[GPUSceneTest] Test 8 Failed: Expected initial page state to be Unloaded.");
                return false;
            }

            // Request the page directly
            LOG_INFO("[GPUSceneTest] Requesting page...");
            RVGPageStreamingManager::Get().RequestPageDirectly(assetID, testPageID, 1.0f);

            // Wait a moment for worker thread to transition state to Requested/Reading/Decompressing/UploadQueued
            LOG_INFO("[GPUSceneTest] Sleeping 30ms for worker thread processing...");
            std::this_thread::sleep_for(std::chrono::milliseconds(30));

            GeometryPageState stateAfterSleep = RVGPageStreamingManager::Get().GetPageState(assetID, testPageID);
            LOG_INFO(("[GPUSceneTest] Page state after 30ms sleep: " + std::string(GetPageStateName(stateAfterSleep))).c_str());

            // Trigger Update to process staging upload
            LOG_INFO("[GPUSceneTest] Triggering Update 1...");
            RVGPageStreamingManager::Get().Update(eng, 0, 1);

            GeometryPageState stateAfterUpdate1 = RVGPageStreamingManager::Get().GetPageState(assetID, testPageID);
            LOG_INFO(("[GPUSceneTest] Page state after Update 1: " + std::string(GetPageStateName(stateAfterUpdate1))).c_str());

            // Wait a moment for worker thread & async transfer fence to be simulated/ready
            LOG_INFO("[GPUSceneTest] Sleeping 50ms for fence...");
            std::this_thread::sleep_for(std::chrono::milliseconds(50));

            // Trigger Update again to finalize upload (fence completion check)
            LOG_INFO("[GPUSceneTest] Triggering Update 2...");
            RVGPageStreamingManager::Get().Update(eng, 0, 2);

            // Check if page state has successfully transitioned to Resident
            GeometryPageState finalState = RVGPageStreamingManager::Get().GetPageState(assetID, testPageID);
            LOG_INFO(("[GPUSceneTest] Final page state: " + std::string(GetPageStateName(finalState))).c_str());
            if (finalState != GeometryPageState::Resident) {
                LOG_ERROR(("[GPUSceneTest] Test 8 Failed: Expected page state to be Resident, but got: " + 
                           std::string(GetPageStateName(finalState))).c_str());
                return false;
            }

            LOG_INFO("[GPUSceneTest] Test 8 Passed: Virtual geometry page request, async streaming, and GPU upload validated successfully.");
        } else {
            LOG_WARN("[GPUSceneTest] Test 8 Skipped: sphere.rvg contains only 1 page.");
        }
    } else {
        LOG_WARN("[GPUSceneTest] Test 6, 7 & 8 Skipped: Assets/Models/sphere.rvg not found.");
    }

    // Test 9: Visibility Buffer Rendering Validation (Milestone G10)
    if (renderer && renderer->GetVisibilityMode() == Renderer::VisibilityMode::VisibilityBuffer) {
        LOG_INFO("[GPUSceneTest] Test 9: Visibility Buffer Rendering Validation...");
        
        bool hasVisPass = false;
        bool hasVisInst = false;
        bool hasVisClust = false;
        bool hasVisPrim = false;
        bool hasVisDepth = false;

        // Check render graph registration by querying the pass configurations
        // Since we exposed it, let's also dump pass info or query the structures directly if possible,
        // or we can test if the m_VisibilityPipeline is Valid and the render graph has registered the outputs.
        // We can do a quick check on the renderer variables!
        if (renderer->m_VisibilityRenderPass != VK_NULL_HANDLE && renderer->m_VisibilityPipeline != VK_NULL_HANDLE) {
            hasVisPass = true;
        }

        if (!renderer->m_VisibilityInstanceHandles.empty() && renderer->m_VisibilityInstanceHandles[0].IsValid()) hasVisInst = true;
        if (!renderer->m_VisibilityClusterHandles.empty() && renderer->m_VisibilityClusterHandles[0].IsValid()) hasVisClust = true;
        if (!renderer->m_VisibilityPrimitiveHandles.empty() && renderer->m_VisibilityPrimitiveHandles[0].IsValid()) hasVisPrim = true;
        if (!renderer->m_VisibilityDepthHandles.empty() && renderer->m_VisibilityDepthHandles[0].IsValid()) hasVisDepth = true;

        if (!hasVisPass) {
            LOG_ERROR("[GPUSceneTest] Test 9 Failed: VisibilityPass renderpass or graphics pipeline is invalid.");
            return false;
        }
        if (!hasVisInst || !hasVisClust || !hasVisPrim || !hasVisDepth) {
            LOG_ERROR("[GPUSceneTest] Test 9 Failed: Missing required visibility buffer targets.");
            return false;
        }

        LOG_INFO("[GPUSceneTest] Test 9 Passed: Visibility pass, pipelines, and target attachments successfully verified.");

        // Test 10: G11 Attribute Reconstruction Pipeline Verification
        LOG_INFO("[GPUSceneTest] Test 10: G11 Attribute Reconstruction Pipeline Verification...");
        bool hasResolvePass = false;
        if (renderer->m_VisibilityResolvePipeline != VK_NULL_HANDLE && renderer->m_VisibilityResolvePipelineLayout != VK_NULL_HANDLE) {
            hasResolvePass = true;
        }
        if (!hasResolvePass) {
            LOG_ERROR("[GPUSceneTest] Test 10 Failed: Visibility Resolve Pipeline or Layout is invalid.");
            return false;
        }
        LOG_INFO("[GPUSceneTest] Test 10 Passed: Attribute reconstruction resolve pipeline and layout successfully verified.");
    } else {
        LOG_WARN("[GPUSceneTest] Test 9 & 10 Skipped: VisibilityBuffer mode is not active.");
    }

    LOG_INFO("[GPUSceneTest] All GPU Scene tests passed successfully.");
    LOG_INFO("================================================================================");
    return true;
}

} // namespace eng::renderer
