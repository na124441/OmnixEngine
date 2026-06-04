#pragma once
#include <vector>
#include <algorithm>
#include <string>
#include <sstream>
#include <iomanip>
#include <cstdint>
#include "RenderObject.h"
#include "Core/Engine/Log.h"

namespace eng::renderer {

/// Small struct that is cheap to copy – it contains only raw pointers
/// and a transform matrix.  It is built from a RenderObject each frame.
struct RenderItem {
    Mesh*       mesh     = nullptr;   // non‑owning
    Material*   material = nullptr;   // non‑owning
    glm::mat4   transform = glm::mat4(1.0f);
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
