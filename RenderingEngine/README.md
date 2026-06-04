# Omnix Rendering Engine

Welcome to the **Omnix Rendering Engine**, a highly optimized, modern, and robust 3D Graphics pipeline powered directly by the **Vulkan Graphics API**. This engine is designed for high performance, utilizing low-level memory management, explicit synchronization, and a custom Render Hardware Interface (RHI).

## 🏗️ Architecture Overview

The Rendering Engine is divided into several distinct layers to ensure modularity and maintainability:

### 1. RHI (Render Hardware Interface)
The RHI is the foundational layer that talks directly to the GPU. It provides an abstraction over raw Vulkan API calls, allowing the higher-level engine to manage GPU resources without dealing with extreme Vulkan verbosity.
- **VulkanInstance**: Bootstraps the Vulkan API, loads the Validation Layers for debugging, and creates the window surface.
- **VulkanDevice**: Manages the Physical Device (GPU selection) and Logical Device, mapping queue families for Graphics and Presentation operations.
- **VulkanSwapChain**: Handles the swapchain images, image views, and formats required to present rendered frames directly to the OS window manager.

### 2. Runtime & Engine Loop
The `EngineLoop` is the heartbeat of the application. It manages the continuous execution of the engine frame-by-frame.
- **Frame Context & Double Buffering**: The engine utilizes `m_MaxFramesInFlight = 2` to allow the CPU to process the next frame while the GPU is still rendering the current one.
- **Swapchain Synchronization**: The engine implements industry-standard `m_ImagesInFlight` tracking. This guarantees that `VkSemaphore` objects are never reused while still in flight, preventing validation errors during triple-buffering.

### 3. Pipeline & Renderers
The rendering layer translates geometric data into visual pixels on the screen via the GPU pipeline.
- **Graphics Pipeline**: Configures fixed-function blocks, including dynamic viewports, rasterization, and multisampling.
- **Shader Compilation**: GLSL shaders (`.vert` and `.frag`) are automatically compiled into SPIR-V bytecode at build-time using `glslc`.

---

## 🛠️ Evolution: Phase 0 to Phase 1

The engine has undergone a significant architectural transformation to move from a hardcoded demo to a generalized 3D renderer.

### Phase 0: The Baseline Primitive
**Objective**: Establish a stable Vulkan context and render a rotating pyramid.
*   **Hardcoded Geometry**: Used a static vertex/index buffer for a 5-sided pyramid.
*   **Static Transformations**: Shaders used a simplified projection hack specifically designed for objects at the origin.
*   **Manual Buffering**: Managed a single set of Uniform Buffers shared across the entire pass.

### Phase 1: The General Scene Renderer
**Objective**: Enable arbitrary 3D model loading and generalized scene management.
*   **OBJ Model Loading**: Implemented a robust `.obj` parser (`ModelLoader`) that converts text geometry into Vulkan `Mesh` objects with support for vertex duplication.
*   **Mesh & RenderObject Abstraction**: Introduced a scene hierarchy where multiple `RenderObject`s (Mesh + Material + Transform) can coexist.
*   **Standard MVP Pipeline**: Replaced hardcoded projection hacks with a production-grade **Model-View-Projection** system in the vertex shader.
*   **Push Constant Integration**: Optimized per-object updates using Vulkan **Push Constants**. This allows the engine to send unique transformation matrices to the GPU for every object without re-binding Descriptor Sets.
*   **Auto-Normalization System**: Implemented an automated pre-render pass during model loading:
    *   **Centering**: Automatically offsets models so their geometric center sits at `(0,0,0)`.
    *   **Scaling**: Detects the model's bounding box and rescales it to a standard viewing size (~10 units).

---

## ⚙️ The Rendering Lifecycle

For every single frame, the following sequence of operations occurs in `EngineLoop::BuildAndExecuteGraph()`:

1.  **Synchronization**: CPU waits for the current frame's fence (`vkWaitForFences`).
2.  **Acquisition**: Acquires the next swapchain image index.
3.  **UBO Update**: `PyramidRenderer::Update` calculates the new Camera View and Perspective matrices.
4.  **Command Recording**:
    *   **Dynamic State**: Viewports and Scissors are set based on the current window size.
    *   **Push Constants**: For each `RenderObject`, the engine "pushes" its unique `mat4` transform to the GPU.
    *   **Draw Call**: `vkCmdDrawIndexed` is issued for the mesh.
5.  **Submission**: Dispatches the command buffer to the Graphics Queue.
6.  **Presentation**: Displays the frame on the monitor once the rendering semaphore signals completion.

---

## 🚀 Building and Extending

To add new features to the rendering engine:
1.  **New Models**: Add `.obj` files to `RenderingEngine/` and update `CUSTOM_MODEL_PATH` in `EngineLoop.h`.
2.  **Shaders**: Edit `shader.vert` or `shader.frag`; CMake will automatically recompile them to SPIR-V.
3.  **Materials**: Create new `MaterialInstance` objects to map different pipelines to your meshes.
