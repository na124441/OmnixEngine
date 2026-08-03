#include "Core/Memory/FreeListAllocator.h"
#include "Core/Memory/AllocationTracker.h"
#include "Core/Diagnostics/Assert.h"
#include <algorithm>

namespace eng::memory {

    FreeListAllocator::FreeListAllocator(size_t capacity, AllocationPolicy policy)
        : m_Capacity(capacity), m_Policy(policy), m_OwnsMemory(true) {
        m_Base = new uint8_t[capacity];
        AllocationTracker::RegisterAllocation(m_Base, m_Capacity, "FreeListAllocator");
        Reset();
    }

    FreeListAllocator::FreeListAllocator(void* base, size_t capacity, AllocationPolicy policy)
        : m_Base(static_cast<uint8_t*>(base)), m_Capacity(capacity), m_Policy(policy), m_OwnsMemory(false) {
        Reset();
    }

    FreeListAllocator::~FreeListAllocator() {
        if (m_OwnsMemory && m_Base) {
            AllocationTracker::RegisterDeallocation(m_Base);
            delete[] m_Base;
            m_Base = nullptr;
        }
    }

    FreeListAllocator::FreeListAllocator(FreeListAllocator&& other) noexcept
        : m_Base(other.m_Base),
          m_Capacity(other.m_Capacity),
          m_UsedMemory(other.m_UsedMemory),
          m_PeakUsage(other.m_PeakUsage),
          m_OwnsMemory(other.m_OwnsMemory),
          m_FreeBlocks(other.m_FreeBlocks),
          m_Policy(other.m_Policy) {
        other.m_Base = nullptr;
        other.m_Capacity = 0;
        other.m_UsedMemory = 0;
        other.m_PeakUsage = 0;
        other.m_OwnsMemory = false;
        other.m_FreeBlocks = nullptr;
    }

    FreeListAllocator& FreeListAllocator::operator=(FreeListAllocator&& other) noexcept {
        if (this != &other) {
            if (m_OwnsMemory && m_Base) {
                AllocationTracker::RegisterDeallocation(m_Base);
                delete[] m_Base;
            }
            m_Base = other.m_Base;
            m_Capacity = other.m_Capacity;
            m_UsedMemory = other.m_UsedMemory;
            m_PeakUsage = other.m_PeakUsage;
            m_OwnsMemory = other.m_OwnsMemory;
            m_FreeBlocks = other.m_FreeBlocks;
            m_Policy = other.m_Policy;

            other.m_Base = nullptr;
            other.m_Capacity = 0;
            other.m_UsedMemory = 0;
            other.m_PeakUsage = 0;
            other.m_OwnsMemory = false;
            other.m_FreeBlocks = nullptr;
        }
        return *this;
    }

    void* FreeListAllocator::Allocate(size_t size, size_t alignment) {
        Node* prevNode = nullptr;
        Node* bestNode = nullptr;
        Node* bestPrev = nullptr;
        size_t bestPadding = 0;

        Node* currentNode = m_FreeBlocks;

        while (currentNode) {
            size_t nodeAddr = reinterpret_cast<size_t>(currentNode);
            size_t headerAddr = nodeAddr + sizeof(Header);
            size_t padding = 0;
            if ((headerAddr % alignment) != 0) {
                padding = alignment - (headerAddr % alignment);
            }
            size_t requiredSpace = padding + sizeof(Header) + size;

            if (currentNode->size >= requiredSpace) {
                if (m_Policy == AllocationPolicy::FindFirst) {
                    bestNode = currentNode;
                    bestPrev = prevNode;
                    bestPadding = padding;
                    break;
                } else if (m_Policy == AllocationPolicy::FindBest) {
                    if (!bestNode || currentNode->size < bestNode->size) {
                        bestNode = currentNode;
                        bestPrev = prevNode;
                        bestPadding = padding;
                    }
                }
            }
            prevNode = currentNode;
            currentNode = currentNode->next;
        }

        if (!bestNode) {
            return nullptr;
        }

        size_t requiredSpace = bestPadding + sizeof(Header) + size;
        size_t remainingSpace = bestNode->size - requiredSpace;

        if (remainingSpace >= sizeof(Node)) {
            Node* splitNode = reinterpret_cast<Node*>(reinterpret_cast<uint8_t*>(bestNode) + requiredSpace);
            splitNode->size = remainingSpace;
            splitNode->next = bestNode->next;

            if (bestPrev) {
                bestPrev->next = splitNode;
            } else {
                m_FreeBlocks = splitNode;
            }
        } else {
            requiredSpace = bestNode->size;
            if (bestPrev) {
                bestPrev->next = bestNode->next;
            } else {
                m_FreeBlocks = bestNode->next;
            }
        }

        size_t headerAddress = reinterpret_cast<size_t>(bestNode) + bestPadding;
        Header* header = reinterpret_cast<Header*>(headerAddress);
        header->size = requiredSpace - bestPadding;
        header->padding = bestPadding;

        m_UsedMemory += requiredSpace;
        m_PeakUsage = std::max(m_PeakUsage, m_UsedMemory);

        return reinterpret_cast<void*>(headerAddress + sizeof(Header));
    }

    void FreeListAllocator::Free(void* ptr) {
        if (!ptr) return;

        Header* header = reinterpret_cast<Header*>(static_cast<uint8_t*>(ptr) - sizeof(Header));
        size_t blockStart = reinterpret_cast<size_t>(header) - header->padding;
        size_t blockSize = header->size + header->padding;

        Node* freedBlock = reinterpret_cast<Node*>(blockStart);
        freedBlock->size = blockSize;
        freedBlock->next = nullptr;

        if (!m_FreeBlocks || blockStart < reinterpret_cast<size_t>(m_FreeBlocks)) {
            freedBlock->next = m_FreeBlocks;
            m_FreeBlocks = freedBlock;
        } else {
            Node* current = m_FreeBlocks;
            while (current->next && reinterpret_cast<size_t>(current->next) < blockStart) {
                current = current->next;
            }
            freedBlock->next = current->next;
            current->next = freedBlock;
        }

        m_UsedMemory -= blockSize;

        Node* current = m_FreeBlocks;
        while (current && current->next) {
            size_t currentEnd = reinterpret_cast<size_t>(current) + current->size;
            if (currentEnd == reinterpret_cast<size_t>(current->next)) {
                current->size += current->next->size;
                current->next = current->next->next;
            } else {
                current = current->next;
            }
        }
    }

    void FreeListAllocator::Reset() {
        m_UsedMemory = 0;
        m_PeakUsage = 0;
        m_FreeBlocks = reinterpret_cast<Node*>(m_Base);
        m_FreeBlocks->size = m_Capacity;
        m_FreeBlocks->next = nullptr;
    }

} // namespace eng::memory
