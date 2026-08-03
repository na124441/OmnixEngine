#pragma once
#include "core/memory/LinearAllocator.h"
#include "core/memory/RingAllocator.h"
#include "core/memory/PoolAllocator.h"
#include "rhi/RHI.h"

namespace frame {

    /**
     * @brief Holds all transient GPU‑side allocations needed for a single frame.
     *
     * The lifetime of this object is **exactly one frame** – it is cleared
     * (reset) by `FrameScheduler::EndFrame` after the GPU work has finished.
     *
     * The memory model is:
     *   - `LinearAllocator`  → for one‑shot buffers that are only read
     *                            by the GPU (e.g., per‑frame instance data).
     *   - `RingAllocator`     → for dynamic uniform/storage buffers that are
     *                            written each frame and may be read by the GPU
     *                            for several subsequent frames (double‑buffered
     *                            to avoid overwriting still‑in‑flight data).
     *   - `PoolAllocator<Job>` → for per‑frame `Job` objects (if the job system
     *                            creates frame‑local jobs).
     *
     * All GPU resources (buffers, textures) are created via the RHI, but the **
     * underlying memory** is allocated from a **RHIResourcePool** that the
     * `RHI::Device` owns. `FrameResources` only stores **handles** to those
     * GPU objects; it does **not** own Vulkan objects directly.
     */
    class FrameResources {
    public:
        explicit FrameResources(RHI::Device* rhiDevice);
        ~FrameResources();

        // ----- Linear / Ring / Pool allocators -----
        LinearAllocator& GetLinearAllocator()  noexcept { return linear_; }
        RingAllocator& GetRingAllocator()    noexcept { return ring_; }
        // The pool allocator is used internally for transient jobs, not exposed.

        // ----- Helper creating RHI resources that are automatically reclaimed –
        //      they live until `Reset()` is called.                                   --
        Result CreateTransientBuffer(const RHI::BufferDesc& desc,
            RHI::BufferHandle& outHandle);
        Result CreateTransientTexture(const RHI::TextureDesc& desc,
            RHI::TextureHandle& outHandle);
        // The implementation forwards to `rhiDevice->CreateBuffer` using memory from
        // `linear_` or `ring_` and registers the handle for later deletion.

        /** Reset all transient allocations – called once per frame after the GPU is done. */
        void Reset();

    private:
        RHI::Device* m_Device;        // not owned (owned by EngineLoop)
        LinearAllocator           m_Linear{ nullptr, 0 };
        RingAllocator             m_Ring{ nullptr, 0 };
        // Internally we keep a vector of the temporary RHI handles so we can release them.
        std::vector<RHI::BufferHandle>   m_TempBuffers;
        std::vector<RHI::TextureHandle>  m_TempTextures;
    };

} // namespace eng::runtime
