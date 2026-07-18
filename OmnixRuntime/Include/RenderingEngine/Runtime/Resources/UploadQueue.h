#pragma once
#include <queue>
#include <mutex>
#include "rhi/RHI.h"
#include "core/memory/RingAllocator.h"

namespace eng::runtime {

    /**
     * @class UploadQueue
     *
     * Thread‑safe FIFO queue of *GPU upload commands*.
     *
     * Each entry consists of:
     *    - Source CPU buffer (pointer + size)
     *    - Destination RHI buffer or texture handle
     *    - Optional row‑pitch / image‑layout information.
     *
     * The `EngineLoop` drains the queue once per frame (just before the
     * FrameGraph executes).  Drainage is performed on the *main thread* because
     * Vulkan command pools are typically thread‑affine.
     *
     * The queue lives for the entire lifetime of the engine (it does not reset
     * each frame).  The **CPU side memory** used for staging buffers is allocated
     * from a *ring allocator* that lives inside `FrameResources` – each frame
     * we advance the ring generation and reuse the same mapped memory.
     */
    class UploadQueue {
    public:
        struct UploadCommand {
            RHI::BufferHandle dstBuffer;
            const void* srcData;      // pointer into a staging buffer
            size_t            sizeBytes;
            size_t            dstOffset;   // optional offset into dstBuffer
        };

        explicit UploadQueue(RHI::Device* rhi);
        ~UploadQueue();

        /** Enqueue a copy from CPU memory to a GPU buffer. */
        void Enqueue(const UploadCommand& cmd);

        /** Drain the queue, recording all copies into the supplied command list. */
        void Flush(RHI::CommandList* cmdList,
            RingAllocator& stagingAllocator); // uses a ring for temporary staging

        /** Clear all pending uploads (used on shutdown). */
        void Reset();

    private:
        RHI::Device* m_RHI;
        std::mutex              m_Mutex;
        std::vector<UploadCommand> m_Pending;
    };

} // namespace eng::runtime
