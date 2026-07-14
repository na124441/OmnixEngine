#pragma once
#include <vector>
#include <algorithm>
#include <string>
#include <sstream>
#include <iomanip>
#include <cstdint>
#include "RenderObject.h"
#include "Core/Engine/Log.h"

#include "Rendering/Geometry/GeometryHandle.h"
#include "Runtime/Public/OmnixMeshFormat.h"

namespace eng::renderer {

struct RenderItem {
    Mesh*       mesh     = nullptr;   // non‑owning
    Material*   material = nullptr;   // non‑owning
    glm::mat4   transform = glm::mat4(1.0f);
    glm::mat4   previousTransform = glm::mat4(1.0f);
    glm::vec3   minBounds{0.0f};
    glm::vec3   maxBounds{0.0f};
    uint32_t    entityID = 0;
    bool        castShadows = true;
    uint32_t    layerMask = 1;

    // Standardized RVG v0.5 Fields
    GeometryHandle geometry;
    uint32_t       submeshIndex = 0;
    uint64_t       materialHandle = 0;
    glm::mat4      model = glm::mat4(1.0f);
    glm::mat4      previousModel = glm::mat4(1.0f);
    BoundingBox    worldBounds;
    glm::vec4      worldSphere{0.0f}; // xyz = center, w = radius
    uint32_t       objectID = 0;
    uint32_t       meshID = 0;
    uint32_t       materialID = 0;
    uint32_t       flags = 0;
};

/// Container that lives for the whole lifetime of the renderer.
class RenderQueue {
public:
    RenderQueue() = default;
    ~RenderQueue() = default;

    void clear() { items.clear(); }

    void push_back(const RenderItem& item) { items.emplace_back(item); }

    /** Sort items so that items sharing a Material are consecutive.
     *  This is a **stable** sort – it preserves the original order for
     *  items that have the same material (useful for later stable‑sort
     *  passes such as distance front‑to‑back). */
    void sortByMaterial()
    {
        std::stable_sort(items.begin(), items.end(),
            [](const RenderItem& a, const RenderItem& b)
            {
                // Pointer comparison is sufficient – the same Material* means same pipeline.
                return a.material < b.material;
            });
    }

    const std::vector<RenderItem>& getItems() const { return items; }
    std::vector<RenderItem>& getItemsMutable() { return items; }

    void sortByDistanceBackToFront(const glm::vec3& cameraPos)
    {
        std::stable_sort(items.begin(), items.end(),
            [&cameraPos](const RenderItem& a, const RenderItem& b)
            {
                glm::vec3 posA = glm::vec3(a.transform[3]);
                glm::vec3 posB = glm::vec3(b.transform[3]);
                glm::vec3 diffA = posA - cameraPos;
                glm::vec3 diffB = posB - cameraPos;
                float distSqA = glm::dot(diffA, diffA);
                float distSqB = glm::dot(diffB, diffB);
                return distSqA > distSqB;
            });
    }

    // -----------------------------------------------------------------
    // Debug helpers (enabled only in a DEBUG build)
    void logStats() const
    {
        LOG_INFO("RenderQueue: " + std::to_string(items.size()) + " items.");
        if (!items.empty()) {
            LOG_DEBUG("First material ptr = " + pointerToString(items.front().material));
            LOG_DEBUG("Last  material ptr = " + pointerToString(items.back().material));
        }
    }

private:
    std::vector<RenderItem> items;

    static std::string pointerToString(const void* p)
    {
        std::ostringstream oss;
        oss << "0x" << std::hex << reinterpret_cast<uintptr_t>(p);
        return oss.str();
    }
};

} // namespace eng::renderer
