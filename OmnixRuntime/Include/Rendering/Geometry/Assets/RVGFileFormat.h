#pragma once
#include <cstdint>

namespace eng::renderer {

#pragma pack(push, 1)

struct RVGHeader {
    char magic[8] = {'O', 'M', 'N', 'I', 'X', 'R', 'V', 'G'};
    uint32_t version = 1;
    uint32_t sectionCount = 0;
    uint32_t flags = 0;
};

enum class RVGSectionType : uint32_t {
    Metadata = 0,
    Materials = 1,
    Hierarchy = 2,
    Clusters = 3,
    Pages = 4,
    FallbackMesh = 5,
    DebugMetadata = 6
};

struct RVGSectionEntry {
    uint32_t type = 0; // RVGSectionType value
    uint64_t offset = 0;
    uint64_t size = 0;
    uint32_t checksum = 0;
    uint32_t reserved = 0;
};

#pragma pack(pop)

} // namespace eng::renderer
