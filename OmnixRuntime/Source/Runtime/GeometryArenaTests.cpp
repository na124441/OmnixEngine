#include "Runtime/GeometryArenaTests.h"
#include "Rendering/Geometry/Arena/GeometryArena.h"
#include "Core/Engine/EngineResources.h"
#include "Core/Logger.h"
#include <vector>

namespace eng::renderer {

bool RunGeometryArenaTests(EngineResources& eng) noexcept {
    LOG_INFO("================================================================================");
    LOG_INFO("                         RUNNING GEOMETRY ARENA TESTS                           ");
    LOG_INFO("================================================================================");

    // Test 1: Initialize temporary arena with tiny capacity to test growth easily
    LOG_INFO("[GeometryArenaTest] Test 1: Initialization & Allocation with tiny bounds...");
    GeometryArena testArena;
    // 256 bytes vertex capacity (e.g. holds ~8 vertices of 32 bytes)
    // 128 bytes index capacity (holds ~32 indices of 4 bytes)
    testArena.Initialize(eng.device, eng.allocator, 256, 128);

    if (testArena.GetVertexBuffer() == VK_NULL_HANDLE || testArena.GetIndexBuffer() == VK_NULL_HANDLE) {
        LOG_ERROR("[GeometryArenaTest] Test 1 FAILED: Buffers not created on initialization.");
        return false;
    }

    struct TempVertex {
        float pos[3];
        float color[3];
        float uv[2];
    };

    // Create a mock mesh of 4 vertices (128 bytes) and 6 indices (24 bytes)
    std::vector<TempVertex> vertices(4);
    std::vector<uint32_t> indices = { 0, 1, 2, 2, 3, 0 };

    GeometryHandle h1;
    bool ok = testArena.Allocate(eng, vertices.data(), vertices.size(), sizeof(TempVertex), indices.data(), indices.size(), h1);
    if (!ok || !h1.IsValid()) {
        LOG_ERROR("[GeometryArenaTest] Test 1 FAILED: Failed to allocate first mock mesh.");
        return false;
    }

    const auto* alloc1 = testArena.GetAllocation(h1);
    if (!alloc1 || alloc1->vertexCount != 4 || alloc1->indexCount != 6) {
        LOG_ERROR("[GeometryArenaTest] Test 1 FAILED: Allocation metadata is incorrect.");
        return false;
    }

    LOG_INFO("[GeometryArenaTest] Test 1 Passed.");

    // Test 2: Allocation Staleness and Validation
    LOG_INFO("[GeometryArenaTest] Test 2: Allocation validation and staleness...");
    {
        if (!testArena.IsValid(h1)) {
            LOG_ERROR("[GeometryArenaTest] Test 2 FAILED: Allocated handle should be valid.");
            return false;
        }

        testArena.Free(h1);
        // Note: Freeing defers the actual allocator block reclaim, but invalidates the handle immediately.
        if (testArena.IsValid(h1)) {
            LOG_ERROR("[GeometryArenaTest] Test 2 FAILED: Freed handle must be invalid immediately.");
            return false;
        }
        if (testArena.GetAllocation(h1) != nullptr) {
            LOG_ERROR("[GeometryArenaTest] Test 2 FAILED: GetAllocation must return null for invalid/freed handles.");
            return false;
        }

        // Process deferred frees by simulating BeginFrame
        testArena.BeginFrame(0);
    }
    LOG_INFO("[GeometryArenaTest] Test 2 Passed.");

    // Test 3: Arena Growth
    LOG_INFO("[GeometryArenaTest] Test 3: Dynamic buffer growth...");
    {
        // Re-allocate mock meshes to fill and exceed the tiny capacities
        // Initial capacities: 256 vertex bytes, 128 index bytes.
        // Mesh 1: 4 vertices (128 bytes), 6 indices (24 bytes)
        // Mesh 2: 4 vertices (128 bytes), 6 indices (24 bytes) -> fills vertex buffer to 256 bytes exactly
        // Mesh 3: 4 vertices (128 bytes), 6 indices (24 bytes) -> triggers growth!

        GeometryHandle hA, hB, hC;
        bool okA = testArena.Allocate(eng, vertices.data(), vertices.size(), sizeof(TempVertex), indices.data(), indices.size(), hA);
        bool okB = testArena.Allocate(eng, vertices.data(), vertices.size(), sizeof(TempVertex), indices.data(), indices.size(), hB);
        
        if (!okA || !okB) {
            LOG_ERROR("[GeometryArenaTest] Test 3 FAILED: Failed to allocate first two meshes.");
            return false;
        }

        ArenaStats statsBefore = testArena.GetStats();
        if (statsBefore.growthCount != 0) {
            LOG_ERROR("[GeometryArenaTest] Test 3 FAILED: Growth count should be 0 before overflow.");
            return false;
        }

        // Mesh 3 causes growth
        bool okC = testArena.Allocate(eng, vertices.data(), vertices.size(), sizeof(TempVertex), indices.data(), indices.size(), hC);
        if (!okC || !hC.IsValid()) {
            LOG_ERROR("[GeometryArenaTest] Test 3 FAILED: Failed to allocate third mesh triggering growth.");
            return false;
        }

        ArenaStats statsAfter = testArena.GetStats();
        if (statsAfter.growthCount == 0) {
            LOG_ERROR("[GeometryArenaTest] Test 3 FAILED: Growth count did not increase after buffer overflow.");
            return false;
        }

        // Verify all handles survive and are still valid after growth
        if (!testArena.IsValid(hA) || !testArena.IsValid(hB) || !testArena.IsValid(hC)) {
            LOG_ERROR("[GeometryArenaTest] Test 3 FAILED: Handles did not survive buffer growth.");
            return false;
        }

        LOG_INFO(("[GeometryArenaTest] Growth triggered successfully. Growth count = " + std::to_string(statsAfter.growthCount)).c_str());
    }
    LOG_INFO("[GeometryArenaTest] Test 3 Passed.");

    // Cleanup temporary arena
    testArena.Shutdown();

    LOG_INFO("[GeometryArenaTest] All geometry arena tests passed successfully.");
    return true;
}

} // namespace eng::renderer
