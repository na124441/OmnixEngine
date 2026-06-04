#pragma once
#include <cstddef>
#include <vector>

namespace eng::memory {

    class FragmentationDiagnostics {
    public:
        /**
         * @brief Calculate fragmentation percentage based on free block sizes.
         * Formula: (1.0 - (LargestFreeBlock / TotalFreeMemory)) * 100.0
         */
        static float CalculateFragmentation(const std::vector<size_t>& freeBlocks) {
            if (freeBlocks.empty()) return 0.0f;
            size_t totalFree = 0;
            size_t largestFree = 0;
            for (size_t block : freeBlocks) {
                totalFree += block;
                if (block > largestFree) {
                    largestFree = block;
                }
            }
            if (totalFree == 0) return 0.0f;
            return (1.0f - (static_cast<float>(largestFree) / static_cast<float>(totalFree))) * 100.0f;
        }
    };

} // namespace eng::memory
