#pragma once
#include <vector>
#include <memory>
#include <glm/glm.hpp>

namespace eng::renderer {

    struct AtlasNode {
        std::unique_ptr<AtlasNode> child[4]; // Quadtree split
        uint32_t x = 0;
        uint32_t y = 0;
        uint32_t size = 0;
        bool occupied = false;
        uint32_t lightID = 0xFFFFFFFF; // Owner ID
        uint32_t lastUsedFrame = 0;
    };

    class ShadowAtlasAllocator {
    public:
        ShadowAtlasAllocator(uint32_t totalSize) : m_TotalSize(totalSize) {
            Reset();
        }

        void Reset() {
            m_Root = std::make_unique<AtlasNode>();
            m_Root->size = m_TotalSize;
        }

        bool Allocate(uint32_t size, uint32_t lightID, uint32_t frameIndex, uint32_t& outX, uint32_t& outY) {
            return AllocateNode(m_Root.get(), size, lightID, frameIndex, outX, outY);
        }

        void Deallocate(uint32_t lightID) {
            DeallocateNode(m_Root.get(), lightID);
        }

    private:
        bool AllocateNode(AtlasNode* node, uint32_t size, uint32_t lightID, uint32_t frameIndex, uint32_t& outX, uint32_t& outY) {
            if (!node) return false;

            // If node has children, try allocating in children
            if (node->child[0]) {
                for (int i = 0; i < 4; ++i) {
                    if (AllocateNode(node->child[i].get(), size, lightID, frameIndex, outX, outY)) {
                        return true;
                    }
                }
                return false;
            }

            // Node is a leaf
            if (node->occupied) return false;
            if (node->size < size) return false;

            // Perfect fit
            if (node->size == size) {
                node->occupied = true;
                node->lightID = lightID;
                node->lastUsedFrame = frameIndex;
                outX = node->x;
                outY = node->y;
                return true;
            }

            // Split node
            uint32_t half = node->size / 2;
            for (int i = 0; i < 4; ++i) {
                node->child[i] = std::make_unique<AtlasNode>();
                node->child[i]->size = half;
            }
            node->child[0]->x = node->x;        node->child[0]->y = node->y;
            node->child[1]->x = node->x + half; node->child[1]->y = node->y;
            node->child[2]->x = node->x;        node->child[2]->y = node->y + half;
            node->child[3]->x = node->x + half; node->child[3]->y = node->y + half;

            return AllocateNode(node->child[0].get(), size, lightID, frameIndex, outX, outY);
        }

        void DeallocateNode(AtlasNode* node, uint32_t lightID) {
            if (!node) return;
            if (node->occupied && node->lightID == lightID) {
                node->occupied = false;
                node->lightID = 0xFFFFFFFF;
                return;
            }
            if (node->child[0]) {
                for (int i = 0; i < 4; ++i) {
                    DeallocateNode(node->child[i].get(), lightID);
                }
                // Try merge children if all are unoccupied leaves
                bool canMerge = true;
                for (int i = 0; i < 4; ++i) {
                    if (node->child[i]->child[0] || node->child[i]->occupied) {
                        canMerge = false;
                        break;
                    }
                }
                if (canMerge) {
                    for (int i = 0; i < 4; ++i) {
                        node->child[i].reset();
                    }
                }
            }
        }

        uint32_t m_TotalSize;
        std::unique_ptr<AtlasNode> m_Root;
    };

} // namespace eng::renderer
