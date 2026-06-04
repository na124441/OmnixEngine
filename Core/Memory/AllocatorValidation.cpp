#include "Core/Memory/AllocatorValidation.h"
#include "Core/Memory/LinearAllocator.h"
#include "Core/Memory/PoolAllocator.h"
#include "Core/Memory/StackAllocator.h"
#include "Core/Memory/AllocationTracker.h"
#include "Core/Memory/FragmentationDiagnostics.h"
#include "Core/Logging/Logger.h"
#include <vector>
#include <iostream>
#include <iomanip>

namespace eng::memory {

    static bool TestLinearAllocator() {
        LOG_INFO("[MemoryTest] --- Starting LinearAllocator Tests ---");
        
        constexpr size_t Capacity = 4096;
        LinearAllocator allocator(Capacity);

        if (allocator.GetCapacity() != Capacity) {
            LOG_ERROR("[MemoryTest] LinearAllocator: Incorrect capacity reported.");
            return false;
        }

        // Test 1: Allocate thousands of small blocks
        constexpr size_t AllocSize = 8;
        constexpr size_t NumAllocs = 200;
        std::vector<void*> ptrs;
        ptrs.reserve(NumAllocs);

        for (size_t i = 0; i < NumAllocs; ++i) {
            void* ptr = allocator.Allocate(AllocSize, 8);
            if (!ptr) {
                LOG_ERROR("[MemoryTest] LinearAllocator: Failed allocating block index %zu", i);
                return false;
            }
            // Verify alignment
            if (reinterpret_cast<size_t>(ptr) % 8 != 0) {
                LOG_ERROR("[MemoryTest] LinearAllocator: Pointer is not 8-byte aligned at index %zu", i);
                return false;
            }
            ptrs.push_back(ptr);
        }

        // Verify sequential offsets
        for (size_t i = 1; i < NumAllocs; ++i) {
            uint8_t* pPrev = static_cast<uint8_t*>(ptrs[i - 1]);
            uint8_t* pCurr = static_cast<uint8_t*>(ptrs[i]);
            if (pCurr - pPrev != AllocSize) {
                LOG_ERROR("[MemoryTest] LinearAllocator: Non-contiguous allocation detected!");
                return false;
            }
        }

        // Test 2: Out of memory failure
        void* failedAlloc = allocator.Allocate(3000); // Exceeds remaining capacity
        if (failedAlloc != nullptr) {
            LOG_ERROR("[MemoryTest] LinearAllocator: Allowed allocating beyond capacity!");
            return false;
        }

        // Test 3: Reset and repeat
        allocator.Reset();
        if (allocator.GetOffset() != 0) {
            LOG_ERROR("[MemoryTest] LinearAllocator: Reset failed to zero the offset.");
            return false;
        }

        void* newPtr = allocator.Allocate(128, 16);
        if (!newPtr || newPtr != allocator.GetBase()) {
            LOG_ERROR("[MemoryTest] LinearAllocator: Allocation after reset failed or returned wrong base address.");
            return false;
        }

        LOG_INFO("[MemoryTest] LinearAllocator Tests Passed Successfully.");
        return true;
    }

