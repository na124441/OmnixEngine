# GPU Frustum Culling Pass

This document details the design and implementation of the GPU-driven frustum culling system added in Milestone 3.

## 1. Overview and Theory

Moving frustum culling from the CPU to the GPU leverages the massive parallel processing capabilities of the graphics card. Instead of sequentially checking each renderable instance's bounding sphere on the CPU, we launch one GPU compute thread per instance to do the frustum intersection checks concurrently.

### Compute Shader Logic (`frustum_cull.comp`)
For each instance, the thread:
1. Loads the instance's bounds center and radius from `InstanceBuffer` (Binding 0).
2. Tests the bounding sphere against the 6 camera frustum planes stored in `FrustumBuffer` (Binding 1).
3. If the instance is visible (i.e. not completely outside any of the 6 planes):
   - Atomically increments the `visibleCount` counter in `VisibleCountBuffer` (Binding 3).
   - Appends the original `instanceIndex` to `VisibleInstanceBuffer` (Binding 2) at the slot returned by the atomic increment.

---

## 2. Resource Allocation and Layouts

### Buffers
We allocate two visibility buffers per frame-in-flight:
1. **`VisibleInstanceBuffer`**:
   - Usage: `VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT`
   - Memory: `VMA_MEMORY_USAGE_GPU_ONLY`
   - Size: Dynamically resized to match the active frame's GPUScene instance capacity.
2. **`VisibleCountBuffer`**:
   - Usage: `VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT`
   - Memory: `VMA_MEMORY_USAGE_CPU_TO_GPU` (host-visible/coherent for direct CPU mapping and readback).
   - Size: 4 bytes (holds a single `uint32_t` counter).

### Descriptors (Set 0)
* **Binding 0**: `InstanceBuffer` (Storage Buffer, Read-Only)
* **Binding 1**: `FrustumBuffer` (Uniform Buffer, Read-Only)
* **Binding 2**: `VisibleInstanceBuffer` (Storage Buffer, Write-Only)
* **Binding 3**: `VisibleCountBuffer` (Storage Buffer, Read-Write)

---

## 3. Execution and Synchronization

The `FrustumCullPass` is integrated directly into the `RenderGraph` as a compute pass executing at the beginning of each frame (registered under slot `PassID::Shadow`).

### Command Sequence:
1. **Clear Count**:
   Clear the `visibleCountBuffer` atomic counter to 0:
   ```cpp
   vkCmdFillBuffer(cmd, visibleCountBuffer, 0, 4, 0);
   ```
2. **Transfer to Compute Barrier**:
   Add a pipeline barrier to ensure the buffer clear completes before compute shader execution starts:
   ```cpp
   VkBufferMemoryBarrier clearBarrier{};
   clearBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
   clearBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
   clearBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
   clearBarrier.buffer = visibleCountBuffer;
   // ...
   vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, ...);
   ```
3. **Dispatch Compute Shader**:
   Bind the pipeline, descriptor set, push constants (instance count), and dispatch:
   ```cpp
   uint32_t groupCount = (instanceCount + 63) / 64;
   vkCmdDispatch(cmd, groupCount, 1, 1);
   ```
4. **Compute to Host / Transfer Barrier**:
   Add a pipeline barrier to transition culling pass writes to be visible to future passes or host readbacks:
   ```cpp
   VkBufferMemoryBarrier barriers[2]{};
   // barrier 0: visibleInstanceBuffer
   // barrier 1: visibleCountBuffer (dstAccessMask |= VK_ACCESS_HOST_READ_BIT)
   vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_HOST_BIT, ...);
   ```

---

## 4. Diagnostics and Verification

### Host Readback Synchronization
To avoid CPU-GPU stalls, we query the statistics from the *previous* frame resources of the same index. At the beginning of `Renderer::BeginFrame()`, we wait on the current frame's fence:
```cpp
vkWaitForFences(resources.device, 1, &resources.inFlightFences[frameIndex], VK_TRUE, UINT64_MAX);
```
Once the fence is signaled, the GPU has finished executing all commands for that frame index. We can safely map the coherent count buffer on the host and copy the final visibility count without introducing any driver stalls:
```cpp
uint32_t gpuCount = m_FrustumCullPass.ReadBackVisibleCount(resources, frameIndex);
```

### Editor Diagnostics UI
The statistics are rendered in real-time under the Viewport Diagnostics overlay:
```text
Visible Meshes: 10          <-- CPU Culling Count (if CPU Culling is ON)
GPU Visible Meshes: 10      <-- GPU Culling Count (always updates dynamically)
```
Toggling CPU culling ON/OFF will change `Visible Meshes` to show either the CPU-culled count or total count, while `GPU Visible Meshes` always updates in real-time based on camera position and frustum intersection checks running on the GPU.
