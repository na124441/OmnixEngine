#include "Rendering/Geometry/Arena/GeometryAllocator.h"
#include <algorithm>

namespace eng::renderer {

void GeometryAllocator::Initialize(uint64_t totalSize) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_TotalSize = totalSize;
    m_UsedSize = 0;
    m_HighWaterMark = 0;
    m_AllocationCount = 0;
    m_FreeBlocks.clear();
    if (totalSize > 0) {
        m_FreeBlocks.push_back({0, totalSize});
    }
}

bool GeometryAllocator::Allocate(uint64_t size, uint64_t alignment, uint64_t& outOffset) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (size == 0) return false;

    for (auto it = m_FreeBlocks.begin(); it != m_FreeBlocks.end(); ++it) {
        uint64_t blockOffset = it->offset;
        uint64_t blockSize = it->size;

        uint64_t alignedOffset = blockOffset;
        if (alignment > 0 && blockOffset % alignment != 0) {
            alignedOffset = blockOffset + (alignment - (blockOffset % alignment));
        }

        uint64_t padding = alignedOffset - blockOffset;
        if (blockSize >= size + padding) {
            outOffset = alignedOffset;
            uint64_t remainingSize = blockSize - (size + padding);

            if (padding > 0) {
                // Keep the padding part as the current free block
                it->size = padding;
                if (remainingSize > 0) {
                    // Insert remaining part after the allocated space
                    m_FreeBlocks.insert(it + 1, {alignedOffset + size, remainingSize});
                }
            } else {
                if (remainingSize > 0) {
                    // Just shrink the current free block to the remaining part
                    it->offset = alignedOffset + size;
                    it->size = remainingSize;
                } else {
                    // Remove the free block entirely
                    m_FreeBlocks.erase(it);
                }
            }

            m_UsedSize += size;
            m_AllocationCount++;
            m_HighWaterMark = std::max(m_HighWaterMark, m_UsedSize);
            return true;
        }
    }

    return false;
}

void GeometryAllocator::Free(uint64_t offset, uint64_t size) {
    if (size == 0) return;
    std::lock_guard<std::mutex> lock(m_Mutex);

    m_FreeBlocks.push_back({offset, size});
    std::sort(m_FreeBlocks.begin(), m_FreeBlocks.end(), [](const FreeBlock& a, const FreeBlock& b) {
        return a.offset < b.offset;
    });

    MergeFreeBlocks();

    if (m_UsedSize >= size) {
        m_UsedSize -= size;
    } else {
        m_UsedSize = 0;
    }
    if (m_AllocationCount > 0) {
        m_AllocationCount--;
    }
}

void GeometryAllocator::Grow(uint64_t newTotalSize) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (newTotalSize <= m_TotalSize) return;

    uint64_t addedSize = newTotalSize - m_TotalSize;
    m_FreeBlocks.push_back({m_TotalSize, addedSize});
    m_TotalSize = newTotalSize;

    std::sort(m_FreeBlocks.begin(), m_FreeBlocks.end(), [](const FreeBlock& a, const FreeBlock& b) {
        return a.offset < b.offset;
    });

    MergeFreeBlocks();
}

uint64_t GeometryAllocator::GetTotalSize() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_TotalSize;
}

uint64_t GeometryAllocator::GetUsedSize() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_UsedSize;
}

uint64_t GeometryAllocator::GetFreeSize() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_TotalSize - m_UsedSize;
}

double GeometryAllocator::GetFragmentation() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (m_FreeBlocks.empty()) return 0.0;

    uint64_t largestFreeBlock = 0;
    uint64_t totalFree = 0;
    for (const auto& block : m_FreeBlocks) {
        totalFree += block.size;
        largestFreeBlock = std::max(largestFreeBlock, block.size);
    }

    if (totalFree == 0) return 0.0;
    return (1.0 - (double)largestFreeBlock / totalFree) * 100.0;
}

uint64_t GeometryAllocator::GetHighWaterMark() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_HighWaterMark;
}

uint32_t GeometryAllocator::GetAllocationCount() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_AllocationCount;
}

void GeometryAllocator::MergeFreeBlocks() {
    if (m_FreeBlocks.size() < 2) return;

    for (size_t i = 0; i < m_FreeBlocks.size() - 1; ) {
        if (m_FreeBlocks[i].offset + m_FreeBlocks[i].size == m_FreeBlocks[i+1].offset) {
            m_FreeBlocks[i].size += m_FreeBlocks[i+1].size;
            m_FreeBlocks.erase(m_FreeBlocks.begin() + i + 1);
        } else {
            i++;
        }
    }
}

} // namespace eng::renderer
