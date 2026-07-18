#pragma once
#include "runtime/resources/UploadQueue.h"
#include "rhi/RHI.h"

namespace eng::runtime {

    /**
     * @class GPUTransfer
     *
     * Convenience wrapper that combines a staging allocation + upload command
     * creation in one call.  Used by the `SceneBuilder` and by any runtime code
     * that needs to push data to the GPU (e.g., updating a dynamic uniform buffer).
     *
     * The function allocates the required temporary memory from the *frame ring
     * allocator* (so the allocation lives only until the GPU finishes reading it).
     */
    class GPUTransfer {
    public:
        static Result BufferUpload(RHI::Device* rhi,
            UploadQueue& uploadQueue,
            RingAllocator& ring,
            RHI::BufferHandle dst,
            const void* src,
            size_t sizeBytes,
            size_t dstOffset = 0);
    };

} // namespace eng::runtime