    static bool TestPoolAllocator() {
        LOG_INFO("[MemoryTest] --- Starting PoolAllocator Tests ---");

        struct TestObject {
            uint32_t a;
            uint32_t b;
            uint64_t c;
        }; // Size: 16 bytes

        constexpr size_t SlotCount = 10;
        PoolAllocator allocator(sizeof(TestObject), SlotCount, alignof(TestObject));

        if (allocator.GetCapacity() != SlotCount || allocator.GetFreeCount() != SlotCount) {
            LOG_ERROR("[MemoryTest] PoolAllocator: Incorrect capacity or free count.");
            return false;
        }

        // Test 1: Saturate allocations
        std::vector<void*> allocatedPtrs;
        for (size_t i = 0; i < SlotCount; ++i) {
            void* ptr = allocator.Allocate();
            if (!ptr) {
                LOG_ERROR("[MemoryTest] PoolAllocator: Allocation failed at index %zu", i);
                return false;
            }
            allocatedPtrs.push_back(ptr);
        }

        if (allocator.GetFreeCount() != 0) {
            LOG_ERROR("[MemoryTest] PoolAllocator: Free count did not drop to 0 after saturation.");
            return false;
        }

        // Verify next allocation fails (returns nullptr)
        void* overflowPtr = allocator.Allocate();
        if (overflowPtr != nullptr) {
            LOG_ERROR("[MemoryTest] PoolAllocator: Allowed allocation on saturated pool!");
            return false;
        }

        // Test 2: Free and reallocate
        // Free slots 2 and 5
        void* ptr2 = allocatedPtrs[2];
        void* ptr5 = allocatedPtrs[5];
        allocator.Free(ptr2);
        allocator.Free(ptr5);

        if (allocator.GetFreeCount() != 2) {
            LOG_ERROR("[MemoryTest] PoolAllocator: Free count incorrect after frees.");
            return false;
        }

        // Reallocate should succeed and return the freed addresses
        void* realloc1 = allocator.Allocate();
        void* realloc2 = allocator.Allocate();

        if (realloc1 != ptr5 && realloc1 != ptr2) {
            LOG_ERROR("[MemoryTest] PoolAllocator: Reallocated block has incorrect address!");
            return false;
        }
        if (realloc2 != ptr5 && realloc2 != ptr2) {
            LOG_ERROR("[MemoryTest] PoolAllocator: Reallocated block has incorrect address!");
            return false;
        }

        // Test 3: Reset
        allocator.Reset();
        if (allocator.GetFreeCount() != SlotCount) {
            LOG_ERROR("[MemoryTest] PoolAllocator: Reset failed to restore free count.");
            return false;
        }

        LOG_INFO("[MemoryTest] PoolAllocator Tests Passed Successfully.");
        return true;
    }

    static bool TestStackAllocator() {
        LOG_INFO("[MemoryTest] --- Starting StackAllocator Tests ---");

        constexpr size_t Capacity = 1024;
        StackAllocator allocator(Capacity);

        // Test 1: Stack LIFO workflow
        StackAllocator::Marker marker0 = allocator.GetMarker();
        void* ptrA = allocator.Allocate(64, 8);
        if (!ptrA) {
            LOG_ERROR("[MemoryTest] StackAllocator: Failed to allocate ptrA.");
            return false;
        }

        StackAllocator::Marker marker1 = allocator.GetMarker();
        void* ptrB = allocator.Allocate(128, 8);
        if (!ptrB) {
            LOG_ERROR("[MemoryTest] StackAllocator: Failed to allocate ptrB.");
            return false;
        }

        StackAllocator::Marker marker2 = allocator.GetMarker();
        void* ptrC = allocator.Allocate(256, 8);
        if (!ptrC) {
            LOG_ERROR("[MemoryTest] StackAllocator: Failed to allocate ptrC.");
            return false;
        }

        // Free to marker2 -> should free ptrC
        allocator.FreeToMarker(marker2);
        if (allocator.GetOffset() != marker2) {
            LOG_ERROR("[MemoryTest] StackAllocator: Rollback to marker2 failed.");
            return false;
        }

        // Allocate ptrD, should get the exact same address as ptrC
        void* ptrD = allocator.Allocate(256, 8);
        if (ptrD != ptrC) {
            LOG_ERROR("[MemoryTest] StackAllocator: Memory address reuse after marker pop failed!");
            return false;
        }

        // Free to marker1 -> should free ptrB and ptrD
        allocator.FreeToMarker(marker1);
        if (allocator.GetOffset() != marker1) {
            LOG_ERROR("[MemoryTest] StackAllocator: Rollback to marker1 failed.");
            return false;
        }

        // Free to marker0 -> should free everything
        allocator.FreeToMarker(marker0);
        if (allocator.GetOffset() != 0) {
            LOG_ERROR("[MemoryTest] StackAllocator: Rollback to marker0 failed.");
            return false;
        }

        LOG_INFO("[MemoryTest] StackAllocator Tests Passed Successfully.");
        return true;
    }

