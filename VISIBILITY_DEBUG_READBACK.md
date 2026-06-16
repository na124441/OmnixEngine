# Visibility Debug Readback and Diagnostic System

This document outlines the visual culling diagnostics and bounds visualization tools added in Milestone 4.

## 1. GPU Readback Mechanism

To visualize culling and verify correctness, we read back both the visibility count and the list of visible instance indices from the GPU to the CPU every frame.

### CPU-GPU Synchronization
Rather than stalling the render loop by mapping memory immediately after compute dispatch, we read back visibility results from the frame resources belonging to the *current* frame index at the start of `Renderer::BeginFrame()`.
1. At the beginning of the frame, the CPU waits for the fence of the current frame index:
   ```cpp
   vkWaitForFences(resources.device, 1, &resources.inFlightFences[frameIndex], VK_TRUE, UINT64_MAX);
   ```
2. Once the fence is signaled, all GPU commands for this frame resources have finished executing.
3. We safely map the host-visible culling count buffer and indices buffer:
   * **`ReadBackVisibleCount`**: Reads the final visible instance count.
   * **`ReadBackVisibleInstances`**: Reads the array of visible instance indices.

---

## 2. CPU Reference Culling Path

To compare and validate GPU results, we maintain a CPU-side reference culling calculation in `Renderer::buildRenderQueue()`.
1. The CPU extracts frustum planes from the active camera view-projection matrix.
2. It tests every mesh instance in the scene against the 6 planes.
3. It accumulates:
   * **`m_TotalInstanceCount`**: The total count of renderable instances in the scene.
   * **`m_CpuRefVisibleCount`**: The number of instances passing CPU culling.

This CPU reference culling runs unconditionally in the background, providing a baseline comparison regardless of whether CPU culling is actually used to filter drawing calls.

---

## 3. Editor Diagnostics Overlay

The culling metrics are displayed under the **View -> Show Diagnostics** overlay panel in the editor viewport:

```text
--- GPU Culling Diagnostics ---
Total Instances: 1000
GPU Visible: 243
CPU Ref Visible: 245
Difference: 2
```

### Analyzing Differences:
* A **Difference** of 0 means perfect alignment between GPU and CPU culling.
* Small differences (e.g., $\le 5$) are expected due to floating-point precision differences between host and device bounds transformation math or frustum projection boundaries.
* Large differences (e.g., $>50$) indicate culling bugs, such as plane extraction mismatch, unit matrix issues, or buffer indexing corruption.

---

## 4. Debug Bounds Visualization

When the **View -> Show GPU Bounds** option is enabled, the editor draws wireframe debug spheres around the instances in the scene:
1. It queries the list of GPU-visible instance indices read back in the current frame.
2. For each instance `i` in the scene:
   * If `i` is present in the GPU-visible list, the wireframe bounding sphere is colored **green** (`glm::vec4(0.0f, 1.0f, 0.0f, 1.0f)`).
   * If `i` is absent from the list, the wireframe bounding sphere is colored **red** (`glm::vec4(1.0f, 0.0f, 0.0f, 1.0f)`).

### Visual Diagnosis:
* Objects in front of the camera display green spheres.
* Moving or rotating the camera away from objects causes their spheres to instantly turn red.
* Bringing objects back into view instantly reverts their spheres to green.
