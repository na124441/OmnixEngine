# Render Target Manager

A centralized system for owning and managing Vulkan render target lifecycles, layouts, formats, and extents inside the Radiance renderer.

---

## 1. System Architecture

The Render Target Manager replaces distributed Vulkan image creation/destruction with a single owner class. This eliminates mismatches in swapchain/viewport extents and incorrect layout transitions.

### Key Types

#### `RenderTargetDesc`
Specifies all configuration data required to allocate a Vulkan image:
- `width` / `height`: Dimensions
- `format`: `VkFormat`
- `usage`: `VkImageUsageFlags`
- `aspect`: `VkImageAspectFlags`
- `colorAttachment` / `depthAttachment` / `storage` / `sampled`: Usage hints
- `debugName`: Custom string for Vulkan debugging tools

#### `RenderTarget`
Holds the active Vulkan resource handles:
- `VkImage` / `VkImageView`
- `VmaAllocation`
- Cached metadata: format, layout, extent, aspect, usage, version (monotonically increasing on recreation), debug name.

#### `RenderTargetHandle`
A generational handle to a target:
- `index`: Index in the manager's target pool
- `generation`: Incremented on destruction to invalidate stale handle references

---

## 2. Layout Tracking & Validation

The manager tracks the `currentLayout` of each managed target. 

### Transitions
Transitions are performed centrally using:
```cpp
void RenderTargetManager::Transition(
    VkCommandBuffer cmd,
    RenderTargetHandle handle,
    VkImageLayout newLayout,
    VkPipelineStageFlags srcStage,
    VkPipelineStageFlags dstStage,
    VkAccessFlags srcAccess,
    VkAccessFlags dstAccess
);
```
This automatically updates `currentLayout` upon executing the pipeline barrier.

### Assertions
Render graph passes validate their input layouts prior to execution using:
```cpp
void AssertLayout(RenderTargetHandle handle, VkImageLayout expected);
```
If a layout mismatch occurs, a diagnostic warning is logged showing the expected vs. actual layout of the target.

---

## 3. Verification & Test Results

### 3.1 Startup Unit Tests
During startup in `Renderer::Initialize()`, the manager creates and destroys test color/depth targets to verify memory allocations and generational indices.

#### Log Output
```
[16:24:56.727][Runtime][Info] UNIT TEST: Starting RenderTargetManager creation/destruction verification...
[16:24:56.846][Runtime][Info] UNIT TEST: Color target created successfully.
[16:24:56.847][Runtime][Info] UNIT TEST: Depth target created successfully.
[16:24:56.847][Runtime][Info] ================= Render Target Manager Dump =================
[16:24:56.848][Runtime][Info] Target [0] Debug Name: 'UnitTest_ColorTarget'
[16:24:56.848][Runtime][Info]   Size:         128x128
[16:24:56.848][Runtime][Info]   Format:       37 (VK_FORMAT_R8G8B8A8_UNORM)
[16:24:56.848][Runtime][Info]   Usage:        20 (COLOR_ATTACHMENT | SAMPLED)
[16:24:56.848][Runtime][Info]   Layout:       0 (UNDEFINED)
[16:24:56.848][Runtime][Info]   Version:      1
[16:24:56.848][Runtime][Info] Target [1] Debug Name: 'UnitTest_DepthTarget'
[16:24:56.848][Runtime][Info]   Size:         128x128
[16:24:56.848][Runtime][Info]   Format:       126 (VK_FORMAT_D32_SFLOAT)
[16:24:56.849][Runtime][Info]   Usage:        32 (DEPTH_STENCIL_ATTACHMENT)
[16:24:56.849][Runtime][Info]   Layout:       0 (UNDEFINED)
[16:24:56.849][Runtime][Info]   Version:      1
[16:24:56.849][Runtime][Info] ==============================================================
[16:24:56.850][Runtime][Info] UNIT TEST: Color/Depth target destruction completed.
```

---

## 4. Resource Allocation Status
All active engine targets are now allocated through the `RenderTargetManager`:
1. `DepthBuffer`
2. `GBufferA` / `GBufferB` / `GBufferC` / `GBufferD`
3. `HDRColor`
4. `LDRColor`
5. `ViewportColor`
6. `ShadowMap`
7. `SSAO` (SSAO Foundation Target)
