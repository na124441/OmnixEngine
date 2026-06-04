# Subsystem Communication Rules

This document establishes the official governance rules for subsystem interaction inside **Omnix Studio Engine**. These rules are mechanically enforced to prevent future architectural decay.

---

## 🏛️ Approved Communication Patterns

All cross-subsystem communication must conform to one of the following four approved patterns. Any interaction violating these patterns will be flagged in code reviews and rejected.

```
                  ┌───────────────────────────────┐
                  │      Direct Calls             │
                  │   (Runtime -> Subsystem)      │
                  └──────────────┬────────────────┘
                                 │
  ┌──────────────────────────────┼──────────────────────────────┐
  │                              │                              │
  ▼                              ▼                              ▼
┌──────────────────┐           ┌──────────────────┐           ┌──────────────────┐
│    Event Bus     │           │     ECS Data     │           │  Command Queues  │
│  (One-to-Many)   │           │ (Simulation/Pos) │           │(Deferred/Thread) │
└──────────────────┘           └──────────────────┘           └──────────────────┘
```

---

### 1. Direct Calls (Control Flow)
* **Description**: Synchronous method invocations on a subsystem interface.
* **Allowed Context**:
  * Only allowed **downward** in the layer hierarchy (e.g., `EngineRuntime` invoking `IRenderer::Initialize()`).
  * Higher-level systems may invoke public methods of directly-owned dependencies.
* **Prohibited Context**:
  * Low-level systems must never make direct calls to high-level systems (e.g. the RHI device must never invoke the `SceneManager`).
  * Direct invocation of concrete implementations (e.g., calling `VulkanDevice` instead of `IRHIDevice`). Use interface classes.

### 2. Event Bus (Decoupled Notification)
* **Description**: Decentralized publish/subscribe message dispatching via the thread-safe `EventManager`.
* **Allowed Context**:
  * One-to-many broadcast notifications (e.g., `OnWindowResize`, `OnAssetReloaded`, `OnPlayerSpawned`).
  * Decoupling systems that do not share lifetime bounds (e.g., `InputManager` publishing keystrokes, which are read by the game's `PlayerControllerComponent` system).
* **Prohibited Context**:
  * Performance-critical per-frame updates (e.g., transmitting camera matrix coordinates every frame should use ECS Components, not Event Bus).

### 3. ECS Data (Simulation State)
* **Description**: Sharing data via contiguous component pools. Systems read and write data directly to entity component references (`TransformComponent`, `MeshRendererComponent`).
* **Allowed Context**:
  * Parallelizable gameplay and physics simulations (e.g. `PhysicsSystem` updating transforms which are subsequently read by `RenderSystem`).
* **Prohibited Context**:
  * Systems using component data structures to trigger direct rendering API commands. Systems only process data; the Renderer coordinates actual draws.

### 4. Command Queues & Buffers (Deferred Actions)
* **Description**: Thread-safe structures that buffer work to be executed sequentially at a deterministic sync point (e.g. `EntityCommandBuffer`, GPU command lists).
* **Allowed Context**:
  * Structural ECS modifications (creation/destruction) during system iteration loops.
  * Multi-threaded rendering command record submissions.
* **Prohibited Context**:
  * Instantaneous queries that expect a return value.

---

## 🛑 Rule Enforcement Checklist

1. **Include Boundary**: Subsystem `A` must never include subsystem `B`'s internal `Private/` headers.
2. **Interface Isolation**: Subsystem `A` should only depend on subsystem `B`'s public interface (`IB`) or shared types (`BTypes`).
3. **No Direct Singleton Chains**: Replace static global singletons (`Renderer::Get()->GetSwapChain()`) with dependency-injected interfaces (`ctx.renderer`).