    static bool TestFragmentationDiagnostics() {
        LOG_INFO("[MemoryTest] --- Starting Fragmentation Diagnostics ---");

        // Simulate free blocks: 100 bytes, 250 bytes, 500 bytes, 150 bytes
        std::vector<size_t> freeBlocks = { 100, 250, 500, 150 };
        float fragmentation = FragmentationDiagnostics::CalculateFragmentation(freeBlocks);

        // Total free = 1000, Largest = 500. Expected: (1.0 - (500 / 1000)) * 100.0 = 50%
        if (fragmentation != 50.0f) {
            LOG_ERROR("[MemoryTest] Fragmentation calculation failed: expected 50.0%%, got %.2f%%", fragmentation);
            return false;
        }

        // Single block. Expected: 0%
        std::vector<size_t> singleBlock = { 500 };
        float fragSingle = FragmentationDiagnostics::CalculateFragmentation(singleBlock);
        if (fragSingle != 0.0f) {
            LOG_ERROR("[MemoryTest] Fragmentation calculation failed on single block: expected 0.0%%, got %.2f%%", fragSingle);
            return false;
        }

        LOG_INFO("[MemoryTest] Fragmentation Diagnostics Tests Passed. (Simulated fragmentation calculated: %.2f%%)", fragmentation);
        return true;
    }

    static bool TestAllocationLeakDetection() {
        LOG_INFO("[MemoryTest] --- Starting Allocation Tracking & Leak Detection ---");

        // We clean tracker state first
        AllocationTracker::Reset();

        // 1. Check statistics telemetry is initialized
        MemoryStatistics statsStart = AllocationTracker::GetStatistics();
        if (statsStart.activeAllocations != 0 || statsStart.totalAllocated != 0) {
            LOG_ERROR("[MemoryTest] Tracker initialization state invalid.");
            return false;
        }

        // 2. Track a simulated memory block
        void* fakeBlock = reinterpret_cast<void*>(0xDEADBEEF);
        constexpr size_t FakeSize = 1024;
        AllocationTracker::RegisterAllocation(fakeBlock, FakeSize, "RendererTest");

        MemoryStatistics statsAlloc = AllocationTracker::GetStatistics();
        if (statsAlloc.activeAllocations != 1 || statsAlloc.totalAllocated != FakeSize || statsAlloc.peakUsage != FakeSize) {
            LOG_ERROR("[MemoryTest] Tracker failed to register allocation.");
            return false;
        }

        // Print intermediate report showing the leak
        LOG_INFO("[MemoryTest] Printing simulated memory leak report (EXPECT LEAK):");
        AllocationTracker::DumpLeakReport();

        // 3. Register deallocation
        AllocationTracker::RegisterDeallocation(fakeBlock);
        MemoryStatistics statsDealloc = AllocationTracker::GetStatistics();
        if (statsDealloc.activeAllocations != 0 || statsDealloc.totalAllocated != 0) {
            LOG_ERROR("[MemoryTest] Tracker failed to register deallocation.");
            return false;
        }

        LOG_INFO("[MemoryTest] Printing clean memory leak report (EXPECT NO LEAKS):");
        AllocationTracker::DumpLeakReport();

        // Reset tracker back to clean state
        AllocationTracker::Reset();

        LOG_INFO("[MemoryTest] Allocation Tracking & Leak Detection Tests Passed.");
        return true;
    }

    bool RunMemoryValidationTests() {
        LOG_INFO("[MemoryTest] Starting Engine Memory Infrastructure Stress Validation");
        LOG_INFO("[MemoryTest] =====================================================");

        bool success = true;
        success &= TestLinearAllocator();
        success &= TestPoolAllocator();
        success &= TestStackAllocator();
        success &= TestFragmentationDiagnostics();
        success &= TestAllocationLeakDetection();

        LOG_INFO("[MemoryTest] =====================================================");
        if (success) {
            LOG_INFO("[MemoryTest] ALL ALLOCATOR MEMORY INFRASTRUCTURE TESTS PASSED.");
        } else {
            LOG_ERROR("[MemoryTest] SOME MEMORY INFRASTRUCTURE TESTS FAILED!");
        }

        return success;
    }

} // namespace eng::memory
