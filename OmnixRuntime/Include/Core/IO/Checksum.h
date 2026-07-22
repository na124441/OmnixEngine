#pragma once
#include <cstdint>
#include <cstddef>

namespace eng::core {

    inline uint64_t ComputeFileChecksum(const uint8_t* fileBuffer, size_t fileSize) noexcept {
        if (fileSize < 32) { // Minimum FileHeader size
            return 0;
        }
        
        // Hash FNV-1a of the buffer, treating the checksum field (offset 16, 8 bytes) in the header as 0
        uint64_t hash = 14695981039346656037ULL;
        for (size_t i = 0; i < fileSize; ++i) {
            uint8_t byteVal = fileBuffer[i];
            // The checksum field spans indices [16, 23] inclusive inside FileHeader
            if (i >= 16 && i < 24) {
                byteVal = 0;
            }
            hash ^= static_cast<uint64_t>(byteVal);
            hash *= 1099511628211ULL;
        }
        return hash;
    }

} // namespace eng::core
