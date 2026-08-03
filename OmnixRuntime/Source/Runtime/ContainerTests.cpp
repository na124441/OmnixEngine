#include "Runtime/ContainerTests.h"
#include "Core/Containers/DynamicArray.h"
#include "Core/Containers/HashMap.h"
#include "Core/Containers/String.h"
#include "Core/Containers/StringView.h"
#include "Core/Memory/FreeListAllocator.h"
#include "Core/Logging/Logger.h"
#include <iostream>
#include <vector>
#include <cassert>

namespace eng::runtime {

    // Helper tracker to detect constructor/destructor leaks
    struct DestructorTracker {
        static int s_ActiveCount;
        int value = 0;

        DestructorTracker() {
            ++s_ActiveCount;
        }

        explicit DestructorTracker(int val) : value(val) {
            ++s_ActiveCount;
        }

        DestructorTracker(const DestructorTracker& other) : value(other.value) {
            ++s_ActiveCount;
        }

        DestructorTracker(DestructorTracker&& other) noexcept : value(other.value) {
            ++s_ActiveCount;
        }

        DestructorTracker& operator=(const DestructorTracker& other) {
            value = other.value;
            return *this;
        }

        DestructorTracker& operator=(DestructorTracker&& other) noexcept {
            value = other.value;
            return *this;
        }

        ~DestructorTracker() {
            --s_ActiveCount;
        }
    };

    int DestructorTracker::s_ActiveCount = 0;

    bool RunContainerTests() noexcept {
        LOG_INFO("=== Running Kernel Container Tests ===");

        // Setup custom allocator for container tests
        constexpr size_t ARENA_SIZE = 1024 * 1024; // 1 MB
        eng::memory::FreeListAllocator customAlloc(ARENA_SIZE);

        // ---------------------------------------------------------
        // 1. DynamicArray Tests
        // ---------------------------------------------------------
        {
            LOG_INFO("[Test] Starting DynamicArray Tests...");
            size_t initialMemory = customAlloc.GetUsedMemory();

            DestructorTracker::s_ActiveCount = 0;
            {
                eng::containers::DynamicArray<DestructorTracker> arr(&customAlloc);
                assert(arr.Size() == 0);
                assert(arr.Capacity() == 0);

                // Push elements
                arr.PushBack(DestructorTracker(10));
                arr.PushBack(DestructorTracker(20));
                arr.EmplaceBack(30);

                assert(arr.Size() == 3);
                assert(arr.Capacity() >= 3);
                assert(DestructorTracker::s_ActiveCount == 3);

                assert(arr[0].value == 10);
                assert(arr[1].value == 20);
                assert(arr[2].value == 30);

                // Resize tests
                arr.Resize(5, DestructorTracker(50));
                assert(arr.Size() == 5);
                assert(arr[4].value == 50);
                assert(DestructorTracker::s_ActiveCount == 5);

                arr.Resize(2);
                assert(arr.Size() == 2);
                assert(DestructorTracker::s_ActiveCount == 2);

                // Copy constructor
                {
                    eng::containers::DynamicArray<DestructorTracker> copyArr(arr);
                    assert(copyArr.Size() == 2);
                    assert(copyArr[0].value == 10);
                    assert(DestructorTracker::s_ActiveCount == 4);
                }
                assert(DestructorTracker::s_ActiveCount == 2);

                // Erase test
                arr.Erase(arr.begin());
                assert(arr.Size() == 1);
                assert(arr[0].value == 20);
                assert(DestructorTracker::s_ActiveCount == 1);
            }

            // All elements destroyed, memory freed
            assert(DestructorTracker::s_ActiveCount == 0);
            assert(customAlloc.GetUsedMemory() == initialMemory);
            LOG_INFO("[Test] DynamicArray Tests Passed.");
        }

        // ---------------------------------------------------------
        // 2. HashMap Tests
        // ---------------------------------------------------------
        {
            LOG_INFO("[Test] Starting HashMap Tests...");
            size_t initialMemory = customAlloc.GetUsedMemory();

            {
                eng::containers::HashMap<int, std::string> map(&customAlloc);
                assert(map.Size() == 0);

                map.Insert(1, "One");
                map.Insert(2, "Two");
                map.Insert(3, "Three");

                assert(map.Size() == 3);
                assert(map[1] == "One");
                assert(map[2] == "Two");
                assert(map[3] == "Three");

                // Overwrite value
                map.Insert(2, "Second");
                assert(map[2] == "Second");

                // Find tests
                auto it = map.Find(3);
                assert(it != map.end());
                assert((*it).second == "Three");

                auto itFail = map.Find(99);
                assert(itFail == map.end());

                // Erase test
                bool erased = map.Erase(2);
                assert(erased);
                assert(map.Size() == 2);
                assert(map.Find(2) == map.end());

                // operator[] auto insert
                map[5] = "Five";
                assert(map.Size() == 3);
                assert(map[5] == "Five");

                // Quadratic probing / rehashing test
                // Insert many items to trigger multiple growth cycles
                for (int i = 10; i < 100; ++i) {
                    map.Insert(i, "Value_" + std::to_string(i));
                }
                assert(map.Size() == 93);
                assert(map[50] == "Value_50");
            }

            assert(customAlloc.GetUsedMemory() == initialMemory);
            LOG_INFO("[Test] HashMap Tests Passed.");
        }

        // ---------------------------------------------------------
        // 3. String & StringView Tests
        // ---------------------------------------------------------
        {
            LOG_INFO("[Test] Starting String & StringView Tests...");
            size_t initialMemory = customAlloc.GetUsedMemory();

            {
                eng::containers::String s1("Hello", &customAlloc);
                assert(s1.Size() == 5);
                assert(std::strcmp(s1.CStr(), "Hello") == 0);

                eng::containers::String s2(" World", &customAlloc);
                s1 += s2;
                assert(s1.Size() == 11);
                assert(std::strcmp(s1.CStr(), "Hello World") == 0);

                // StringView comparisons
                eng::containers::StringView sv(s1);
                assert(sv.Size() == 11);
                assert(sv[0] == 'H');

                eng::containers::StringView svSub = sv.Substring(6, 5);
                assert(svSub.Compare("World") == 0);
                assert(svSub == "World");

                eng::containers::String s3(svSub, &customAlloc);
                assert(s3 == "World");
            }

            assert(customAlloc.GetUsedMemory() == initialMemory);
            LOG_INFO("[Test] String & StringView Tests Passed.");
        }

        LOG_INFO("=== All Container Tests Passed Successfully ===");
        return true;
    }

} // namespace eng::runtime
