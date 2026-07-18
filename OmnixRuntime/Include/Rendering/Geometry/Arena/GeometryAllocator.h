#pragma once
#include <cstdint>
#include <vector>
#include <mutex>

namespace eng::renderer {

struct FreeBlock {
    uint64_t offset;
    uint64_t size;
};

class GeometryAllocator {
public:
    GeometryAllocator() = default;
    ~GeometryAllocator() = default;

    void Initialize(uint64_t totalSize);
    bool Allocate(uint64_t size, uint64_t alignment, uint64_t& outOffset);
    void Free(uint64_t offset, uint64_t size);
    void Grow(uint64_t newTotalSize);

    uint64_t GetTotalSize() const;
    uint64_t GetUsedSize() const;
    uint64_t GetFreeSize() const;
    double GetFragmentation() const;
    uint64_t GetHighWaterMark() const;
    uint32_t GetAllocationCount() const;

private:
    uint64_t m_TotalSize = 0;
    uint64_t m_UsedSize = 0;
    uint64_t m_HighWaterMark = 0;
    uint32_t m_AllocationCount = 0;
    std::vector<FreeBlock> m_FreeBlocks;
    mutable std::mutex m_Mutex;

    void MergeFreeBlocks();
};

} // namespace eng::renderer
