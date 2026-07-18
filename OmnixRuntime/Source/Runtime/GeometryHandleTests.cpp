#include "Runtime/GeometryHandleTests.h"
#include "Rendering/Geometry/GeometryHandle.h"
#include "Core/Logger.h"
#include <unordered_map>
#include <iostream>

namespace eng::renderer {

// Simulates a global Geometry Registry/Arena
struct DummyGeometryRegistry {
    struct Slot {
        uint32_t generation = 0;
        bool active = false;
    };
    std::vector<Slot> slots;

    GeometryHandle Allocate() {
        for (uint32_t i = 0; i < slots.size(); ++i) {
            if (!slots[i].active) {
                slots[i].active = true;
                return GeometryHandle(i, slots[i].generation);
            }
        }
        slots.push_back({0, true});
        return GeometryHandle(static_cast<uint32_t>(slots.size() - 1), 0);
    }

    void Deallocate(GeometryHandle handle) {
        if (handle.index < slots.size()) {
            slots[handle.index].active = false;
        }
    }

    void Reload(uint32_t index) {
        if (index < slots.size()) {
            slots[index].generation++;
        }
    }

    bool Validate(GeometryHandle handle) const {
        if (handle.index >= slots.size()) return false;
        const auto& slot = slots[handle.index];
        return slot.active && slot.generation == handle.generation;
    }

    void UnloadAll() {
        for (auto& slot : slots) {
            slot.active = false;
            slot.generation++; // Increments generation so old handles are stale
        }
    }
};

bool RunGeometryHandleTests() noexcept {
    LOG_INFO("================================================================================");
    LOG_INFO("                         RUNNING GEOMETRY HANDLE TESTS                          ");
    LOG_INFO("================================================================================");

    // Test 1: Invalid Handles
    LOG_INFO("[GeometryHandleTest] Test 1: Invalid handle representation...");
    {
        GeometryHandle invalidH;
        if (invalidH.IsValid()) {
            LOG_ERROR("[GeometryHandleTest] Test 1 FAILED: Default constructor did not create invalid handle!");
            return false;
        }
        if (invalidH.index != 0xFFFFFFFF) {
            LOG_ERROR("[GeometryHandleTest] Test 1 FAILED: Invalid handle index must be 0xFFFFFFFF.");
            return false;
        }
        LOG_INFO("[GeometryHandleTest] Test 1 Passed.");
    }

    // Test 2: Equality Operators
    LOG_INFO("[GeometryHandleTest] Test 2: Equality / inequality operators...");
    {
        GeometryHandle h1(5, 1);
        GeometryHandle h2(5, 1);
        GeometryHandle h3(5, 2);
        GeometryHandle h4(6, 1);

        if (!(h1 == h2)) {
            LOG_ERROR("[GeometryHandleTest] Test 2 FAILED: h1 and h2 should be equal!");
            return false;
        }
        if (h1 != h2) {
            LOG_ERROR("[GeometryHandleTest] Test 2 FAILED: h1 and h2 should not be unequal!");
            return false;
        }
        if (h1 == h3) {
            LOG_ERROR("[GeometryHandleTest] Test 2 FAILED: Different generations should not be equal!");
            return false;
        }
        if (h1 == h4) {
            LOG_ERROR("[GeometryHandleTest] Test 2 FAILED: Different indices should not be equal!");
            return false;
        }
        LOG_INFO("[GeometryHandleTest] Test 2 Passed.");
    }

    // Test 3: Hash and Map Support
    LOG_INFO("[GeometryHandleTest] Test 3: Unordered map support & hash uniqueness...");
    {
        std::unordered_map<GeometryHandle, std::string> handleMap;
        GeometryHandle h1(10, 0);
        GeometryHandle h2(10, 1);
        GeometryHandle h3(20, 0);

        handleMap[h1] = "Handle 10 Gen 0";
        handleMap[h2] = "Handle 10 Gen 1";
        handleMap[h3] = "Handle 20 Gen 0";

        if (handleMap.size() != 3) {
            LOG_ERROR("[GeometryHandleTest] Test 3 FAILED: Map size should be 3.");
            return false;
        }
        if (handleMap[h1] != "Handle 10 Gen 0" || handleMap[h2] != "Handle 10 Gen 1") {
            LOG_ERROR("[GeometryHandleTest] Test 3 FAILED: Map returned incorrect value.");
            return false;
        }
        LOG_INFO("[GeometryHandleTest] Test 3 Passed.");
    }

    // Test 4: Stale Handle & Reload Behavior
    LOG_INFO("[GeometryHandleTest] Test 4: Asset reload behavior (stale handles)...");
    {
        DummyGeometryRegistry registry;
        GeometryHandle meshH = registry.Allocate();

        if (!registry.Validate(meshH)) {
            LOG_ERROR("[GeometryHandleTest] Test 4 FAILED: Newly allocated handle failed validation.");
            return false;
        }

        // Simulate reload
        registry.Reload(meshH.index);

        // Old handle is now stale because generation has incremented in registry
        if (registry.Validate(meshH)) {
            LOG_ERROR("[GeometryHandleTest] Test 4 FAILED: Stale handle passed validation after reload!");
            return false;
        }

        // Get new valid handle for reloaded asset
        GeometryHandle newMeshH(meshH.index, registry.slots[meshH.index].generation);
        if (!registry.Validate(newMeshH)) {
            LOG_ERROR("[GeometryHandleTest] Test 4 FAILED: Valid handle after reload failed validation.");
            return false;
        }
        LOG_INFO("[GeometryHandleTest] Test 4 Passed.");
    }

    // Test 5: Scene Unload Behavior
    LOG_INFO("[GeometryHandleTest] Test 5: Scene unload behavior...");
    {
        DummyGeometryRegistry registry;
        GeometryHandle h1 = registry.Allocate();
        GeometryHandle h2 = registry.Allocate();

        registry.UnloadAll();

        if (registry.Validate(h1) || registry.Validate(h2)) {
            LOG_ERROR("[GeometryHandleTest] Test 5 FAILED: Handles survived scene unload!");
            return false;
        }
        LOG_INFO("[GeometryHandleTest] Test 5 Passed.");
    }

    LOG_INFO("[GeometryHandleTest] All geometry handle tests passed successfully.");
    return true;
}

} // namespace eng::renderer
