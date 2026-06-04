# Omnix Studio Engine v0.2 — Detailed 36-Week Development Roadmap
**Codename:** Runtime Genesis
**Philosophy:** Architectural Stability Before Expansion
**Core Rule:** No feature begins until its dependency gate is 100% validated and signed off.

---

## Document Structure

Each week entry contains:
- **Context** — why this week exists and what problem it solves
- **Primary Objective** — the single overriding goal
- **Architectural Rationale** — the engineering reason this work is sequenced here
- **Linear Tasks** — 5 granular, actionable technical tasks in execution order
- **Technical Notes** — implementation guidance, pitfalls, and constraints
- **Dependency Gate** — what must be 100% true before the next week begins
- **Validation Check** — a specific, executable test to prove the gate is met
- **Expected Deliverable** — the concrete artifact produced this week

---

# PHASE 1 — ENGINE ARCHITECTURE CLEANUP
## Months 1–2 | Weeks 1–8

> The single most important phase of v0.2. Without architectural stability, every subsequent system builds on sand. This phase is not about adding features — it is about understanding, mapping, and hardening what already exists.

---

## MONTH 1 — Subsystem Mapping & Architecture Audit

### Week 1 — Subsystem Inventory

**Context:**
Omnix v0.1 grew organically. Systems were added as needed, ownership was assumed rather than declared, and initialization happened wherever it was convenient. Before building anything new, the full truth of what exists must be documented. This is the foundation that every subsequent week depends on.

**Primary Objective:**
Produce a complete, honest, zero-ambiguity inventory of every subsystem in the engine.

**Architectural Rationale:**
You cannot fix a dependency problem you cannot see. The subsystem inventory is not documentation for its own sake — it is the prerequisite for the dependency graph (Week 2), the global state audit (Week 3), and ultimately the design of EngineRuntime (Week 5). Skipping or rushing this week guarantees discovering hidden coupling at the worst possible time — mid-refactor.

**Linear Tasks:**

1. **Enumerate all subsystems.** Walk every source directory and header file. Produce a flat list of all distinct engine subsystems. The minimum expected list: Renderer, ECS World, Scheduler/Job System, Event Bus, Input System, Asset Loader, Serialization, Scene System, Window Layer, Vulkan Backend, Memory Utilities, Logging. Mark any discovered beyond these as additional entries.

2. **Document each subsystem's ownership.** For every subsystem, write: the name of the class or module that owns it, which source files constitute its implementation, and who (which team member or architectural layer) is considered the responsible owner. If ownership is unclear, mark it explicitly as `OWNERSHIP_UNCLEAR`.

3. **Record initialization and shutdown points.** For every subsystem, find the exact line(s) where it is initialized (constructed, opened, or started) and the line(s) where it is shut down. If a subsystem initializes itself via a static constructor or global object, mark it as `STATIC_INIT` — this is a red flag.

4. **Classify stability.** Apply a three-tier classification to every subsystem: `STABLE` (well-understood, no known bugs, clean API), `FRAGILE` (works but has known issues, fragile assumptions, or unclear ownership), `UNKNOWN` (not well understood — needs investigation before touching).

5. **Produce `SUBSYSTEMS.md`.** Write a structured markdown table with columns: Subsystem Name | Owner Class | Source Files | Init Location | Shutdown Location | Stability | Notes. Commit this file to the repo root. It becomes the living reference document for all of Phase 1.

**Technical Notes:**
- Do not attempt to fix anything discovered this week. The goal is observation, not intervention.
- If you find a subsystem that has no identifiable owner, that is important data — record it as `OWNERSHIP_UNCLEAR`, not as something to skip.
- Pay special attention to template headers and utility libraries — these often contain hidden initialization or global state.
- The `UNKNOWN` classification should be treated as a warning: any system marked `UNKNOWN` that is modified later without first being understood is a source of future instability.

**Dependency Gate:**
Every subsystem in the codebase has an entry in `SUBSYSTEMS.md`. Zero subsystems are missing, unnamed, or unclassified.

**Validation Check:**
Do a `grep -r "class " src/ --include="*.h"` pass. Every class that owns significant state or behavior must have a corresponding entry in `SUBSYSTEMS.md`. Review the output with a second pair of eyes if possible.

**Expected Deliverable:** `SUBSYSTEMS.md` committed to repo root.

---

### Week 2 — Dependency Graph Construction

**Context:**
Having the list of subsystems is not enough. The critical question is: what depends on what? Hidden dependencies are the root cause of most engine instability — a system that "accidentally" depends on another system being initialized first will crash intermittently in ways that are nearly impossible to debug. This week maps the entire web.

**Primary Objective:**
Produce a complete visual and documented map of every inter-subsystem dependency.

**Architectural Rationale:**
The dependency graph is the architectural X-ray of the engine. It reveals circular dependencies (which make refactoring impossible), identifies which subsystems are true leaf nodes (can be tested in isolation), and exposes which systems are load-bearing — meaning any change to them risks cascading failures across the engine.

**Linear Tasks:**

1. **Build a dependency matrix.** For every subsystem in `SUBSYSTEMS.md`, fill in a table with the following columns: `Depends On` (systems it requires to function), `Used By` (systems that call into it), `Owns` (resources or objects it is solely responsible for), `Lifetime` (does it live for the full runtime, or is it transient?), `Thread Safety` (single-threaded only, or safe for concurrent access?).

2. **Produce a directed graph diagram.** Using Mermaid, Graphviz, or a hand-drawn diagram, create a visual representation of the dependency graph where arrows point from "depends on" to "dependency". Each node is a subsystem. This diagram goes into `ARCHITECTURE.md`.

3. **Identify and document all circular dependencies.** A circular dependency exists when subsystem A depends on B which depends on C which depends on A (in any length chain). Walk every cycle and document it explicitly: `Renderer → SceneSystem → ECS → Renderer` is an example of a dangerous three-way cycle. List every discovered cycle in `SUBSYSTEMS.md` under a `CIRCULAR_DEPS` section.

4. **Identify cross-layer violations.** In a clean engine, dependencies flow in one direction (high-level systems depend on low-level systems, never the reverse). Find any case where a low-level system (e.g., Memory Utilities, Logging) depends on a high-level system (e.g., Scene, Gameplay). These are architectural violations that must be resolved.

5. **Document the target dependency flow.** Based on the audit, write down the intended one-directional dependency order as a chain. Example: `Platform Layer → Logging → Memory → Window → Vulkan Backend → Renderer → Asset Loader → ECS → Scene → Scheduler → Gameplay`. This becomes the reference for the initialization order in Week 7.

**Technical Notes:**
- Dependency can be both compile-time (via `#include`) and runtime (via pointer/reference). Document both kinds separately.
- A circular `#include` cycle is often a symptom of a circular ownership cycle — investigate both when you find either.
- The graph does not need to be perfect on the first pass. It should be accurate enough to identify the most dangerous cycles and the grossest architectural violations.
- Keep the diagram reasonably readable — if there are too many edges, split into a high-level overview diagram and per-cluster detail diagrams.

**Dependency Gate:**
Full dependency graph is visualized and committed. Every circular dependency is explicitly listed in `SUBSYSTEMS.md`. The target one-directional flow is documented.

**Validation Check:**
Walk every edge in the diagram manually and verify it against actual `#include` directives and function call sites in the source. Confirm no undocumented edges exist.

**Expected Deliverable:** Dependency graph diagram and updated `SUBSYSTEMS.md` with circular dependency section.

---

### Week 3 — Global State Audit

**Context:**
Global mutable state is the second most common source of engine instability after circular dependencies. Global state creates invisible coupling — any code anywhere can read or modify the state, making the order of operations unpredictable and testing nearly impossible. This week catalogs every piece of global state in the engine.

**Primary Objective:**
Identify and document every piece of global mutable state in the engine, with a disposition decision for each.

**Architectural Rationale:**
Before `EngineRuntime` can own the engine's lifecycle (Week 5), the existing ownership must be understood. Global state represents "ownership by nobody" — and ownership by nobody means responsibility by nobody. Every piece of global state that moves into `EngineRuntime` or gets replaced with dependency injection is a direct improvement to the engine's stability and testability.

**Linear Tasks:**

1. **Search for static globals.** Run: `grep -rn "^static " src/ --include="*.cpp" --include="*.h"` and `grep -rn "^[A-Z].*=[^=]" src/ --include="*.cpp"` to find file-scope static variables and global objects. Log every hit.

2. **Search for singletons.** Find any class that implements the Singleton pattern (private constructor, static `GetInstance()` or `Get()` method, or static member instance). Document each: class name, what it owns, who calls it, and when it is first accessed.

3. **Search for extern variables.** Run: `grep -rn "extern " src/ --include="*.h" --include="*.cpp"`. Every `extern` variable is a global by another name. Document all of them.

4. **Classify every discovered global.** For each global state item, assign one of three dispositions: `KEEP_GLOBAL` (it genuinely needs to be global — e.g., a platform-level singleton like the Vulkan instance), `MOVE_TO_RUNTIME` (it should be owned by `EngineRuntime` and passed via `RuntimeContext`), `REPLACE_WITH_INJECTION` (it should be eliminated as a global entirely, and the dependency should be explicitly injected at the call site).

5. **Produce `GLOBAL_STATE_AUDIT.md`.** One entry per global: name, file, current type (static/singleton/extern), current justification, proposed disposition, and migration notes. This document is input to Week 6 (`RuntimeContext` design).

**Technical Notes:**
- Thread-local storage (`thread_local`) is not global state in the traditional sense but should still be documented for completeness.
- Do not confuse `const` globals with mutable globals — `static const char* VERSION = "0.2"` is harmless. Focus on mutable state only.
- Pay particular attention to globals that are initialized by constructors (SIOF — static initialization order fiasco). These are the most dangerous because their initialization order is undefined across translation units.
- Logging and assertion utilities often have legitimate reasons to be global — distinguish between "this is fine" and "this needs fixing" in the classification.

**Dependency Gate:**
Zero undocumented mutable globals remain. Every global has a classification and disposition in `GLOBAL_STATE_AUDIT.md`.

**Validation Check:**
`grep -rn "static " src/` and `grep -rn "extern " src/` output — every hit is accounted for in the audit document. No hits should be "unexplained."

**Expected Deliverable:** `GLOBAL_STATE_AUDIT.md` committed to repo.

---

### Week 4 — Circular Dependency Elimination

**Context:**
Armed with the complete dependency graph (Week 2), this week makes the actual code changes to break every circular dependency. This is the last "fix the existing code" week before building `EngineRuntime`. If circular dependencies remain when `EngineRuntime` is built, they will be baked in and become exponentially harder to remove.

**Primary Objective:**
Break every identified circular dependency, leaving the dependency graph strictly one-directional.

**Architectural Rationale:**
Circular dependencies prevent deterministic initialization ordering (you can't initialize A before B if B depends on A). They also cause compile-time errors in large refactors because touching one header triggers cascading recompilation across the cycle. Breaking them now, before adding any new systems, is vastly cheaper than untangling them post-expansion.

**Linear Tasks:**

1. **Design break strategies for each cycle.** For every circular dependency identified in Week 2, choose the appropriate elimination strategy: (a) **Interface extraction** — introduce `IRenderer`, `IScheduler`, etc. as pure-virtual abstract headers that break the concrete dependency; (b) **Forward declaration** — replace a full `#include` with a forward declaration where only a pointer or reference is needed; (c) **Ownership transfer** — if A and B are circular because B wrongly owns something that belongs to A, move that ownership; (d) **Event inversion** — if B calls into A, invert this to A subscribing to events from B, removing the compile-time dependency.

2. **Implement abstract interface headers.** For the most depended-upon subsystems, create interface headers (e.g., `IRenderer.h`, `IScheduler.h`, `IAssetLoader.h`) containing only pure-virtual method declarations. Concrete systems include the interface header, not the implementation header. Callers depend on the interface, not the concrete type.

3. **Replace `#include` chains with forward declarations.** Walk through every identified circular include chain. Replace concrete type includes with forward declarations (`class Renderer;`, `struct Entity;`) wherever a full type definition is not required (i.e., only a pointer or reference is used, not a member or value type).

4. **Validate each break individually.** After breaking each cycle, run a full incremental build. Verify the specific cycle is gone. Do not batch-break all cycles at once — this makes debugging failures impossible.

5. **Re-draw the dependency graph.** After all cycles are broken, produce an updated version of the dependency graph showing the clean, one-directional flow. Commit both the old and new diagrams to `ARCHITECTURE.md` so the improvement is visible and documented.

**Technical Notes:**
- Introducing interfaces is the most powerful technique but also the most invasive. Apply it selectively — not every subsystem needs an interface. Reserve it for systems that are widely depended upon.
- Forward declarations are cheap and effective for pointer-based dependencies. They do not work for value-type members — in those cases, ownership transfer is usually the right answer.
- Watch for "phantom cycles" — cycles that exist at the `#include` level but not at the logical ownership level. These can sometimes be broken with a simple `#pragma once` or header reorganization without any ownership changes.
- After all cycles are broken, the include graph should be a DAG (directed acyclic graph). There are tools (include-what-you-use, cinclude2dot) that can visualize this automatically.

**Dependency Gate:**
Zero circular include cycles remain. The dependency graph, when redrawn, is a strict DAG. Full clean build succeeds with `-Wall -Wextra`.

**Validation Check:**
Run a full clean build from scratch (`cmake --build . --clean-first`). Zero "circular include" or "incomplete type" errors. Optionally run `include-what-you-use` on the codebase to confirm.

**Expected Deliverable:** Updated dependency graph. All interface headers committed. Clean build confirmed.

---

## MONTH 2 — EngineRuntime Core & Dependency Boundaries

### Week 5 — EngineRuntime Class Foundation

**Context:**
All the audit and cleanup work of Month 1 was preparation for this moment. `EngineRuntime` is the central architectural bet of v0.2 — it is the single owner of the entire engine lifecycle. Instead of scattered `Initialize()` calls in `main()`, a static object being constructed here, a singleton being accessed there, everything flows through one authoritative class. This week creates the skeleton.

**Primary Objective:**
Implement the `EngineRuntime` class as the sole authority over engine lifecycle — initialization, main loop, and shutdown.

**Architectural Rationale:**
Centralized lifecycle management solves three problems simultaneously: (1) initialization order becomes explicit and deterministic; (2) shutdown order becomes the reverse of initialization, preventing use-after-free; (3) ownership becomes unambiguous — if it's in `EngineRuntime`, `EngineRuntime` owns it. This is the structural backbone that all later subsystem integration attaches to.

**Linear Tasks:**

1. **Create `EngineRuntime` class skeleton.** Create `EngineRuntime.h` and `EngineRuntime.cpp`. The public interface is exactly three methods: `bool Initialize()`, `void Run()`, `void Shutdown()`. The class is non-copyable and non-movable. Add a private `bool m_Running` flag. No other public methods at this stage.

2. **Move subsystem instances into `EngineRuntime`.** The following subsystems become private owned members of `EngineRuntime`: Renderer, ECSWorld, AssetManager, SceneManager, EventBus, Scheduler, WindowSystem. These are value members (not pointers) where possible, or `std::unique_ptr` where forward declarations are required. Remove all global and static instances of these subsystems.

3. **Implement stub `Initialize()`.** Call each subsystem's initialization in the documented order from Week 2: Logging → Memory → WindowSystem → Renderer (Vulkan) → AssetManager → SceneManager → ECSWorld → Scheduler → Gameplay. Each call is wrapped in a failure check: if any subsystem fails to initialize, call `Shutdown()` on all already-initialized subsystems in reverse and return `false`. Log the failure subsystem name.

4. **Implement stub `Shutdown()`.** Tear down subsystems in exact reverse order of initialization. This is not an optional nicety — GPU resources, file handles, and network connections require ordered shutdown. Use a `std::vector<std::function<void()>>` shutdown stack: each successful `Initialize()` pushes a corresponding shutdown lambda onto the stack. `Shutdown()` pops and calls them in reverse.

5. **Wire `main()` to use only `EngineRuntime`.** `main()` becomes three lines: `EngineRuntime runtime; if (!runtime.Initialize()) return 1; runtime.Run(); runtime.Shutdown(); return 0;`. Remove every other subsystem interaction from `main()`. Commit the change.

**Technical Notes:**
- The shutdown stack pattern is particularly valuable because it guarantees partial-initialization cleanup: if `Initialize()` fails halfway through (e.g., Renderer initializes but AssetManager fails), only the systems that successfully initialized get shut down — and in correct order.
- Do not implement the full runtime loop in `Run()` yet. A stub `while(m_Running) { /* todo */ }` is sufficient for this week.
- Resist the temptation to add convenience accessors like `static EngineRuntime* Get()` — this recreates the singleton anti-pattern. Systems that need other systems will get them via `RuntimeContext` (Week 6).
- Keep the `EngineRuntime` header lean. It should include only the forward declarations needed to declare its members, not the full headers of all its owned subsystems.

**Dependency Gate:**
`EngineRuntime` is the sole entry point. `main()` contains exactly the three calls. No subsystem initializes itself from a static constructor or global object.

**Validation Check:**
Remove all direct subsystem calls from `main()`. Build and run. Engine boots, enters the main loop stub, and shuts down cleanly. Run under Address Sanitizer — zero detected initialization-order issues or use-after-free on shutdown.

**Expected Deliverable:** `EngineRuntime.h`, `EngineRuntime.cpp`, and updated `main.cpp` committed.

---

### Week 6 — RuntimeContext & Dependency Injection

**Context:**
`EngineRuntime` now owns all the subsystems, but subsystems still need to talk to each other. The naive solution — static accessors or singletons — recreates the global state problem in a different form. The correct solution is explicit dependency injection via a `RuntimeContext` struct: a lightweight container of non-owning pointers that is passed to subsystems during initialization.

**Primary Objective:**
Define `RuntimeContext` and refactor all subsystems to receive explicit dependencies rather than accessing globals or singletons.

**Architectural Rationale:**
`RuntimeContext` makes dependencies visible at the call site. Instead of `Renderer::Get()` (invisible, global), a subsystem's `Initialize(const RuntimeContext& ctx)` signature explicitly declares what it depends on. This makes the system easier to test (you can construct a partial context for unit tests), easier to reason about (all dependencies are declared), and eliminates the last remaining need for global state in most subsystems.

**Linear Tasks:**

1. **Design `RuntimeContext`.** Create `RuntimeContext.h` containing `struct RuntimeContext { Renderer* renderer; AssetManager* assets; SceneManager* scenes; ECSWorld* ecs; EventBus* events; Scheduler* scheduler; WindowSystem* window; }`. All fields are raw non-owning pointers. `RuntimeContext` does not own anything — it is a view into `EngineRuntime`'s owned members. Add a constructor that takes all fields explicitly, and an `IsValid()` method that asserts all pointers are non-null.

2. **Refactor subsystem `Initialize()` signatures.** For every subsystem that depends on other subsystems, change its `Initialize()` method to accept `const RuntimeContext&` as a parameter. Inside the method, store only the specific pointers it needs (e.g., a `Renderer` only needs `ctx.window` and `ctx.assets`, not all of `RuntimeContext`). Do not store the full `RuntimeContext` — store only what is used.

3. **Construct and pass `RuntimeContext` from `EngineRuntime`.** In `EngineRuntime::Initialize()`, after all subsystems are constructed, build a `RuntimeContext` pointing to the owned members and pass it to each subsystem's `Initialize()`. The context is built incrementally — it must only contain subsystems already initialized at the point of each call.

4. **Remove all singleton accessors.** For every singleton `GetInstance()` / `Get()` method discovered in the audit, remove the method. If code breaks because it was calling the singleton, fix the calling code to use the injected `RuntimeContext` pointer instead. This may require threading `RuntimeContext` references deeper into some subsystems.

5. **Verify no subsystem stores `RuntimeContext` by value or beyond its `Initialize()` lifetime.** `RuntimeContext` is a temporary construction-time convenience. After `Initialize()`, each subsystem should hold only specific pointers, not the full context. Enforce this by making `RuntimeContext` move-only with a deleted copy constructor, so accidental storage is a compile error.

**Technical Notes:**
- Some subsystems will need to store pointers to their dependencies as member variables (e.g., `Renderer` stores `m_AssetManager` as a pointer). This is fine — it makes the dependency explicit. The key rule is that these pointers are set during `Initialize()` and never change afterward.
- `RuntimeContext` is not a service locator. It does not support dynamic registration or lookup by type. It is a plain struct with named fields. If you find yourself adding a `template<T> T* Get()` method to it, stop — that is a service locator, which is a worse version of a global singleton.
- If a subsystem only needs one other subsystem (e.g., `SceneSerializer` only needs `AssetManager`), it is acceptable to pass that dependency directly rather than the full `RuntimeContext`. The goal is explicit dependencies, not mandatory use of the context struct.

**Dependency Gate:**
Zero subsystems access engine state through global variables, static singletons, or `extern` declarations. All cross-subsystem access flows through explicitly stored pointers set during `Initialize()`.

**Validation Check:**
Delete all singleton `GetInstance()` method bodies (leave declarations to catch callers, then remove those too). Build must succeed. Run the engine — zero null-pointer crashes, zero assertion failures.

**Expected Deliverable:** `RuntimeContext.h` committed. All subsystem `Initialize()` signatures updated. All singleton accessors removed.

---

### Week 7 — Deterministic Startup & Shutdown Validation

**Context:**
The initialization sequence exists on paper (Week 5) but has not been battle-tested. Real engines fail in initialization — a GPU driver might not support the required Vulkan features, a required asset directory might not exist, memory allocation might fail. This week proves that the initialization sequence is robust under all failure conditions.

**Primary Objective:**
Prove through deliberate failure injection that the initialization and teardown sequence is fully deterministic and safe under all failure scenarios.

**Architectural Rationale:**
A startup sequence that only works when everything succeeds is not a startup sequence — it is a wish. Production engines run on machines with missing drivers, corrupted asset caches, and restricted permissions. The engine must handle partial initialization failures gracefully, without leaking GPU resources, file handles, or memory. The shutdown stack introduced in Week 5 is the mechanism; this week is the proof.

**Linear Tasks:**

1. **Instrument every subsystem's `Initialize()` and `Shutdown()`.** Add a structured log call at the first and last line of each method: `LOG_INFO("[EngineRuntime] Renderer::Initialize — BEGIN")` and `LOG_INFO("[EngineRuntime] Renderer::Initialize — SUCCESS")` (or `FAILED`). The log output must make the boot and shutdown sequence immediately readable in the console.

2. **Add initialization guards.** Each subsystem must assert it is not initialized twice. Add a `bool m_Initialized = false;` guard to every subsystem. `Initialize()` asserts `!m_Initialized` at the top and sets it to `true` on success. `Shutdown()` asserts `m_Initialized` and sets it to `false`. This prevents the engine from accidentally double-initializing a subsystem.

3. **Inject deliberate failures at each init stage.** Add a debug-only `EngineRuntime::SetFailAt(SubsystemID)` method. For each subsystem from 1 to N, call `SetFailAt(i)`, run the engine, and verify: (a) shutdown is called correctly on all subsystems initialized before the failure; (b) the engine returns a clean failure code; (c) no GPU resource, file handle, or memory is leaked. Use Address Sanitizer and Vulkan validation layers during this test.

4. **Enable Vulkan validation layers unconditionally in debug builds.** Add a `#if defined(DEBUG)` block to Vulkan initialization that enables `VK_LAYER_KHRONOS_validation`. Add a startup log line: `LOG_INFO("[Renderer] Vulkan validation layers: ACTIVE")`. The engine must boot with zero Vulkan validation errors or warnings on a clean first run.

5. **Write `RUNTIME_LIFECYCLE.md`.** Document the final, confirmed startup and shutdown sequences as ordered lists. Include: the subsystem name, what it initializes (in plain English), its dependencies, and what it cleans up on shutdown. This becomes the reference document for all future subsystem additions.

**Technical Notes:**
- Failure injection testing is not optional. The most common cause of engine crashes in development is a partial initialization that the developer "knows will work" until the day it doesn't.
- Address Sanitizer (`-fsanitize=address`) and Vulkan validation layers are development tools, not production overhead. Enable both unconditionally in debug builds, and only for targeted performance profiling in release builds.
- The Vulkan validation layer is particularly important at this stage because it will flag resource leaks that are invisible at the C++ level — a `VkBuffer` that is never freed shows up in Vulkan validation output even if C++ reports no memory leak.
- `RUNTIME_LIFECYCLE.md` should be treated as a living document — update it every time a new subsystem is added to `EngineRuntime`.

**Dependency Gate:**
Engine survives deliberate failure injection at every initialization stage. Each failure path produces zero leaks (Address Sanitizer), zero GPU resource leaks (Vulkan validation), and a clean shutdown sequence in the logs.

**Validation Check:**
Run the engine with `SetFailAt(i)` for every value of `i` from 1 to the number of subsystems. Every run must produce a clean shutdown log showing only the subsystems that were successfully initialized being torn down in reverse order.

**Expected Deliverable:** `RUNTIME_LIFECYCLE.md` committed. Instrumented log output from a clean boot captured and archived.

---

### Week 8 — Clean Dependency Boundaries & Communication Standards

**Context:**
The engine architecture is now clean — no circular dependencies, all subsystems owned by `EngineRuntime`, dependencies injected explicitly. The final risk is architectural drift: as development continues, engineers (or future-you) will naturally reach for the most convenient way to have two systems talk to each other, which is often the wrong way. This week establishes explicit standards that prevent the architecture from degrading.

**Primary Objective:**
Enforce subsystem interface separation and standardize all cross-subsystem communication patterns so architectural rules can be mechanically enforced.

**Architectural Rationale:**
Architecture that relies on discipline alone degrades. Architecture with mechanical enforcement — standards that produce compile errors when violated — persists. Separating public from private APIs, and defining exactly four communication patterns, gives every engineer a clear decision tree for adding new cross-subsystem interactions.

**Linear Tasks:**

1. **Enforce public API separation.** For every subsystem, verify that its header files are organized into exactly two categories: (a) the public API header (e.g., `Renderer.h`) containing only the interface that other systems may call, and (b) internal implementation headers (e.g., `VulkanSwapchain.h`, `DescriptorSetCache.h`) that are never included outside the subsystem's own source files. Verify this by attempting to include an internal header from a different subsystem's source file — the build should fail (enforce via include path configuration).

2. **Document the four communication standards.** Write a `COMMUNICATION_STANDARDS.md` document defining exactly four approved cross-subsystem communication patterns: (a) **Direct Calls** — used for tight ownership relationships (e.g., `EngineRuntime` calling `Renderer::Initialize()`); (b) **Event Bus** — used for decoupled, one-to-many notifications (e.g., "asset reloaded", "window resized"); (c) **ECS Data** — used for simulation data that multiple systems read (transform, physics, render component data); (d) **Command Buffers** — used for deferred, batched operations that should not execute immediately (ECS structural changes, GPU command recording).

3. **Audit every existing cross-subsystem call.** For every place in the codebase where subsystem A calls into subsystem B, classify the call as one of the four patterns. If it does not fit any pattern, either it is wrong and should be refactored, or a new pattern needs to be justified and added to the standards.

4. **Replace off-pattern calls.** Fix any cross-subsystem interactions that do not conform to the standards. The most common violation at this stage is a system making a direct call where an Event Bus message would be more appropriate — for example, the Input System directly calling methods on the Gameplay System instead of emitting input events.

5. **Update `SUBSYSTEMS.md`.** Add a final column: `Public API Surface`. For each subsystem, list the methods that are part of the public API — these are the only methods that other subsystems are permitted to call. This is a living constraint document, not just documentation.

**Technical Notes:**
- The four-pattern standard does not need to cover every edge case. Its value is in providing a clear "what is this?" question for any new cross-subsystem interaction. If an engineer cannot answer which of the four patterns a new interaction is, that is a signal to discuss the architecture before implementing it.
- Public/private API separation at the file level (which directories are on the include path) is more enforceable than at the class level (public/private member access). Use CMake `target_include_directories(PRIVATE ...)` to prevent internal headers from being reachable outside the subsystem.
- Event Bus and Command Buffers are both forms of deferred execution, but they serve different purposes: Event Bus is for cross-subsystem notifications (many possible listeners), Command Buffers are for same-subsystem batching (a single consumer applies the commands).

**Dependency Gate:**
Every cross-subsystem communication follows one of the four documented patterns. No subsystem includes another's internal implementation headers. `COMMUNICATION_STANDARDS.md` is committed.

**Validation Check:**
Attempt to add a new cross-subsystem call that violates the standards (e.g., include `VulkanSwapchain.h` from `SceneSystem.cpp`). The build must fail. If it does not fail, the include path enforcement is not working and must be fixed.

**Expected Deliverable:** `COMMUNICATION_STANDARDS.md`. Updated `SUBSYSTEMS.md`. Enforced include path configuration in CMake.

---

# PHASE 4 — ASSET PIPELINE
## Months 3–4 | Weeks 9–16

> The asset pipeline is built before the renderer stabilization because the material system and GPU resource lifetime work (Phase 3) both depend on the ability to load assets by handle. Building the renderer first, then the asset system, would require rewriting renderer resource loading when the asset system arrives.

---

## MONTH 3 — Asset Handle System & Registry

### Week 9 — Asset Handle Type Design

**Context:**
The single most important architectural decision in the asset system is how assets are referenced at runtime. The wrong choice — raw string file paths — creates a cascade of problems: path dependencies baked into compiled code, no refactoring safety (rename a file and silently break all references), O(N) string comparisons for lookups, and no compile-time type safety. The correct choice is a typed, opaque handle backed by a UUID.

**Primary Objective:**
Define the `AssetHandle` type and establish it as the exclusive runtime asset reference mechanism throughout the engine.

**Architectural Rationale:**
`AssetHandle` is a `uint64_t` wrapper. This gives O(1) hash-map lookups (integer keys), type safety (you can't accidentally pass a mesh handle where a texture is expected with `AssetHandle<MeshAsset>`), and complete decoupling of runtime code from file system paths. All path-to-handle resolution happens at import time — the runtime never needs to know a file path exists.

**Linear Tasks:**

1. **Define the base `AssetHandle` type.** Create `AssetHandle.h` with `using AssetHandle = uint64_t;`. Add `constexpr AssetHandle kInvalidHandle = 0;`. Add `inline bool IsValidHandle(AssetHandle h) { return h != kInvalidHandle; }`. Add a typed wrapper template: `template<typename T> struct TypedAssetHandle { AssetHandle id; bool IsValid() const { return IsValidHandle(id); } };` — concrete handles are `TypedAssetHandle<MeshAsset>`, `TypedAssetHandle<TextureAsset>`, etc.

2. **Implement UUID generation.** Implement `GenerateAssetUUID(const std::filesystem::path& sourcePath, AssetType type) → AssetHandle`. Use a deterministic hash (FNV-1a or xxHash) of the canonical source path combined with the asset type enum value. The same source file always produces the same handle — this is critical for the registry to be stable across engine restarts.

3. **Write unit tests for the handle type.** Test: (a) two calls to `GenerateAssetUUID` with identical inputs produce identical handles; (b) two different source paths produce different handles; (c) `IsValidHandle(kInvalidHandle)` returns false; (d) `IsValidHandle(validHandle)` returns true; (e) handles for different asset types of the same source file produce different handles.

4. **Enforce the no-raw-paths rule.** Document in `CODING_STANDARDS.md` (create if it does not exist): "Raw `std::string` or `std::filesystem::path` values must never appear as parameters to runtime asset access functions. All runtime asset references use `AssetHandle`." Optionally enforce with a `[[deprecated]]` wrapper around any existing path-based lookup functions.

5. **Update `COMMUNICATION_STANDARDS.md`.** Add a section: "Asset References — All cross-subsystem asset references must use `TypedAssetHandle<T>`. The `AssetManager` is the sole authority for resolving handles to live runtime objects."

**Technical Notes:**
- Using a deterministic hash for UUID generation (rather than a true random UUID) is a deliberate trade-off. True random UUIDs are more collision-resistant but break on engine restart (the same file gets a different handle every time). Deterministic hashes are stable across restarts, which is required for the serialization system to work correctly.
- The collision probability of a 64-bit deterministic hash over a typical game asset set (< 100,000 assets) is astronomically low, but if it is a concern, a 128-bit hash (`__uint128_t` or a struct of two `uint64_t` values) can be used with minimal overhead.
- The typed wrapper (`TypedAssetHandle<T>`) is optional but recommended — it catches errors like passing a mesh handle to a function that expects a texture handle at compile time, which raw `uint64_t` would not.

**Dependency Gate:**
`AssetHandle` type is committed, unit tests pass, and the no-raw-paths rule is documented and enforced at code review level.

**Validation Check:**
Attempt to compile a function that accepts `std::string` and is called from runtime code — if the `[[deprecated]]` wrapper is in place, this produces a compiler warning or error. All unit tests pass with zero failures.

**Expected Deliverable:** `AssetHandle.h` committed. Unit tests committed and passing. `CODING_STANDARDS.md` updated.

---

### Week 10 — Asset Registry Database

**Context:**
The `AssetRegistry` is the engine's central ledger of all known assets. It answers the question: "For this handle, what are the source file, type, dependencies, and import state?" It is the single source of truth for asset metadata and is persisted to disk so it survives engine restarts without re-scanning all source files.

**Primary Objective:**
Implement a persistent, reliable `AssetRegistry` that tracks every known asset with full metadata.

**Architectural Rationale:**
The registry must exist before importers (Weeks 13–14) because importers write to the registry. It must exist before runtime loading (Week 15) because the runtime reads from the registry. It must be owned by `EngineRuntime` and initialized before the Renderer because the Renderer will eventually load assets. Getting this order right now prevents painful restructuring later.

**Linear Tasks:**

1. **Define `AssetMetadata`.** Create `AssetMetadata.h` with `struct AssetMetadata { AssetHandle handle; AssetType type; std::filesystem::path sourcePath; std::filesystem::path importedPath; uint64_t importTimestamp; std::vector<AssetHandle> dependencies; bool isDirty; bool isImported; }`. This is the full per-asset record.

2. **Implement `AssetRegistry` class.** Core methods: `Register(sourcePath, type) → AssetHandle` (generates UUID, creates metadata, returns handle), `Lookup(handle) → const AssetMetadata*` (O(1) hash-map lookup, returns nullptr if not found), `Contains(handle) → bool`, `MarkDirty(handle)`, `GetAllDirty() → std::vector<AssetHandle>`. Internal storage: `std::unordered_map<AssetHandle, AssetMetadata>`.

3. **Implement registry serialization.** On engine shutdown, serialize the full registry to `asset_registry.omnixdb` (use a simple binary format or JSON — correctness over performance at this stage). On startup, if the file exists, deserialize it. After deserialization, validate: for every entry, check if the source file still exists at the recorded path. If not, mark the entry as `isDirty = true` (source has changed or been deleted).

4. **Implement staleness detection.** After deserialization, for every entry where `isImported = true`, compare the current file modification timestamp of `sourcePath` against `importTimestamp`. If the file is newer than the last import, set `isDirty = true`. This is the mechanism that drives hot reloading (Week 16) and ensures assets are always up to date.

5. **Register `AssetRegistry` in `EngineRuntime`.** Add `AssetRegistry m_AssetRegistry;` as a member of `EngineRuntime`. Initialize it immediately after Logging and Memory, before any other subsystem. Add it to `RuntimeContext` as `AssetRegistry* assetRegistry`.

**Technical Notes:**
- The binary format for `asset_registry.omnixdb` should include a magic number and a version field so that format changes can be detected and the registry can be rebuilt from scratch gracefully (rather than crashing on a format mismatch).
- The registry should never throw on a missing handle lookup — return `nullptr` and let the caller decide what to do. Throwing on a lookup is appropriate only if the handle was previously validated as present and somehow disappeared, which would indicate a programming error.
- Consider whether the registry should support asset deletion. If a source file is deleted, the handle remains in the registry (marked as broken). The runtime can check `IsImported()` before accessing the asset and log a descriptive error. This is more useful for debugging than silently removing the handle.

**Dependency Gate:**
Registry saves and loads correctly — round-trip test passes. Staleness detection correctly identifies modified files. Registry is initialized in `EngineRuntime` before the Renderer.

**Validation Check:**
Register 100 mock assets, save the registry, modify the timestamps of 10 source files, restart engine, load registry — the 10 modified files are marked `isDirty`, the other 90 are not.

**Expected Deliverable:** `AssetRegistry.h`, `AssetRegistry.cpp`, and `AssetMetadata.h` committed. Registry integrated into `EngineRuntime`.

---

### Week 11 — Runtime Asset Lookup & Loader Interface

**Context:**
The registry knows about assets (metadata). The loader knows how to produce them (runtime objects). The `AssetManager` is the bridge: given a handle, it uses the registry to find the source data, invokes the appropriate loader, and returns a live runtime object. This week defines the loader contract and the fast lookup path.

**Primary Objective:**
Implement a fast O(1) runtime asset lookup and define the `IAssetLoader` interface that all importers will implement.

**Architectural Rationale:**
The loader interface must be defined before the individual importers (Weeks 13–14) because the importers implement it. The fast lookup (`GetAsset<T>()`) must be defined before the renderer and material system use it. Defining both this week ensures that Weeks 13 onward are purely implementation work with no API design debt.

**Linear Tasks:**

1. **Implement `AssetManager::GetAsset<T>(TypedAssetHandle<T>) → T*`.** Internally, maintain a second hash-map: `std::unordered_map<AssetHandle, void*> m_LoadedAssets`. `GetAsset<T>` looks up the handle, casts the `void*` to `T*`, and returns it. If the handle is not in `m_LoadedAssets`, return `nullptr` (do not auto-load — loading is explicit). Add a debug assertion if the handle is valid but not loaded: `ASSERT_MSG(false, "Asset {} is registered but not loaded — call LoadAsset() first")`.

2. **Define `IAssetLoader` interface.** Create `IAssetLoader.h` with: `virtual bool CanLoad(AssetType type) const = 0`, `virtual bool Load(const AssetMetadata& meta, void** outAsset) = 0` (populates `*outAsset` on success), `virtual void Unload(AssetHandle handle, void* asset) = 0`, `virtual AssetType GetType() const = 0`. This interface is the contract for all importers.

3. **Implement `AssetManager::RegisterLoader(IAssetLoader*)`.** Maintain a `std::unordered_map<AssetType, IAssetLoader*> m_Loaders`. `RegisterLoader` adds or replaces the loader for a given type. `GetAsset` and `LoadAsset` use this map to dispatch to the correct loader.

4. **Add reference counting to `AssetMetadata`.** Add `uint32_t refCount = 0;` to `AssetMetadata`. `GetAsset` increments refCount on the first call per handle. `ReleaseAsset(handle)` decrements it. `UnloadAsset(handle)` calls the loader's `Unload()` only if `refCount == 0`. This prevents double-free when multiple systems hold references to the same asset.

5. **Write integration test.** Create a `MockLoader` that implements `IAssetLoader` and returns a mock asset. Register it, call `AssetManager::LoadAsset(handle)`, verify `GetAsset<MockAsset>(handle)` returns a valid non-null pointer, call `ReleaseAsset(handle)` and `UnloadAsset(handle)`, verify `GetAsset` returns null. Run under Address Sanitizer — zero leaks.

**Technical Notes:**
- `void*` in `m_LoadedAssets` is a deliberate choice over a type-erased variant or polymorphic base class. The `TypedAssetHandle<T>` template ensures type safety at the call site — the `void*` cast is only done inside `GetAsset<T>`, which is trusted code. This avoids introducing a virtual dispatch or heap allocation per asset access.
- The `IAssetLoader` interface takes `void**` as the output parameter (rather than returning a value) so that loaders can return failure without exceptions. The pattern `if (!loader->Load(meta, &asset)) { /* handle failure */ }` is clear and exception-free.
- Do not implement async loading yet. Synchronous loading (block the calling thread until the asset is ready) is correct and simple. Async loading is an optimization that can be added after the system is proven stable.

**Dependency Gate:**
`GetAsset<T>()` returns valid data for a loaded asset, null for an unloaded one, and fires an assertion for a valid handle that was never loaded. Reference counting prevents double-free.

**Validation Check:**
Deliberately access an unloaded (but registered) handle — assertion fires with the handle value and a descriptive message. No segfault. Under Address Sanitizer, zero leaks after load + unload cycle.

**Expected Deliverable:** `AssetManager.h`, `AssetManager.cpp`, `IAssetLoader.h` committed. Integration test committed and passing.

---

### Week 12 — Asset Directory Structure & Metadata Pipeline

**Context:**
The registry and loader interface are implemented, but assets still need to enter the system. The metadata pipeline is the "on-ramp" — the process by which a raw source file (a PNG, an FBX) gets discovered, assigned a handle, and queued for import. This week establishes the canonical directory layout and the automated file discovery mechanism.

**Primary Objective:**
Establish the canonical asset directory structure and implement automated asset discovery and metadata generation.

**Architectural Rationale:**
A canonical directory structure is not aesthetic — it enables the asset pipeline to be predictable and tool-friendly. If the engine knows that all meshes are in `Assets/Meshes/`, it can scan that directory for new FBX files without guessing. The `.omnixmeta` sidecar file is the critical link between the source file on disk and the runtime `AssetHandle` — it persists the UUID so that the handle is stable even if the registry file is deleted.

**Linear Tasks:**

1. **Define and enforce canonical directory structure.** Create the following directory structure under the project root: `Assets/Meshes/`, `Assets/Textures/`, `Assets/Materials/`, `Assets/Scenes/`, `Assets/Shaders/`, `Assets/Prefabs/`, `Assets/Audio/`. Add these paths to a `AssetDirectories` configuration struct in `AssetManager`. The engine should never use hardcoded strings for these paths.

2. **Implement directory scanner.** On engine startup, `AssetManager::ScanDirectories()` walks all registered asset directories, finds all files with recognized extensions (`.png`, `.jpg`, `.dds` for textures; `.fbx`, `.gltf`, `.obj` for meshes; etc.), and for each file without a `.omnixmeta` sidecar, calls `Register()` to generate a UUID and create the sidecar. Files that already have a sidecar have their handle read from the sidecar and re-registered (no UUID regeneration).

3. **Implement `.omnixmeta` sidecar generation.** When a new source file is registered, create a `.omnixmeta` file in the same directory. Format (JSON or simple key-value): `handle`, `asset_type`, `import_settings`, `version`. The `handle` value is the UUID generated by `GenerateAssetUUID()`. This sidecar is the source of truth for the handle — the registry DB is a cache of this information.

4. **Implement `AssetImportQueue`.** Add `std::vector<AssetHandle> m_ImportQueue;` to `AssetManager`. Every `Register()` call on a new (or dirty) asset pushes its handle onto the import queue. `ProcessImportQueue()` iterates the queue, looks up the appropriate `IAssetLoader`, calls `Load()`, stores the result, and clears the queue. This method is called during engine startup (after loaders are registered) and can also be triggered manually.

5. **Handle deleted source files gracefully.** If `ScanDirectories()` encounters a `.omnixmeta` file for which the source file no longer exists, mark the corresponding `AssetMetadata` as `isSourceMissing = true`. Log `[Assets] WARN: Source file missing for handle {handle}: {path}`. Do not remove the handle — it may be referenced in a saved scene. Attempting to load a `isSourceMissing` asset returns null with a descriptive error.

**Technical Notes:**
- The `.omnixmeta` file is the linchpin of handle stability. If the registry DB is deleted (e.g., for a clean rebuild), the sidecar files allow handles to be recovered without UUID regeneration. This is critical for scene files that serialize asset handles — if handles changed on a clean rebuild, every saved scene would have broken references.
- File extension matching should be case-insensitive (`.PNG` and `.png` must both match).
- The import queue should be processed in dependency order — if a material depends on a texture, the texture must be imported first. The dependency information is in `AssetMetadata.dependencies`. A simple topological sort of the import queue handles this.

**Dependency Gate:**
New asset files dropped into a canonical directory are automatically discovered, assigned stable handles, and queued for import. `.omnixmeta` sidecars are created. Deleted source files are logged without crashing.

**Validation Check:**
Drop a new PNG file into `Assets/Textures/`. Restart the engine. Verify: (1) a `.omnixmeta` file was created next to the PNG; (2) the registry contains a new entry for the PNG with a valid handle; (3) the handle matches between the sidecar and the registry. Delete the PNG. Restart. Verify: the registry entry still exists and is marked `isSourceMissing = true`.

**Expected Deliverable:** Canonical directory structure committed. `ScanDirectories()` implemented. `.omnixmeta` sidecar generation implemented. Import queue implemented.

---

## MONTH 4 — Asset Import Pipeline

### Week 13 — Texture Importer

**Context:**
The infrastructure (registry, handles, loader interface) is in place. Now the first real importer: the texture importer. Textures are the simplest asset type to import (no skeleton, no material references, no scene graph) and are required by the material system (Week 21), making them the highest-priority import target.

**Primary Objective:**
Implement a complete texture import pipeline that takes raw image source files to GPU-ready `.omnixmat` binary assets.

**Architectural Rationale:**
Textures are the most numerous asset type in any game project. An efficient, correct importer saves significant iteration time. The import step (CPU decode + compress + mip generation) is separated from the GPU upload step (Week 15) so that imported assets can be used across platforms with different GPU formats without re-running the CPU decode.

**Linear Tasks:**

1. **Integrate image decode library.** Add `stb_image` (single-header, no build system changes required) to the project. Implement `TextureImporter::DecodeSource(path) → RawImageData` that calls `stbi_load()` for PNG/JPG and `stbi_load_16()` for HDR. For DDS files (BC-compressed formats), use a dedicated DDS parser (DirectXTex if on Windows, or a minimal custom parser). Store decoded data as `RawImageData { width, height, channelCount, formatHint, std::vector<uint8_t> pixels }`.

2. **Implement mipmap chain generation.** Starting from the decoded base image, generate the full mip chain using box filter downsampling. For a 512×512 texture, generate: 256×256, 128×128, 64×64, 32×32, 16×16, 8×8, 4×4, 2×2, 1×1. Store all levels in sequence in the output binary. Mip count = floor(log2(max(width, height))) + 1.

3. **Serialize to `.omnixmat` binary.** Define the binary format: (a) 8-byte magic number `OMNIXMAT`; (b) 4-byte format version; (c) 4-byte `TextureHeader` containing: width, height, channelCount, mipCount, textureFormat enum, compressionType; (d) for each mip level: 4-byte data size, then raw pixel data. Write the result to `Assets/Textures/{assetName}.omnixmat`.

4. **Implement `TextureImporter : IAssetLoader`.** Implement `CanLoad(AssetType)` to return true for `AssetType::Texture`. Implement `Load(meta, outAsset)` to call `DecodeSource()`, build the mip chain, serialize to `.omnixmat`, and populate a `TextureAsset` struct with: `handle`, `width`, `height`, `mipCount`, `format`, `importedPath`. The GPU upload is NOT done here — that is a renderer concern handled in Week 15.

5. **Write and run import validation tests.** Import a 512×512 PNG. Verify: (a) output `.omnixmat` file exists; (b) magic number is correct; (c) `TextureHeader.width == 512`, `height == 512`; (d) `mipCount == 10` (log2(512)+1); (e) sum of all mip data sizes is consistent with the mip chain. Import a deliberately corrupted PNG (truncated file). Verify: `Load()` returns false, logs `[Assets] ERROR: Failed to decode texture: {path}`, does not write an output file.

**Technical Notes:**
- The `.omnixmat` extension is used for texture binary assets even though it looks like a material extension — this is intentional in the source documents. If this causes confusion, rename to `.omnixatex` — but match whatever the source spec says.
- Do not apply BC compression in the importer at this stage. BC compression (BC1, BC3, BC7) requires a separate library (compressonator, ispc_texcomp) and is an optimization. Import as RGBA8 first; add compression in a later pass.
- Box filter mip generation is simple but produces slightly soft results. Lanczos is sharper but more complex. Box filter is correct at this stage.
- HDR textures (`.hdr` files) should store as 16-bit float (`R16G16B16A16_SFLOAT`) to preserve the wider dynamic range.

**Dependency Gate:**
PNG and DDS textures import without error. Output `.omnixmat` binary passes all header validation checks. Mip chain is present and correctly sized. Corrupted inputs are handled gracefully.

**Validation Check:**
Import a 512×512 PNG, a 1024×1024 JPG, and a DDS file. Verify header fields for all three. Delete the PNG file and re-run the importer — it logs the missing source error and does not crash.

**Expected Deliverable:** `TextureImporter.h`, `TextureImporter.cpp` committed. Import test committed and passing.

---

### Week 14 — Mesh Importer

**Context:**
After textures, meshes are the second most fundamental asset type. Unlike textures, meshes have a richer structure — multiple vertex streams, index buffers, material slot assignments, optional skeletons, and bounding volumes. The mesh importer must extract all of this from multiple source formats and serialize it into a tightly-packed, runtime-optimized format.

**Primary Objective:**
Implement a mesh import pipeline that converts FBX, GLTF, and OBJ source files into the `.omnixmesh` runtime format.

**Architectural Rationale:**
The `.omnixmesh` format is designed for runtime performance, not human readability. Vertex data is interleaved (all attributes for vertex 0, then all for vertex 1, etc.) for cache-coherent iteration. This format can be directly `memcpy`'d into a GPU vertex buffer without per-vertex processing at load time.

**Linear Tasks:**

1. **Integrate mesh parsing libraries.** Add `cgltf` (single-header, GLTF/GLB parsing) for GLTF files. Implement a minimal OBJ parser inline (OBJ is simple enough to parse in ~100 lines). For FBX, evaluate: if FBX support is required, integrate `ufbx` (small, C, no dependencies) rather than Assimp (large, complex build). Implement `MeshImporter::ParseGLTF(path) → RawMeshData`, `ParseOBJ(path) → RawMeshData`, `ParseFBX(path) → RawMeshData`.

2. **Define `RawMeshData` and `OmnixVertex`.** `RawMeshData` contains: `std::vector<glm::vec3> positions`, `std::vector<glm::vec3> normals`, `std::vector<glm::vec4> tangents`, `std::vector<glm::vec2> uv0`, `std::vector<glm::vec2> uv1`, `std::vector<uint32_t> indices`, `std::vector<Submesh> submeshes` (each submesh has: `indexOffset`, `indexCount`, `materialSlotIndex`). `OmnixVertex` is the interleaved runtime struct: `{ vec3 position; vec3 normal; vec4 tangent; vec2 uv0; vec2 uv1; }` — 15 floats, 60 bytes per vertex.

3. **Generate tangent vectors.** Tangent vectors are required for normal mapping. If the source file does not supply tangents (OBJ and many FBX files do not), compute them using the Mikktspace algorithm. Integrate the `mikktspace` library (small, single-purpose). Generated tangents are stored in the `tangent.w` component: +1 or -1 for the bitangent sign.

4. **Compute bounding sphere.** For each submesh, compute an axis-aligned bounding box (AABB) from the vertex positions, then compute the enclosing sphere using Ritter's algorithm. Store as `BoundingSphere { vec3 center; float radius; }`. This is used for frustum culling (Phase 6) and physics broadphase.

5. **Serialize to `.omnixmesh` binary.** Format: (a) 8-byte magic `OMNIXMSH`; (b) format version; (c) `MeshHeader` containing: vertexCount, indexCount, submeshCount, vertexStride (= sizeof OmnixVertex), hasSkeleton flag; (d) BoundingSphere; (e) interleaved vertex buffer (`vertexCount * sizeof(OmnixVertex)` bytes); (f) index buffer (`indexCount * sizeof(uint32_t)` bytes); (g) submesh descriptors. Write to `Assets/Meshes/{assetName}.omnixmesh`.

**Technical Notes:**
- Tangent generation is frequently skipped and causes subtle visual bugs when normal mapping is added later. Generate tangents during import unconditionally, even if the current renderer does not use them — they will cost nothing if unused (they just sit in the vertex buffer) but will save a painful retroactive fix.
- The 60-byte `OmnixVertex` stride is deliberately chosen to be a power-of-two multiple of cache line alignment. This is not premature optimization — it is a one-time format decision that cannot be changed without re-importing all meshes.
- For models with skeletal animation, the vertex buffer needs additional data: 4 joint indices and 4 joint weights (8 floats, 32 bytes per vertex). Define a separate `OmnixSkinnedVertex` for skinned meshes. The `MeshHeader.hasSkeleton` flag tells the loader which vertex format to use.

**Dependency Gate:**
GLTF and OBJ meshes import to `.omnixmesh` with correct vertex count, index count, bounding sphere, and tangent vectors. Header validation passes.

**Validation Check:**
Import a cube (8 vertices, 36 indices). Verify: `MeshHeader.vertexCount == 8`, `indexCount == 36`, bounding sphere radius ≈ 0.866 (sqrt(3)/2 for a unit cube). Open `.omnixmesh` in a hex editor — confirm `OMNIXMSH` magic at offset 0.

**Expected Deliverable:** `MeshImporter.h`, `MeshImporter.cpp` committed. Import test committed and passing.

---

### Week 15 — Runtime Asset Loading Integration

**Context:**
Importers produce `.omnixmesh` and `.omnixmat` binary files on disk. This week wires those binaries into the runtime — loading them from disk into CPU memory and uploading them to the GPU. After this week, the engine can render an actual mesh with an actual texture for the first time using the new asset system.

**Primary Objective:**
Wire the texture and mesh importers into `AssetManager` and implement full GPU upload, producing a fully GPU-resident asset from a source file.

**Architectural Rationale:**
The GPU upload step is deliberately separated from the import step (Weeks 13–14). Import is a build-time or editor-time operation. Runtime loading reads the pre-imported binary format, which is faster (no decode, no mip generation). The separation allows the engine to ship runtime binaries without shipping source assets or the import pipeline.

**Linear Tasks:**

1. **Register loaders with `AssetManager`.** In `EngineRuntime::Initialize()`, after the Renderer and AssetManager are both initialized, call: `m_AssetManager.RegisterLoader(&m_TextureLoader)`, `m_AssetManager.RegisterLoader(&m_MeshLoader)`. The loaders require the Renderer's device handle to create GPU resources — pass this via the loader constructors (inject the Vulkan device and allocator pointers from `RuntimeContext`).

2. **Implement GPU upload in `TextureLoader::Load()`.** The runtime `TextureLoader` is distinct from the import-time `TextureImporter`. `TextureLoader` reads a `.omnixmat` binary (the already-imported file), validates the header, then: creates a `VkImage` with `VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT` and the correct format/dimensions/mip-count; creates a staging `VkBuffer` via VMA (Vulkan Memory Allocator); copies pixel data into the staging buffer; records a command buffer that: transitions the image to `VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL`, calls `vkCmdCopyBufferToImage` for all mip levels, transitions to `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL`; submits and waits.

3. **Implement GPU upload in `MeshLoader::Load()`.** `MeshLoader` reads `.omnixmesh` binary, validates header, then: creates a vertex `VkBuffer` with `VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT` sized to `vertexCount * vertexStride`; creates an index `VkBuffer` with `VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT` sized to `indexCount * sizeof(uint32_t)`; copies data via staging buffers; submits and waits. Populate a `RuntimeMesh` struct with the two `VkBuffer` handles and the submesh descriptor list.

4. **Call `AssetManager::LoadAll()` during engine boot.** After loaders are registered, call `m_AssetManager.ProcessImportQueue()` to import all dirty assets, then `m_AssetManager.LoadAll()` to GPU-upload all imported assets. Verify with a test draw call that a loaded mesh renders correctly.

5. **Destroy staging buffers after upload.** Staging buffers are CPU-visible temporary buffers used only during the upload transfer. After `vkQueueSubmit` and `vkWaitForFences`, destroy the staging buffers and free their VMA allocations. They must not persist after the upload is complete — this is a source of GPU memory waste if left alive.

**Technical Notes:**
- VMA (Vulkan Memory Allocator) should be used for all GPU memory allocation at this stage if it is not already integrated. Manual `vkAllocateMemory` calls do not scale — Vulkan implementations have limits on the number of separate allocations, and VMA pools allocations efficiently.
- The `vkQueueSubmit` + `vkWaitForFences` pattern (synchronous GPU upload) is correct and simple. Async upload (using a transfer queue and semaphores) is an optimization for later.
- The transition to `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL` must happen before the image is used in a descriptor set. Doing this as part of the upload command buffer (before the first render frame) is the cleanest approach.

**Dependency Gate:**
Mesh and texture assets are fully GPU-resident after `LoadAll()`. Vulkan validation layer reports zero errors during the full loading pass.

**Validation Check:**
Run Vulkan validation layers at maximum verbosity during `LoadAll()`. Output must contain zero errors and zero warnings. Issue a draw call using the loaded mesh and texture — it renders without corruption.

**Expected Deliverable:** `TextureLoader.h/cpp`, `MeshLoader.h/cpp` committed. GPU upload verified with a draw call.

---

### Week 16 — Asset Hot Reloading Foundation

**Context:**
Hot reloading — updating assets in a running engine without restart — is one of the highest-impact iteration-speed features an engine can provide. This week implements the foundation: file system watching, reload event queuing, and the mechanism to safely swap a GPU resource while the engine is rendering.

**Primary Objective:**
Implement live asset reload so modified source files update in the running engine without requiring a restart.

**Architectural Rationale:**
Hot reloading is architecturally non-trivial because it requires destroying and recreating GPU resources while the GPU may be using them. This is only safe because the deferred GPU destruction queue (Week 18) will handle the safe destruction. However, the reload queue and the file watcher can be built now — the GPU-safe destruction will be plumbed in during Week 18 with minimal changes to the reload logic.

**Linear Tasks:**

1. **Implement a filesystem watcher.** Use `inotify` (Linux), `ReadDirectoryChangesW` (Windows), or a cross-platform wrapper like `efsw` to monitor all asset directories for file write events. When a write event fires for a path that has a corresponding `.omnixmeta`, retrieve the `AssetHandle` from the sidecar and push it onto `m_ReloadQueue`.

2. **Implement `AssetManager::ProcessReloadQueue()`.** Called once per frame. For each handle in the queue: (a) update the `importTimestamp` in `AssetMetadata`; (b) re-run the appropriate importer to regenerate the `.omnixmat` or `.omnixmesh` binary; (c) call `ReloadAsset(handle)` to re-run the GPU loader, which produces a new GPU resource; (d) schedule the old GPU resource for deferred destruction (this call is a stub now, implemented in Week 18 — for now, destroy immediately after a `vkDeviceWaitIdle()`); (e) update `m_LoadedAssets` with the new resource pointer.

3. **Handle rapid file saves gracefully.** Many editors save files multiple times in quick succession (e.g., a save that writes a temp file then renames it). Debounce the reload: if the same handle appears in the queue multiple times within a 200ms window, coalesce the events into a single reload. Implement using a `std::unordered_map<AssetHandle, std::chrono::time_point>` tracking when each handle was last reloaded.

4. **Add a "Force Reload All" ImGui button.** In the debug tools layer (stub — full ImGui comes in Month 8), expose a `DebugReloadAll()` method on `AssetManager` that pushes every registered handle onto the reload queue. Wire this to a simple key press (`R` with modifier) for now.

5. **Write a rapid-reload stress test.** Using a test harness (not the full engine), modify a test texture file 20 times in rapid succession (simulating rapid save events). Verify: (a) after all events settle, the loaded asset reflects the last write; (b) no GPU validation errors occur; (c) no memory leaks are detected.

**Technical Notes:**
- The `vkDeviceWaitIdle()` call in step 2(d) is a blocking synchronization that stalls the entire GPU. This is acceptable for a development-only hot reload path but must never happen in production or performance-critical code. It will be replaced by the deferred destruction queue in Week 18.
- File watchers on different platforms behave differently. On Windows, `ReadDirectoryChangesW` can miss events if the buffer fills. Use a generous buffer size and implement overflow recovery (full directory rescan if the buffer overflows).
- Hot reloading must not be active in release builds. Gate the entire system behind `#if defined(DEBUG)`.

**Dependency Gate:**
Texture hot reload completes without GPU crash or validation error. Old resource is safely destroyed. The loaded asset reflects the most recent file write after debouncing.

**Validation Check:**
Open the engine in debug mode. Modify a loaded texture file externally. Within 500ms, the rendered output must reflect the new texture content without restarting the engine, without any Vulkan validation error.

**Expected Deliverable:** Filesystem watcher implemented. `ProcessReloadQueue()` implemented. Rapid-reload stress test passing.

---

# PHASE 3 — RENDERER STABILIZATION
## Months 5–6 | Weeks 17–24

> v0.2 renderer goals are correctness, stability, and lifetime management — not advanced graphics. The Vulkan backend must be a reliable substrate before any visual features are added.

---

## MONTH 5 — GPU Resource Lifetime & Descriptor Management

### Week 17 — GPU Resource Ownership Audit

**Context:**
Vulkan's most dangerous characteristic is that it offers no automatic resource lifetime management. Unlike OpenGL, which has a driver managing resource lifetimes, Vulkan requires the application to explicitly synchronize GPU work and destroy resources only after the GPU has finished using them. Any deviation from this produces GPU crashes that are often non-deterministic and extremely difficult to debug.

**Primary Objective:**
Document the ownership, creation responsibility, and safe destruction timing for every GPU resource type in use.

**Architectural Rationale:**
The GPU resource audit is to the renderer what the subsystem inventory was to the engine architecture. Without a clear picture of what exists and who owns it, the deferred destruction queue (Week 18) cannot be designed correctly. Every resource type requires a different destruction call — `vkDestroyBuffer`, `vkDestroyImage`, `vkDestroyPipeline` etc. — and must be destroyed in the right order (image views before images, framebuffers before render passes).

**Linear Tasks:**

1. **Enumerate all GPU resource types.** Produce a table of every Vulkan resource type currently created in the codebase: `VkBuffer`, `VkImage`, `VkImageView`, `VkSampler`, `VkPipeline`, `VkPipelineLayout`, `VkRenderPass`, `VkFramebuffer`, `VkDescriptorSetLayout`, `VkDescriptorPool`, `VkDescriptorSet`, `VkShaderModule`, `VkCommandPool`, `VkCommandBuffer`, `VkSemaphore`, `VkFence`, `VkSwapchainKHR`. For each type, note: which C++ object creates it, which destroys it, and the correct Vulkan destruction function.

2. **Classify destruction safety for each resource.** Mark each resource as one of: `FRAME_SAFE` (safe to destroy at the start of the next frame if double-buffered), `QUEUE_SAFE` (safe to destroy after `vkQueueWaitIdle()`), `DEVICE_SAFE` (requires `vkDeviceWaitIdle()` before destruction — only for swapchain, render pass, pipeline layout), or `UNSAFE` (currently being destroyed without synchronization).

3. **Implement `GpuHandle<T>` RAII wrapper.** Create a template wrapper: `template<typename T> class GpuHandle { T handle = VK_NULL_HANDLE; VkDevice device = VK_NULL_HANDLE; std::function<void(VkDevice, T)> destroyer; public: ~GpuHandle() { if (handle != VK_NULL_HANDLE) destroyer(device, handle); } /* move-only */ }`. All new GPU resources are wrapped in `GpuHandle`. This eliminates accidental double-free and resource leaks.

4. **Audit existing `vkDestroy*` call sites.** Find every `vkDestroyBuffer`, `vkDestroyImage`, `vkDestroyPipeline`, etc. in the codebase. Classify each as safe or unsafe based on the classification from step 2. Mark every unsafe destruction with a `// TODO-UNSAFE: replace with deferred destruction queue (Week 18)` comment.

5. **Produce `GPU_RESOURCE_OWNERSHIP.md`.** Document: resource type, Vulkan handle type, C++ owner class, creation function, destruction function, safety classification, and any special ordering constraints (e.g., "VkImageView must be destroyed before VkImage"). This document is the reference for the deferred destruction queue design.

**Technical Notes:**
- `VkDescriptorSet` is special — it is allocated from a `VkDescriptorPool` and is freed either by explicitly calling `vkFreeDescriptorSets` or by resetting the entire pool with `vkResetDescriptorPool`. Per-set freeing requires `VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT` on pool creation. If this flag is not set, per-set freeing is not possible and the set "lives" until the pool is reset.
- Semaphores and fences are typically created once per frame-in-flight and reused. They do not need deferred destruction unless they are created dynamically (which they should not be).
- Swapchain images are owned by the swapchain — do not call `vkDestroyImage` on them. Their image views (`VkImageView`) are yours to create and destroy.

**Dependency Gate:**
Every GPU resource type in use is documented in `GPU_RESOURCE_OWNERSHIP.md` with its safety classification. All unsafe destruction sites are tagged.

**Validation Check:**
Enable Vulkan validation layers at maximum verbosity. Perform a clean boot-and-shutdown cycle. Zero lifetime or synchronization validation errors. Every tagged `TODO-UNSAFE` site is accounted for in the document.

**Expected Deliverable:** `GPU_RESOURCE_OWNERSHIP.md` committed. `GpuHandle<T>` implemented and applied to new resources.

---

### Week 18 — Deferred GPU Destruction Queue

**Context:**
The GPU processes commands asynchronously. When frame N submits a draw call that references texture T, the CPU may have moved on to frame N+1 (or N+2 in a triple-buffered setup) before the GPU finishes frame N. If the CPU destroys texture T during frame N+1, the GPU is still using it — result: GPU crash, often with a cryptic VK_ERROR_DEVICE_LOST. The deferred destruction queue solves this by tracking which frame a resource was last referenced and only destroying it after the GPU has confirmed that frame is complete.

**Primary Objective:**
Implement a frame-indexed deferred destruction queue that makes it safe to destroy any GPU resource at any time.

**Architectural Rationale:**
The deferred destruction queue is one of the most important infrastructure pieces in any Vulkan engine. Once it exists, all resource destruction (assets being unloaded, hot-reloaded, pipelines being rebuilt) becomes safe by default. Without it, every resource destruction requires a `vkDeviceWaitIdle()` which stalls the GPU completely.

**Linear Tasks:**

1. **Design the queue structure.** `DeferredDestructionQueue` stores entries indexed by frame: `std::vector<PendingDestruction> m_Buckets[MAX_FRAMES_IN_FLIGHT]`. `PendingDestruction` is a `std::function<void()>` — a lambda that calls the appropriate `vkDestroy*` function with the captured handle. Using `std::function` allows the queue to destroy any resource type without templating.

2. **Implement `Schedule(std::function<void()> destroyer, uint32_t frameIndex)`.** Pushes the destroyer lambda into the bucket for `frameIndex % MAX_FRAMES_IN_FLIGHT`. The frame index is the current frame index at the time of destruction — the resource will be destroyed `MAX_FRAMES_IN_FLIGHT` frames later.

3. **Implement `Flush(uint32_t completedFrameIndex)`.** Called at the start of each frame, after acquiring the swapchain image and after the fence for the current frame-in-flight slot is signaled (meaning the GPU has completed that frame). Iterates all lambdas in `m_Buckets[completedFrameIndex % MAX_FRAMES_IN_FLIGHT]`, calls each one, then clears the bucket.

4. **Replace all tagged unsafe destructions.** Go through every `// TODO-UNSAFE` comment from Week 17. Replace the direct `vkDestroy*` call with `m_DestructionQueue.Schedule([device, handle]() { vkDestroyBuffer(device, handle, nullptr); }, m_CurrentFrameIndex)`. For hot reload path (Week 16), remove the `vkDeviceWaitIdle()` and replace with `Schedule()`.

5. **Add debug statistics.** Track `m_PendingDestructionCount` — incremented by `Schedule()`, decremented by `Flush()`. Expose this in the runtime stats panel (stub). If `m_PendingDestructionCount` exceeds a threshold (e.g., 1000) log a `[Renderer] WARN` — this may indicate resources are being scheduled for destruction faster than frames are completing.

**Technical Notes:**
- `MAX_FRAMES_IN_FLIGHT` is typically 2 (double buffering) or 3 (triple buffering). Define this as a constant in the renderer configuration. The deferred queue must use the same value as the swapchain.
- The `Flush()` call must happen after the frame-in-flight fence is waited on. The fence wait guarantees the GPU has finished all work for that frame slot. Only after this is it safe to destroy resources that were last used in that frame.
- `std::function` has a small heap allocation overhead per lambda capture. For high-throughput destruction (hundreds per frame), consider using a custom arena-allocated function wrapper. At this stage, `std::function` is acceptable.

**Dependency Gate:**
Zero synchronous `vkDestroy*` calls remain outside the destruction queue. All destruction is frame-indexed. Under Vulkan validation layers, zero synchronization hazard errors occur even when 1,000 resources are destroyed in a single frame.

**Validation Check:**
Destroy 1,000 textures in a single frame. Verify Vulkan validation layers report zero errors. Verify `m_PendingDestructionCount` reaches 1,000 then decrements to 0 over the next `MAX_FRAMES_IN_FLIGHT` frames.

**Expected Deliverable:** `DeferredDestructionQueue.h/cpp` committed. All unsafe destructions replaced. Hot reload path updated.

---

### Week 19 — Descriptor Allocator

**Context:**
Descriptor sets in Vulkan are allocated from descriptor pools. Ad-hoc pool creation (one pool per pipeline, or one per object) leads to pool exhaustion, wasted pool space, and complex lifetime management. A centralized descriptor allocator with pool growth management solves all of these problems at once.

**Primary Objective:**
Replace all ad-hoc descriptor set allocation with a centralized, pool-growing `DescriptorAllocator`.

**Architectural Rationale:**
The descriptor allocator is prerequisite to the material system (Week 22), which needs to allocate descriptor sets for each material instance. Without a scalable allocator, the material system either pre-allocates too many descriptor sets (wasteful) or fails at runtime when a pool fills (crashing).

**Linear Tasks:**

1. **Implement `DescriptorAllocator` with pool growth.** Internal state: `std::vector<VkDescriptorPool> m_FullPools`, `std::vector<VkDescriptorPool> m_FreePools`, `VkDescriptorPool m_CurrentPool`. `AllocatePool()` creates a new `VkDescriptorPool` with a generous set of descriptor type ratios (e.g., 10:1 sampled images to uniform buffers — tune based on observed usage). When `vkAllocateDescriptorSets` returns `VK_ERROR_FRAGMENTED_POOL` or `VK_ERROR_OUT_OF_POOL_MEMORY`, move the current pool to `m_FullPools` and allocate a new one from `m_FreePools` (or create a new pool if `m_FreePools` is empty).

2. **Implement `Allocate(VkDescriptorSetLayout layout) → VkDescriptorSet`.** Attempts allocation from `m_CurrentPool`. On failure, grows the pool as described above and retries. This method must never fail or throw — it either returns a valid set or crashes with a descriptive assertion.

3. **Implement `ResetPools()` for per-frame transient descriptors.** For descriptor sets that are rebuilt every frame (e.g., per-frame uniform buffer bindings), a separate "transient" `DescriptorAllocator` instance is reset at the start of each frame using `vkResetDescriptorPool`. This allows reuse of pool memory without per-set freeing overhead. The persistent allocator (for material descriptors) is never reset — only grown.

4. **Replace all `vkAllocateDescriptorSets` call sites.** Find every `vkAllocateDescriptorSets` call in the renderer. Replace each with `m_DescriptorAllocator.Allocate(layout)`. For per-frame descriptors, use `m_TransientAllocator.Allocate(layout)` and call `m_TransientAllocator.ResetPools()` at the start of each frame.

5. **Validate pool exhaustion handling.** Write a test that calls `Allocate()` 100,000 times (more than any single pool can hold). Verify: (a) all 100,000 allocations succeed; (b) more than one pool was created (growth happened); (c) Vulkan validation layers report zero errors.

**Technical Notes:**
- The pool size ratios (how many of each descriptor type per pool) should be generous. A common pattern is to allocate pools for 1000 descriptor sets with the following type ratios: 5× sampled images, 2× uniform buffers, 2× storage buffers per set. Adjust based on actual shader usage after profiling.
- `VkDescriptorSet` from the transient allocator must not be used across frames. Enforce this at the debug level by storing the frame index in which a transient set was allocated and asserting that it is not referenced in a later frame.
- Per-set freeing (`vkFreeDescriptorSets`) requires `VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT`. The transient allocator does NOT need this flag (it uses pool reset). The persistent allocator either uses this flag OR relies on pool recreation (simpler, slightly less efficient).

**Dependency Gate:**
All descriptor allocation flows through `DescriptorAllocator`. Pool exhaustion triggers growth, not failure. Vulkan validation layers report zero errors during a 100,000-allocation stress test.

**Validation Check:**
Allocate 100,000 descriptor sets. Verify all succeed. Check that `m_FullPools.size() > 0` (pool growth occurred). Vulkan validation: zero errors.

**Expected Deliverable:** `DescriptorAllocator.h/cpp` committed. All `vkAllocateDescriptorSets` call sites replaced.

---

### Week 20 — GPU Resource Tracking & Statistics

**Context:**
After two weeks of GPU resource lifetime work, the engine has the infrastructure to manage resources safely. Now it needs the visibility to know whether it is managing them correctly at runtime. `GpuMemoryTracker` provides the data that drives the runtime stats panel and, more importantly, surfaces memory leaks before they become problems.

**Primary Objective:**
Add full runtime visibility into GPU memory allocation, active resource counts, and per-frame GPU statistics.

**Architectural Rationale:**
Memory leaks in GPU resources are invisible to CPU-side memory checkers like Address Sanitizer. A GPU buffer that is never freed shows up in Vulkan validation output and in GPU memory usage (which eventually causes `VK_ERROR_OUT_OF_DEVICE_MEMORY`), but only if you are watching for it. `GpuMemoryTracker` makes these problems visible early.

**Linear Tasks:**

1. **Instrument all GPU resource creation.** Wrap every `vkCreateBuffer`, `vkCreateImage`, `vkCreatePipeline`, etc. in a logging call: `GpuMemoryTracker::OnAllocate(ResourceType type, VkDeviceSize size, OwnerSubsystem owner)`. Similarly wrap every `vkDestroy*` in `GpuMemoryTracker::OnFree(ResourceType type, VkDeviceSize size)`. If VMA is used, hook into VMA's `VmaAllocationCallbacks` for buffer and image memory.

2. **Implement `GpuMemoryTracker` statistics.** Track atomically (for thread safety): `totalAllocatedBytes`, `peakAllocatedBytes`, `activeResourceCounts[ResourceType]` (one counter per resource type), `allocationCountThisFrame`, `freeCountThisFrame`. Reset per-frame counters at the start of each frame.

3. **Track per-frame rendering statistics.** Add counters to the render path: `drawCallCount`, `verticesRendered`, `trianglesRendered`, `pipelineSwitchCount`, `descriptorSetBindCount`. Increment these at the appropriate points in `RenderFrame()`. Reset at the start of each frame.

4. **Expose all statistics via a simple struct.** Define `GpuStats { uint64_t totalMemoryBytes; uint64_t peakMemoryBytes; uint32_t activeBuffers; uint32_t activeImages; uint32_t activePipelines; uint32_t drawCallsThisFrame; uint32_t trianglesThisFrame; uint32_t pendingDestructions; }`. `GpuMemoryTracker::GetStats() → GpuStats` returns a snapshot. The runtime stats panel (ImGui, Month 8) will call this.

5. **Write a per-frame leak detection assertion.** At the end of each frame (in debug builds): if `allocationCountThisFrame > 0 && freeCountThisFrame == 0`, log `[Renderer] DEBUG: {allocationCountThisFrame} GPU allocations this frame — expected if loading`. If this number is non-zero for 60+ consecutive frames with no asset loading, log a `WARN` — this suggests a leak.

**Technical Notes:**
- Atomic counters (`std::atomic<uint64_t>`) are required if the renderer uses multiple threads (e.g., a separate transfer thread). Even for single-threaded rendering, atomics are cheap and eliminate data races if threading is added later.
- `peakAllocatedBytes` is useful for memory budget planning — it shows the worst-case memory usage seen during a session, even if current usage is lower (due to freed resources).
- The per-frame allocation/free counts should not alarm on the first few frames (asset loading causes legitimate allocations). The leak detection heuristic should activate only after a "steady state" is detected (e.g., after 10 frames with identical allocation counts).

**Dependency Gate:**
`GpuMemoryTracker` reports accurate per-type allocation totals. Per-frame counters reset correctly at frame boundaries. Zero off-by-one accounting errors.

**Validation Check:**
Run the engine for 60 frames with no new asset loading. The `totalAllocatedBytes` and `activeBuffers/Images/Pipelines` counters must be flat (no change) across those 60 frames. Any upward trend indicates a leak.

**Expected Deliverable:** `GpuMemoryTracker.h/cpp` committed. All GPU resource creation/destruction instrumented.

---

## MONTH 6 — Material System & Renderer Stabilization

### Week 21 — Material Asset Format Design

**Context:**
Without a material system, the renderer contains hardcoded shader references and pipeline configurations. Adding a new visual appearance requires changing C++ source code and recompiling. A material asset externalizes these decisions — shader references, texture bindings, render states — into a data file that can be authored and hot-reloaded without touching compiled code.

**Primary Objective:**
Define the `.omnixmat` material file format and implement the material importer.

**Architectural Rationale:**
The material system is the bridge between the asset system (handles, registry, importers) and the renderer (pipelines, descriptor sets, shaders). It must be designed to work with both — referencing assets by `AssetHandle` and producing GPU pipeline state. Getting the format right now prevents a painful redesign when the renderer's shader reflection is added (Week 23).

**Linear Tasks:**

1. **Define `.omnixmat` file schema.** The format is human-readable (JSON or a custom key-value format) for authoring. Fields: `name` (string), `vertexShader` (path to SPIR-V, or shader asset handle), `fragmentShader` (path to SPIR-V), `textures` (array of `{ slot: string, handle: AssetHandle }`), `uniforms` (array of `{ name: string, type: string, value: any }`), `renderState` (object with: `blendMode`, `depthTestEnabled`, `depthWriteEnabled`, `cullMode`, `fillMode`). Write three example `.omnixmat` files: `opaque_lit.omnixmat`, `transparent_unlit.omnixmat`, `debug_wireframe.omnixmat`.

2. **Implement `MaterialImporter : IAssetLoader`.** Parse the `.omnixmat` JSON into a `MaterialAssetData` struct. Validate: all shader paths resolve to readable SPIR-V files; all texture handles resolve in the `AssetRegistry`. If validation fails, return false from `Load()` with a descriptive error listing every broken reference. Do not partially load a material — all-or-nothing.

3. **Define `MaterialAssetData` struct.** Fields: `name`, `vertexShaderPath`, `fragmentShaderPath`, `TextureBinding[] textureBindings` (each with slot name and `AssetHandle`), `UniformValue[] uniformValues`, `RenderState renderState`. This is the CPU-side representation of a loaded material, before GPU resources are created.

4. **Register `MaterialImporter` with `AssetManager`.** In `EngineRuntime::Initialize()`, after `TextureLoader` and `MeshLoader` are registered, register `MaterialImporter`. Process all `.omnixmat` files in `Assets/Materials/` during `ProcessImportQueue()`.

5. **Write authoring test.** Author all three example `.omnixmat` files by hand. Run the importer on all three. Verify: (a) all parse without error; (b) all referenced textures resolve to valid handles; (c) a deliberately wrong texture handle produces a clear error message naming the unresolved handle.

**Technical Notes:**
- The material format references shaders by path rather than by `AssetHandle` at this stage, because the shader compilation pipeline (GLSL → SPIR-V) is not yet integrated into the asset system. Path references are acceptable for shader files since they are engine-level assets, not project-level assets.
- `RenderState` is deceptively important. Two materials that are identical except for blend mode require different `VkPipeline` objects in Vulkan (unlike OpenGL where blend mode was set as dynamic state). The `PipelineCache` (Week 22) uses the full render state as part of its cache key.
- The human-readable format is for authoring only. Consider adding a step that converts `.omnixmat` to a binary format for faster runtime parsing — this is an optimization for later.

**Dependency Gate:**
All three example `.omnixmat` files parse correctly. All referenced asset handles resolve. Invalid material files produce clear error messages without crashing.

**Validation Check:**
Author a `.omnixmat` referencing a texture handle that does not exist in the registry. Run the importer. It must log `[Materials] ERROR: Unresolved texture handle {handle} in material {name}` and return false. No partial material state is written.

**Expected Deliverable:** Three example `.omnixmat` files committed. `MaterialImporter.h/cpp` committed. Integration test passing.

---

### Week 22 — Material Runtime Object & Pipeline Binding

**Context:**
The importer produces CPU-side `MaterialAssetData`. This week creates the GPU-side `Material` runtime object: it owns the `VkDescriptorSet` binding its textures, holds the cached `VkPipeline` for its shader/renderstate combination, and provides a single `Bind()` call that sets up the GPU state for a draw call. After this week, the renderer can draw with arbitrary materials without any hardcoded shader references.

**Primary Objective:**
Implement the `Material` runtime class and integrate it into the Vulkan render pipeline binding path.

**Linear Tasks:**

1. **Implement `class Material`.** Members: `std::string name`, `VkDescriptorSet textureDescriptorSet` (bound textures), `VkPipelineLayout pipelineLayout`, `std::array<TextureBinding, MAX_TEXTURE_SLOTS> textureBindings`, `RenderState renderState`, `AssetHandle vertexShaderHandle`, `AssetHandle fragmentShaderHandle`. Constructor takes a `MaterialAssetData` and a `RuntimeContext` reference.

2. **Implement `Material::Build(RuntimeContext& ctx)`.** This creates all GPU resources: (a) loads vertex and fragment SPIR-V from paths in `materialData`; (b) creates `VkShaderModule` for each; (c) allocates a `VkDescriptorSet` via `DescriptorAllocator` for texture bindings; (d) calls `vkUpdateDescriptorSets` to bind each texture's `VkImageView` and `VkSampler` to the descriptor set; (e) does NOT create the `VkPipeline` yet — that is cached separately.

3. **Implement `PipelineCache`.** Create `PipelineCache` as a member of the renderer. Key type: `struct PipelineCacheKey { VkShaderModule vs; VkShaderModule fs; VertexLayout vertexLayout; RenderState renderState; VkRenderPass renderPass; }` with a hash function. Method: `GetOrCreate(PipelineCacheKey) → VkPipeline`. If the key is not in the cache, create the `VkGraphicsPipeline` and cache it. If it is already cached, return the cached handle — zero redundant pipeline creation.

4. **Implement `Material::Bind(VkCommandBuffer cmd, VkRenderPass renderPass, VertexLayout vertexLayout)`.** Calls: (a) `VkPipeline pipeline = m_PipelineCache.GetOrCreate({vs, fs, vertexLayout, renderState, renderPass})`; (b) `vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline)`; (c) `vkCmdBindDescriptorSets(cmd, ..., &textureDescriptorSet, ...)`. This is the entire GPU state setup for a draw call. No other code should call `vkCmdBindPipeline` directly.

5. **Replace all hardcoded pipeline bindings in the renderer.** Find every `vkCmdBindPipeline` call. Replace each with a `material->Bind(cmd, renderPass, vertexLayout)` call. After this change, the renderer has zero hardcoded shader references — all shader selection is driven by material data.

**Technical Notes:**
- `VkPipeline` objects are expensive to create (often 10–100ms on first creation). The `PipelineCache` is critical — without it, every unique material + vertex format combination would stall the GPU on first use.
- Vulkan 1.3 introduced `VK_EXT_extended_dynamic_state` which allows some pipeline state (blend mode, cull mode) to be set dynamically rather than baked into the pipeline. If supported by the target hardware, this can significantly reduce the number of unique pipelines. This is an optimization for later — bake all state into the pipeline for now.
- `vkCmdBindPipeline` is not free — avoid redundant binds. Add a `m_LastBoundPipeline` cache in the renderer's command recording path: if the new pipeline is the same as the last bound one, skip the bind call.

**Dependency Gate:**
Materials bind correctly. `PipelineCache` prevents duplicate pipeline creation. Draw calls use materials, not hardcoded pipelines.

**Validation Check:**
Render 100 meshes sharing two materials. Verify `PipelineCache` contains exactly 2 entries. Vulkan validation: zero errors. Render output is visually correct for both materials.

**Expected Deliverable:** `Material.h/cpp`, `PipelineCache.h/cpp` committed. Renderer updated to use material-driven pipeline binding.

---

### Week 23 — Shader Reflection & Descriptor Layout Automation

**Context:**
Currently, descriptor set layouts (what bindings a shader expects) are hardcoded in C++. Every time a shader is changed, the C++ layout must be manually updated to match. This is error-prone and creates tight coupling between shader authoring and engine code. SPIR-V reflection breaks this coupling — the descriptor layout is derived automatically from the compiled shader.

**Primary Objective:**
Use SPIR-V reflection to automatically derive descriptor set layouts and push constant ranges from compiled shaders.

**Linear Tasks:**

1. **Integrate `spirv-reflect`.** Add the `SPIRV-Reflect` library to the project. Create `ShaderReflector.h` with `struct ShaderReflectionData { std::vector<DescriptorBindingInfo> bindings; std::vector<PushConstantRange> pushConstants; VertexInputLayout vertexInput; }` where `DescriptorBindingInfo` contains: `set`, `binding`, `name`, `descriptorType`, `descriptorCount`, `stageFlags`.

2. **Implement `ShaderReflector::Reflect(const std::vector<uint32_t>& spirvWords) → ShaderReflectionData`.** Call `spvReflectCreateShaderModule()`, iterate all descriptor bindings and push constant blocks, populate `ShaderReflectionData`. Return the result.

3. **Automate `VkDescriptorSetLayout` creation.** In `Material::Build()`, after loading SPIR-V for vertex and fragment shaders, call `Reflect()` on both. Merge the binding lists (combining `stageFlags` for bindings that appear in both stages). Use the merged binding list to call `vkCreateDescriptorSetLayout` — no hardcoded `VkDescriptorSetLayoutBinding` arrays anywhere in the codebase after this.

4. **Automate `VkPipelineLayout` creation.** Use the merged push constant ranges from both shader stages to call `vkCreatePipelineLayout`. The pipeline layout is now fully derived from shader reflection — no hardcoded push constant declarations.

5. **Add reflection-vs-material validation.** When building a material, compare the reflected binding names against the binding names declared in the `.omnixmat` file. If a binding declared in `.omnixmat` is not found in the shader, log `[Materials] WARN: Declared binding '{name}' not found in shader — check material file`. If the shader requires a binding not declared in `.omnixmat`, log `[Materials] ERROR: Shader requires binding '{name}' but it is not declared in the material`. This validation catches authoring errors early.

**Technical Notes:**
- SPIR-V reflection is performed at engine startup (when shaders are first loaded), not at runtime per-frame. The cost is one-time.
- The vertex input layout from reflection (`ShaderReflectionData.vertexInput`) gives you the attribute locations the shader expects. Use this to validate that the `OmnixVertex` struct provides all required attributes — and log an error if a required attribute (e.g., `uv1`) is missing from the mesh format.
- Keep `VkShaderModule` objects alive as long as the `VkPipeline` using them is alive. Destroying a shader module invalidates all pipelines created from it in some validation layer configurations.

**Dependency Gate:**
Zero hardcoded `VkDescriptorSetLayoutBinding` arrays remain. Descriptor layouts and push constant ranges are 100% reflection-derived. Material-shader validation catches binding mismatches.

**Validation Check:**
Add a new binding to a test shader SPIR-V. Rebuild. The engine must automatically create a descriptor set layout with the new binding without any C++ changes. Validation log confirms the binding was reflected.

**Expected Deliverable:** `ShaderReflector.h/cpp` committed. Material build system updated to use reflection.

---

### Week 24 — Renderer Stability Pass & Swapchain Robustness

**Context:**
The renderer has grown significantly during Months 5–6. This week is a dedicated stability and hardening pass. The most common source of Vulkan renderer crashes in development is swapchain lifecycle errors (window resizing, minimization, fullscreen toggling) and synchronization races (submitting a frame before the previous one completes). This week eliminates both categories.

**Primary Objective:**
Harden the renderer against all known crash and validation-error scenarios, with special focus on swapchain robustness.

**Linear Tasks:**

1. **Implement robust swapchain recreation.** `vkAcquireNextImageKHR` and `vkQueuePresentKHR` can return `VK_ERROR_OUT_OF_DATE_KHR` or `VK_SUBOPTIMAL_KHR` on window resize, display mode change, or minimize/restore. Implement `RecreateSwapchain()` that: waits for device idle, destroys framebuffers and image views, recreates swapchain, recreates framebuffers and image views at the new resolution, updates the viewport/scissor. Call it whenever `OUT_OF_DATE_KHR` or `SUBOPTIMAL_KHR` is returned.

2. **Implement per-frame in-flight synchronization.** Maintain `VkFence m_InFlightFences[MAX_FRAMES_IN_FLIGHT]` and `VkSemaphore m_ImageAvailableSemas[MAX_FRAMES_IN_FLIGHT]`, `VkSemaphore m_RenderFinishedSemas[MAX_FRAMES_IN_FLIGHT]`. At the start of each frame: call `vkWaitForFences(m_InFlightFences[currentFrame])` (blocks until the GPU has finished this frame slot), reset the fence, acquire the next swapchain image with `m_ImageAvailableSemas[currentFrame]`, record commands, submit with wait on `imageAvailable` and signal on `renderFinished`, present with wait on `renderFinished`.

3. **Handle zero-size window (minimization).** When the window is minimized on some platforms, the swapchain extent becomes 0×0. `vkCreateSwapchainKHR` with a 0×0 extent may fail or produce undefined behavior. Add an explicit check: if `windowWidth == 0 || windowHeight == 0`, skip the frame entirely (no `vkAcquireNextImageKHR`, no rendering) and poll for window resize events instead.

4. **Enable Vulkan validation layers unconditionally in debug builds.** In the Vulkan instance creation, add validation layer names to `VkInstanceCreateInfo.ppEnabledLayerNames` when `DEBUG` is defined. Set up `VkDebugUtilsMessengerEXT` with a callback that routes Vulkan messages to the engine log with appropriate severity levels. Log a startup message: `[Renderer] Vulkan validation layers: ACTIVE (debug build)`.

5. **Run a 10-minute stress loop.** Write a test that: renders 60fps, resizes the window to random sizes every 10 seconds (including 1×1 and back to maximized), minimizes and restores the window every 30 seconds, and changes fullscreen state every 60 seconds. Run for 10 minutes. Zero crashes, zero validation errors, zero frames with corrupted output.

**Technical Notes:**
- The window resize handler should be debounced — do not recreate the swapchain on every `WM_SIZE` event (Windows can send dozens per resize gesture). Recreate on the next frame after resize events have settled.
- `vkWaitForFences` with a timeout is preferable to infinite timeout (`UINT64_MAX`). Use a generous timeout (5 seconds) and log `[Renderer] ERROR: Frame fence timeout` if it fires — this indicates a GPU hang, which is a separate debugging problem.
- The per-frame in-flight synchronization pattern (semaphore + fence per frame slot) is the canonical Vulkan synchronization pattern. It is not an optimization — it is required for correct behavior with any number of frames in flight greater than 1.

**Dependency Gate:**
Renderer survives window resize (including 1×1), minimize, and restore without crash or validation error. Per-frame fences prevent all CPU/GPU synchronization races.

**Validation Check:**
Run the 10-minute stress loop. Zero crashes. Vulkan validation log: zero errors. Resize window to 1×1 and immediately back to 1920×1080 — rendering resumes correctly on the very next frame.

**Expected Deliverable:** Robust swapchain recreation implemented. Per-frame in-flight synchronization implemented. Stress loop result archived.

---

# PHASE 2 — ECS STABILIZATION
## Month 7 | Weeks 25–28

---

## MONTH 7 — ECS Stabilization

### Week 25 — ECS Storage Layout Audit & Benchmarking

**Context:**
ECS is described as "functional" in v0.1. Functional is not the same as correct, stable, or scalable. Before modifying the ECS, its actual performance characteristics must be measured. Without baseline numbers, there is no way to know whether a change is an improvement or a regression. This week establishes those numbers.

**Primary Objective:**
Measure and document actual ECS performance baseline before making any structural changes.

**Linear Tasks:**

1. **Write a component iteration benchmark.** Using a high-resolution timer (`std::chrono::high_resolution_clock`), measure: (a) time to iterate 100,000 entities with a `Transform` + `Velocity` component query (read both, write transform); (b) time to create 10,000 entities with two components in a single frame; (c) time to destroy 10,000 entities in a single frame; (d) time for a 100,000-entity iteration after 50% of entities have been deleted (tests fragmentation). Run each benchmark 10 times, record min/median/max.

2. **Document the current storage layout.** Look at the actual data structures used for component storage: Is it a sparse set? A packed array with a free-list? A hash map? A flat vector? For each, measure: iteration cost (ns/entity), insertion cost (ns/entity), deletion cost (ns/entity). The iteration cost is the most important — simulation systems run every frame.

3. **Run Valgrind massif on the iteration benchmark.** `valgrind --tool=massif` tracks heap allocations. Run the 100,000-entity iteration benchmark under massif. Verify that zero heap allocations occur during the iteration loop itself (only during entity creation). Any per-iteration allocation is a bug — fix it.

4. **Detect fragmentation.** After creating 100,000 entities, randomly delete 50% of them (alternating), then create 10,000 more. Measure iteration time for the remaining entities. Compare to the baseline iteration time with a full, unfragmented set. If the post-fragmentation iteration is more than 20% slower, the storage layout has a fragmentation problem.

5. **Produce `ECS_BENCHMARK_BASELINE.md`.** Record all measured numbers: ns/entity for each operation, heap allocation counts, fragmentation delta. These numbers become the regression gate — any future ECS change that makes these numbers significantly worse must be justified.

**Technical Notes:**
- Use large entity counts (100K+) for meaningful benchmarks. At 1,000 entities, even an inefficient layout will appear fast.
- The Valgrind massif check is critical: allocating inside an iteration loop means the allocator is called tens of thousands of times per frame. This causes massive variance in frame time and is a common source of hitching.
- "Fragmentation" in ECS context means holes in the component array where deleted entities used to be. Some ECS implementations swap-delete (move the last element into the deleted slot) to maintain a packed array. Others use free-lists and tolerate holes. Swap-delete is faster for iteration but changes entity order (relevant for determinism testing in Week 27).

**Dependency Gate:**
Baseline numbers recorded in `ECS_BENCHMARK_BASELINE.md`. Zero heap allocations during iteration confirmed by Valgrind. Fragmentation delta measured.

**Validation Check:**
Run the benchmark suite on a clean checkout and verify all numbers land within 10% of each other across three runs (reproducibility check). If variance is > 10%, the benchmark is not measuring what we think it is — investigate noise sources (background processes, timer resolution).

**Expected Deliverable:** `ECS_BENCHMARK_BASELINE.md` committed. Benchmark suite committed as a runnable test.

---

### Week 26 — EntityCommandBuffer Implementation

**Context:**
Structural ECS operations — creating entities, destroying entities, adding or removing components — are currently performed synchronously, potentially during system iteration. This is dangerous: if a system destroys an entity while iterating over entities, the iterator may be invalidated (depending on the storage layout), causing silent data corruption or crashes. The `EntityCommandBuffer` is the fix: buffer all structural changes and apply them atomically after all systems have run.

**Primary Objective:**
Implement `EntityCommandBuffer` and make it the only way to perform structural ECS changes.

**Linear Tasks:**

1. **Define the command types.** Create `EntityCommandBuffer.h` with an internal `Command` variant (or use a tagged union / std::variant): `CreateEntityCommand { ComponentInitializers... }`, `DestroyEntityCommand { EntityID }`, `AddComponentCommand<T> { EntityID; T component; }`, `RemoveComponentCommand<T> { EntityID; }`. Store commands in a `std::vector<Command> m_PendingCommands`.

2. **Implement the four public methods.** `CreateEntity() → EntityID` (returns a provisional ID — not yet live in the world), `DestroyEntity(EntityID)`, `AddComponent<T>(EntityID, T)`, `RemoveComponent<T>(EntityID)`. All methods push a command onto `m_PendingCommands` and return immediately without touching the `ECSWorld`. The provisional `EntityID` for `CreateEntity()` uses a negative index or a temporary namespace to avoid collisions with live entity IDs.

3. **Implement `Playback(ECSWorld& world)`.** Iterates `m_PendingCommands` in order and applies each to the world. For `CreateEntityCommand`: creates the entity, gets a real ID, applies all queued component adds for the provisional ID. For `DestroyEntityCommand`: destroys the entity (deferred so it cannot be destroyed while being iterated). After `Playback()`, clears `m_PendingCommands`.

4. **Add the Playback stage to the main loop.** In `EngineRuntime::Run()`, the frame loop becomes: `Input.Update() → Systems.Execute(cmd) → cmd.Playback(world) → Renderer.RenderFrame() → Profiler.EndFrame()`. Every system receives a reference to the `EntityCommandBuffer` for structural changes, not a reference to the world.

5. **Enforce the "no direct structural modification" rule.** Remove the `ECSWorld::CreateEntity()`, `DestroyEntity()`, `AddComponent()`, `RemoveComponent()` methods from the public API (or mark them `[[deprecated]]` with a clear message). Systems must use `EntityCommandBuffer`. If a system attempts to call a removed method, it gets a compile error. Document this rule in `CODING_STANDARDS.md`.

**Technical Notes:**
- The provisional `EntityID` for newly created entities is only valid within the same frame's command buffer. If system B needs to reference an entity created by system A in the same frame, this requires careful ordering. The simplest solution: systems that create entities run first, and their provisional IDs are resolved before systems that reference those entities run.
- `Playback()` should be the only place that calls `ECSWorld::CreateEntity()` etc. This makes the playback the single mutation point for the ECS, which is critical for determinism (Week 27).
- The `EntityCommandBuffer` does not need to be thread-safe at this stage — it is used single-threaded. Thread-safe multi-writer command buffers are an ECS parallelism optimization for later.

**Dependency Gate:**
All structural ECS changes go through `EntityCommandBuffer`. Direct structural modification from system code is blocked at compile time (or fails at runtime with a clear assertion message). `Playback()` is called exactly once per frame after all systems execute.

**Validation Check:**
Attempt to call `world.DestroyEntity(id)` directly inside a system — compile error (or assertion: "Direct structural modification is forbidden. Use EntityCommandBuffer."). Verify that an entity created in frame N via `CreateEntity()` is live in the world at the start of frame N+1 (after the next `Playback()`).

**Expected Deliverable:** `EntityCommandBuffer.h/cpp` committed. `ECSWorld` public API narrowed. Main loop updated.

---

### Week 27 — ECS Determinism Validation

**Context:**
Determinism — the property that the same input always produces the same output — is one of Omnix's long-term architectural goals. It enables replay systems (record inputs, replay and get identical simulation), networking (authoritative server simulation), and debugging (reproduce a bug by replaying the exact sequence of inputs that triggered it). This week proves the ECS achieves bitwise determinism.

**Primary Objective:**
Prove through snapshot comparison that ECS simulation is bitwise-identical across multiple independent runs with the same input.

**Linear Tasks:**

1. **Implement `ECSSnapshot`.** Create `ECSSnapshot.h` with a method `ECSSnapshot::Capture(const ECSWorld& world) → std::vector<uint8_t>`. The snapshot serializes every entity and every component in a deterministic order (sorted by `EntityID`, then by component type ID within each entity). The output is a flat byte buffer — 100% deterministic layout for a given world state.

2. **Implement snapshot comparison.** `ECSSnapshot::Compare(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) → bool` returns true if `a == b` byte-for-byte. On false, print the first differing byte offset and the entity/component it corresponds to — this makes debugging non-determinism much easier.

3. **Write the determinism test.** A test harness that: creates a fixed ECS world with 1,000 entities and three component types, runs 1,000 ticks with a deterministic set of systems (no random numbers, no time-based variation), captures snapshots at ticks 100, 500, and 1,000. Then repeats the entire process from scratch. Compares the three snapshot pairs. All six comparisons must return true.

4. **Verify `EntityCommandBuffer::Playback()` ordering.** Write a test that registers systems in different orders on two identical worlds, runs 100 ticks on each, and compares snapshots. If system registration order affects simulation output, that is a determinism bug — fix it by ensuring systems execute in a fixed, registration-order-independent sequence (e.g., sorted by system type ID).

5. **Produce `ECS_DETERMINISM.md`.** Document: what is guaranteed to be deterministic (same systems, same component types, same initial state, same input sequence → identical output), what is not guaranteed (external time sources, `std::rand()`, floating-point variations across platforms — note this as a known limitation), and the testing methodology.

**Technical Notes:**
- Floating-point determinism is a known hard problem in game engines. On the same CPU architecture with the same compiler flags, IEEE 754 operations are deterministic. Across architectures or with different compiler optimization levels (`-O0` vs `-O3`), they may not be. Document this limitation clearly.
- The snapshot must serialize in a deterministic order — `std::unordered_map` iteration order is not deterministic in C++. Use `std::map` (sorted by key) or sort the output of `std::unordered_map` iteration before serializing.
- `EntityID`s must be assigned deterministically. If `CreateEntity()` uses an incrementing counter starting at 0, this is naturally deterministic. If it uses random UUIDs, it is not. Prefer incrementing counters for simulation entities.

**Dependency Gate:**
ECS simulation is bitwise-deterministic across 10 independent runs for the test scenario. System registration order does not affect output.

**Validation Check:**
Run the determinism test 10 times with no other changes to the codebase. All 10 runs produce identical checksums (e.g., CRC32 or SHA256 of the snapshot bytes) at ticks 100, 500, and 1,000.

**Expected Deliverable:** `ECSSnapshot.h/cpp` committed. Determinism test committed and passing 10/10.

---

### Week 28 — Component Reflection System

**Context:**
The ECS Inspector panel (Week 31) and the scene serialization system (Week 33) both need to know about component structure at runtime: what fields a component has, their names, their types, and how to read/write them. Without reflection, each component type requires a manually written inspector and a manually written serializer. With reflection, both are automatic.

**Primary Objective:**
Implement a lightweight runtime component reflection system that enables automatic inspector generation and serialization.

**Linear Tasks:**

1. **Define the reflection data model.** Create `ComponentReflection.h` with: `enum class FieldType { Float, Int, Bool, Vec2, Vec3, Vec4, Quat, AssetHandle, EntityID, String }`, `struct FieldDescriptor { std::string name; FieldType type; size_t byteOffset; size_t byteSize; }`, `struct ComponentDescriptor { std::string name; size_t componentSize; std::vector<FieldDescriptor> fields; }`. Create `ComponentRegistry` singleton (yes, one singleton is acceptable here) that maps `std::type_index → ComponentDescriptor`.

2. **Implement `REFLECT_COMPONENT` macro.** A macro that, at global scope, registers a component's reflection data at startup via a global initializer: `REFLECT_COMPONENT(Transform, FIELD(position, Vec3), FIELD(rotation, Quat), FIELD(scale, Vec3))`. The macro uses the `offsetof()` operator to compute byte offsets for each field at compile time.

3. **Apply `REFLECT_COMPONENT` to core components.** Reflect: `Transform { vec3 position; quat rotation; vec3 scale; }`, `Velocity { vec3 linear; vec3 angular; }`, `MeshRenderer { AssetHandle meshHandle; AssetHandle materialHandle; bool visible; }`, `Camera { float fovDegrees; float nearPlane; float farPlane; }`, `PointLight { vec3 color; float intensity; float radius; }`.

4. **Implement generic field read/write.** `ComponentReflection::ReadField<T>(void* componentPtr, const FieldDescriptor& field) → T` — reads the field value by computing `componentPtr + field.byteOffset` and reinterpret-casting. `WriteField<T>(void* componentPtr, const FieldDescriptor& field, const T& value)` — writes. These are used by the inspector (read + write) and serializer (write on load, read on save).

5. **Write reflection validation tests.** For each reflected component: (a) verify `ComponentDescriptor.componentSize == sizeof(Component)`; (b) for each field, verify `fieldDescriptor.byteOffset == offsetof(Component, fieldName)`; (c) write a test value via `WriteField`, read it back via `ReadField`, verify equality. Run under Address Sanitizer — zero out-of-bounds accesses.

**Technical Notes:**
- `offsetof()` is only defined for standard-layout types. All ECS components should be plain-old-data structs without virtual methods or complex base classes. If a component is not standard-layout, `offsetof()` behavior is undefined — this is a design smell that should be fixed.
- The `ComponentRegistry` global initializer pattern (registering at static init time) is one of the few acceptable uses of global state in the engine, because component reflection data is truly immutable and global — it does not vary by engine instance.
- The `FieldType` enum is intentionally limited to types that the engine uses. Do not try to handle arbitrary C++ types. If a component field is of a type not in the enum, add it to the enum — do not generalize the reflection system to handle arbitrary types.

**Dependency Gate:**
Core components are reflected with correct field names, types, and byte offsets. Field read/write produces correct values. Address Sanitizer: zero violations.

**Validation Check:**
Use `WriteField` to write `vec3(1, 2, 3)` to a Transform's position field, then read it back with `ReadField` — must equal `vec3(1, 2, 3)`. Verify with `offsetof(Transform, position)` that the reflected offset matches the compiler's layout.

**Expected Deliverable:** `ComponentReflection.h/cpp` committed. Reflection applied to all five core components. Validation tests passing.

---

# PHASE 5 — TOOLING & EDITOR FOUNDATION
## Month 8 | Weeks 29–32

---

## MONTH 8 — Tooling & ImGui Integration

### Week 29 — ImGui Backend Integration

**Context:**
Runtime visibility is not a luxury — it is a necessity. Without the ability to inspect the engine's internal state while it is running, debugging becomes a cycle of add-log-rebuild-run-analyze. ImGui provides a zero-overhead (when not rendering), immediate-mode UI that can display any data the engine exposes. This week installs the infrastructure.

**Primary Objective:**
Integrate Dear ImGui with the Vulkan renderer and input system, producing a functioning debug UI layer.

**Linear Tasks:**

1. **Integrate ImGui.** Add `imgui` as a submodule or vendored copy. Add `imgui_impl_vulkan.cpp` and `imgui_impl_sdl2.cpp` (or the appropriate window backend) to the build. Create `ImGuiLayer.h` with `Initialize(RuntimeContext&)`, `BeginFrame()`, `EndFrame()`, `Shutdown()`. Register `ImGuiLayer` as the last subsystem initialized by `EngineRuntime` (it depends on the renderer, window, and input being ready).

2. **Create the ImGui Vulkan render pass.** ImGui renders its own command buffer. Create a dedicated `VkRenderPass` for the UI that: (a) loads the existing color attachment (to draw on top of the scene), (b) does not use depth, (c) has a final layout of `VK_IMAGE_LAYOUT_PRESENT_SRC_KHR`. Submit the UI command buffer after the scene command buffer in the same queue, ensuring the scene finishes before the UI draws on top.

3. **Wire input events.** In the engine's input event processing loop, forward all SDL events (or equivalent) to `ImGui_ImplSDL2_ProcessEvent()` before processing them for gameplay. This gives ImGui first access to mouse clicks and keyboard input — when ImGui has focus (e.g., an input field is active), the engine should not process those events as gameplay input. Check `ImGui::GetIO().WantCaptureKeyboard` and `WantCaptureMouse` before forwarding events to gameplay.

4. **Implement `ImGuiLayer::BeginFrame()` and `EndFrame()`.** `BeginFrame()`: calls `ImGui_ImplVulkan_NewFrame()`, `ImGui_ImplSDL2_NewFrame()`, `ImGui::NewFrame()`. `EndFrame()`: calls `ImGui::Render()`, `ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer)`. These bookend all ImGui draw calls in the main loop.

5. **Render a "Hello, Omnix" test window.** In the first `BeginFrame()` call, add: `ImGui::Begin("Omnix Debug"); ImGui::Text("Engine running. Frame: %d", frameCount); ImGui::End();`. Verify this window appears over the scene, can be dragged, and closes with the X button. Verify Vulkan validation: zero errors with ImGui active.

**Technical Notes:**
- The ImGui render pass must be submitted after the scene render pass in the same frame. The easiest synchronization approach: use a pipeline barrier or semaphore between the scene command buffer and the UI command buffer. Alternatively, record both into the same command buffer with a render pass end/begin sequence.
- ImGui's font atlas is a texture that needs to be uploaded to the GPU during initialization. The `ImGui_ImplVulkan_CreateFontsTexture` call must happen after the Vulkan queue and command pool are set up. Follow the ImGui Vulkan example initialization sequence exactly.
- Do not use the `ImGui::ShowDemoWindow()` in production code — it is useful for learning the API but adds a lot of draw calls and should be removed after the integration test.

**Dependency Gate:**
ImGui renders without Vulkan validation errors. Input is correctly forwarded — ImGui captures mouse clicks when hovered, gameplay does not receive them. `WantCaptureMouse`/`WantCaptureKeyboard` flags work correctly.

**Validation Check:**
Open the test window and drag it across the screen while the scene is rendering at full frame rate. Frame time must not increase more than 10% with the ImGui window visible (ImGui has negligible overhead when not drawing complex widgets).

**Expected Deliverable:** `ImGuiLayer.h/cpp` committed. Test window rendering confirmed.

---

### Week 30 — Debug Console & Log Viewer

**Context:**
The engine now logs to stdout/file, but during development the most important output is the runtime log — what is happening right now, in this frame, while the engine is running. The Debug Console makes this visible without requiring a second terminal window or a log file tail.

**Primary Objective:**
Build a runtime debug console with categorized, filterable, searchable log output.

**Linear Tasks:**

1. **Implement categorized logging backend.** Define `enum class LogCategory { Engine, Renderer, ECS, Assets, Scene, Physics, Audio, Gameplay }` and `enum class LogSeverity { Trace, Info, Warn, Error, Fatal }`. Update all existing `LOG_*` macros to accept a category. Create `LogEntry { LogCategory category; LogSeverity severity; std::string message; uint64_t frameNumber; std::chrono::system_clock::time_point timestamp; }`. Route all log calls to a central `LogSink` that appends to a ring buffer of 4,096 `LogEntry` objects.

2. **Build the ImGui Debug Console panel.** `ImGui::Begin("Debug Console")`. Draw a toolbar: per-category toggle buttons (colored by category), per-severity toggle buttons (Trace/Info/Warn/Error toggles), a text filter input field (`ImGui::InputText` with a search string). Below the toolbar, draw the filtered log list: `ImGui::BeginChild("LogRegion", ...)` with `ImGui::SetScrollHereY(1.0f)` for auto-scroll. Each entry is a single `ImGui::TextColored()` call with severity-appropriate color (gray for info, yellow for warn, red for error).

3. **Add message copy and clipboard support.** Right-click on a log entry → context menu with "Copy message" option. Calls `ImGui::SetClipboardText(entry.message.c_str())`. Add a "Copy All (Filtered)" button at the bottom of the console.

4. **Add frame number and timestamp to each entry.** Display as: `[Frame 1234] [14:32:01.456] [Renderer] [WARN] Swapchain suboptimal`. The frame number is especially useful for correlating log entries with visual artifacts in RenderDoc captures.

5. **Wire all existing log calls into the new categorized system.** Find every `printf`, `std::cout`, `LOG_INFO`, `spdlog::info`, etc. and route them through the new `LogSink`. After this change, no engine log output goes to stdout that does not also appear in the Debug Console.

**Technical Notes:**
- The ring buffer (4,096 entries) will overflow in long sessions. When it overflows, overwrite the oldest entry. This is correct behavior — in practice, the developer looks at recent logs, not logs from 4,096 frames ago.
- Color-coding severity: Trace = `(0.5, 0.5, 0.5, 1.0)` (gray), Info = `(1.0, 1.0, 1.0, 1.0)` (white), Warn = `(1.0, 0.8, 0.0, 1.0)` (yellow), Error = `(1.0, 0.3, 0.3, 1.0)` (red), Fatal = `(1.0, 0.0, 1.0, 1.0)` (magenta).
- The filter field should filter on message content, category, and severity simultaneously. `ImGui::InputText` provides the text input; apply the filter in the log-drawing loop.

**Dependency Gate:**
All engine subsystems route logs through the categorized system. Console displays, filters by category and severity, and supports text search.

**Validation Check:**
Force a renderer warning (e.g., temporarily trigger a swapchain suboptimal condition). It appears in the console under `[Renderer]` at `WARN` severity. Toggle off the `Renderer` category filter — the warning disappears. Toggle it back on — it reappears.

**Expected Deliverable:** `DebugConsole.h/cpp` committed. All log calls updated to use categories.

---

### Week 31 — Runtime Statistics & ECS Inspector

**Context:**
The Debug Console shows log output. The Statistics panel shows quantitative engine health metrics in real time. The ECS Inspector shows the live state of any entity. Together, these three tools (console + stats + inspector) provide comprehensive runtime visibility and make debugging the kind of problem that previously required hours of printf-debugging solvable in minutes.

**Primary Objective:**
Build the runtime statistics panel and the ECS entity/component inspector panel.

**Linear Tasks:**

1. **Implement the Runtime Statistics panel.** `ImGui::Begin("Runtime Statistics")`. Display a rolling 60-frame FPS graph using `ImGui::PlotLines`. Below it, display metrics as label-value pairs: `FPS: 144.0`, `Frame Time: 6.94ms`, `CPU Time: 2.10ms`, `GPU Draw Calls: 87`, `Triangles: 1,240,320`, `GPU Memory: 512 MB / 8 GB`, `Active Entities: 15,432`, `Pending Destructions: 0`. Read all values from `GpuMemoryTracker::GetStats()`, `ECSWorld::GetEntityCount()`, and the CPU profiler (Week 32).

2. **Implement the Entity List panel.** `ImGui::Begin("ECS Inspector — Entities")`. Draw a filterable list of all live entities: `ImGui::InputText("Search", ...)` filters by entity name (if a `NameComponent` is present) or by entity ID. Each row: entity ID, entity name (or "Entity {id}"), number of attached components. Click to select — sets `m_SelectedEntity`.

3. **Implement the Component Inspector panel.** `ImGui::Begin("ECS Inspector — Components")`. For `m_SelectedEntity`, iterate all attached component types via the component reflection system (Week 28). For each component: draw a collapsible header with the component name. Inside: for each field in `ComponentDescriptor.fields`, draw an appropriate ImGui widget based on `FieldType`: `ImGui::DragFloat3` for Vec3, `ImGui::DragFloat` for Float, `ImGui::Checkbox` for Bool, `ImGui::Text` for read-only fields. On widget change, call `WriteField()` to apply the edit to the live component.

4. **Wire live field editing to the ECS.** Field edits via the inspector should take effect immediately (next frame), not through the `EntityCommandBuffer` (since this is a value modification, not a structural change). Call `WriteField()` directly on the component pointer — this is the one case where writing directly to component data is acceptable (it is the inspector's explicit purpose).

5. **Add `NameComponent` to the ECS.** Create `struct NameComponent { char name[64]; }`. Apply `REFLECT_COMPONENT`. In the sandbox world (Week 35), give all entities meaningful names (e.g., "DirectionalLight", "PlayerMesh_01"). The inspector's search field filters by this name.

**Technical Notes:**
- `ImGui::DragFloat3` (for Vec3 position/scale) is more ergonomic than three separate `DragFloat` calls. It draws as a single row with three draggable values.
- For `AssetHandle` fields, display as `AssetHandle: 0x{handle:016X} ({assetName})`. Look up the asset name from the registry. This is much more useful than just the raw hex handle.
- The inspector's live editing should not bypass validation — if the `Transform.scale` field is edited to zero (which would cause a degenerate transform), log a `[ECS] WARN` rather than silently applying an invalid value.

**Dependency Gate:**
ECS inspector displays all reflected components for any selected entity. Field edits take effect immediately. `NameComponent` allows entities to be found by name.

**Validation Check:**
Select a moving entity. Change its velocity component's `linear` field to `(0, 0, 0)` via the inspector. The entity stops moving on the next frame. Change it to `(0, 5, 0)`. The entity moves upward.

**Expected Deliverable:** Statistics panel, entity list panel, and component inspector panel committed and functional.

---

### Week 32 — Profiler Integration & RenderDoc Support

**Context:**
The statistics panel shows what is happening. The profiler shows why it is slow. RenderDoc shows what the GPU is drawing. Together, these are the two most powerful performance debugging tools available to a Vulkan engine developer. This week installs both.

**Primary Objective:**
Add CPU frame profiling and enable RenderDoc frame capture integration.

**Linear Tasks:**

1. **Implement `PROFILE_SCOPE(name)` macro.** Create a lightweight CPU profiler using `std::chrono::high_resolution_clock`. `PROFILE_SCOPE(name)` creates a `ProfileScope` RAII object on the stack: constructor records `startTime = now()`, destructor records `endTime = now()` and pushes `ProfileEntry { name, durationMicros }` to a per-frame result buffer. Implement a `Profiler::BeginFrame()` that clears the buffer, and `Profiler::GetLastFrameResults() → std::vector<ProfileEntry>`.

2. **Apply `PROFILE_SCOPE` to major engine code paths.** Add profiling to: `ECSWorld::ExecuteSystems()` (and each individual system), `Renderer::RenderFrame()`, `AssetManager::ProcessReloadQueue()`, `SceneManager::Update()`, `Input::Update()`. This produces a flame-graph-style view of where each frame's CPU time is spent.

3. **Build an ImGui profiler panel.** `ImGui::Begin("CPU Profiler")`. For each `ProfileEntry` in the last frame, draw a single row: `{name}: {durationMicros} µs`. Sort by duration descending. Highlight entries over 5ms in yellow, over 16ms in red (they are causing frame time budget overruns). Add a frame time budget bar: a horizontal bar showing each profiled scope as a proportional segment.

4. **Add Vulkan debug labels.** Add `vkCmdBeginDebugUtilsLabelEXT(cmd, &labelInfo)` / `vkCmdEndDebugUtilsLabelEXT(cmd)` around every major render pass and significant batch of draw calls. Label names should match the profiler scope names: `"Scene Pass"`, `"Shadow Pass"`, `"UI Pass"`. This makes RenderDoc captures dramatically more readable.

5. **Integrate RenderDoc in-application API.** Load `renderdoc.h` and the RenderDoc API at engine startup (if RenderDoc is installed and its DLL/SO is available). Implement `Renderer::TriggerCapture()` that calls `rdoc_api->TriggerCapture()`. Bind this to `F11`. Log `[Renderer] RenderDoc capture triggered` when the hotkey is pressed. After the capture, RenderDoc automatically opens the frame for inspection.

**Technical Notes:**
- The `PROFILE_SCOPE` implementation must have near-zero overhead (< 100ns per scope). `std::chrono::high_resolution_clock::now()` on modern hardware takes ~10-20ns. Two calls per scope plus one vector push is well within budget.
- The RenderDoc integration should degrade gracefully if RenderDoc is not installed (the DLL is not found). Do not error-out the engine startup if RenderDoc is absent — just disable the `TriggerCapture()` functionality and log `[Renderer] INFO: RenderDoc not installed — capture unavailable`.
- Vulkan debug labels are zero-cost in non-debug drivers (the validation layer strips them). In debug mode (with validation layers), they are visible in both the Vulkan validation output and in RenderDoc captures. Always add them.

**Dependency Gate:**
CPU profiler captures scope timings correctly across a full frame. RenderDoc captures a valid frame when F11 is pressed. All render passes are labeled in the captured frame.

**Validation Check:**
Press F11. Open the capture in RenderDoc. Verify: all passes are labeled with human-readable names. All draw calls within each pass are labeled with the mesh/material name. Zero unlabeled passes or draw calls.

**Expected Deliverable:** `Profiler.h/cpp` committed. ImGui profiler panel committed. Vulkan debug labels added. RenderDoc integration committed.

---

# PHASE 6 — SCENE RUNTIME
## Month 9 | Weeks 33–36

---

## MONTH 9 — Scene Runtime, Sandbox World & Final Sign-Off

### Week 33 — Scene File Format & ECS Serialization

**Context:**
Everything built so far — EngineRuntime, asset pipeline, renderer, ECS, tooling — needs to persist. A game engine that cannot save and load a scene is a toy. The `.omnixscene` format is the serialized form of an entire runtime world state. This week implements the file format and the serialization/deserialization pipeline.

**Primary Objective:**
Define `.omnixscene` binary format and implement full ECS state serialization and deserialization.

**Linear Tasks:**

1. **Define the `.omnixscene` binary format.** Format layout: (a) 16-byte file header: `{ magic: "OMNIXSCN", version: uint32, entityCount: uint32, checksum: uint32 }`; (b) Entity Table: array of `uint32 entityIDs[entityCount]`; (c) Component Blocks: for each reflected component type (in deterministic type-ID order): `{ componentTypeID: uint32, entityCount: uint32, entityIDs: uint32[], componentData: raw-bytes }` — the `componentData` field is an array of `entity_count * sizeof(Component)` bytes, one component per entity in the entityIDs list; (d) Asset Reference Table: array of `{ assetHandle: uint64, sourceUUID: uint64 }` — maps runtime handles to stable UUIDs for cross-session resolution.

2. **Implement `SceneSerializer::Save(const ECSWorld& world, const std::filesystem::path& outputPath)`.** Iterates all entities (in deterministic `EntityID` order). For each component type registered in `ComponentRegistry`, collects all entities that have that component. Writes the binary layout defined above. Computes CRC32 checksum of the entire payload (after the header) and stores it in `header.checksum`. Uses `fwrite` for performance (no stream overhead on large scenes).

3. **Implement `SceneSerializer::Load(const std::filesystem::path& scenePath, ECSWorld& world)`.** Reads and validates the header (magic number check, version check). Recomputes CRC32 of the payload and compares to `header.checksum` — if they differ, log `[Scene] ERROR: Scene file checksum mismatch — file may be corrupted` and return false. Reads entity table and creates entities with specified IDs (preserving IDs for referential integrity). Reads component blocks and calls `ECSWorld::AddComponent()` (via `EntityCommandBuffer`) for each entity-component pair. Resolves asset reference table: for each `{ handle, sourceUUID }`, look up the UUID in the `AssetRegistry` and get the current session's handle — asset handles may differ between sessions but UUIDs are stable.

4. **Validate round-trip correctness.** Create a test world with 500 entities and five component types. Save to a temp file. Create a fresh `ECSWorld`. Load from the file. Capture `ECSSnapshot` of both worlds. Compare checksums — must be identical.

5. **Test corruption detection.** Take a valid `.omnixscene` file. Flip the last byte (corrupt the payload). Load it. Verify: (a) checksum mismatch is detected; (b) `Load()` returns false; (c) the world is not partially modified (either fully loaded or left unchanged — partial loads are dangerous).

**Technical Notes:**
- The asset reference table is critical. Without it, a material handle `0xABCD1234` saved in a scene would be meaningless in the next engine session if the handle changed (e.g., after a clean rebuild that regenerated UUIDs). The `sourceUUID` (the deterministic hash of the source path, from Week 9) is stable across sessions — it is the stable foreign key.
- The `entityCount` in the header must match the number of entries in the Entity Table. The loader should validate this before attempting to read entity data.
- For large scenes (100K+ entities), `fwrite` and `fread` are fast enough. For scenes with streaming requirements, a streaming format would be needed — that is a Phase 6+ concern.

**Dependency Gate:**
Scene round-trip is byte-for-byte identical: `ECSSnapshot` checksums match before save and after load. Checksum mismatch detection works correctly on corrupted files.

**Validation Check:**
Corrupt a `.omnixscene` file (flip any byte). Load it — must produce `[Scene] ERROR: checksum mismatch` and return false with zero world state changes. Un-corrupt the file. Load it — must succeed with snapshot checksums matching.

**Expected Deliverable:** `SceneSerializer.h/cpp` committed. Round-trip test committed and passing. Corruption detection verified.

---

### Week 34 — Scene Loading, Prefab Instancing & Transform Hierarchy

**Context:**
Scene serialization handles persistence. This week handles runtime: loading a scene from a handle, transitioning between scenes, instancing prefabs, and propagating the parent-child transform hierarchy. After this week, the engine can load an interactive world and maintain spatial relationships between entities.

**Primary Objective:**
Implement runtime scene loading, safe scene unloading, prefab instancing, and transform hierarchy propagation.

**Linear Tasks:**

1. **Implement `SceneManager::LoadScene(AssetHandle sceneHandle)`.** Sequence: (a) call `UnloadScene()` to clear the current world if one is loaded; (b) resolve `sceneHandle` via `AssetManager` to get the `.omnixscene` file path; (c) call `SceneSerializer::Load(path, m_World)` to populate the world; (d) call `AssetManager::LoadAll()` to ensure all assets referenced by the scene are GPU-resident; (e) call `m_RuntimeContext.ecs->InitializeGameplaySystems()` to trigger any system initialization callbacks (e.g., physics body creation). Emit `LoadSceneComplete` event on the `EventBus`.

2. **Implement `SceneManager::UnloadScene()`.** Sequence: (a) broadcast `BeforeSceneUnload` event to give systems a chance to clean up; (b) iterate all entities with GPU-referenced components (e.g., `MeshRenderer`) and schedule their resources for deferred destruction; (c) call `ECSWorld::Clear()` to destroy all entities and components; (d) call `AssetManager::ReleaseAll()` to decrement reference counts for all scene assets (assets shared with the next scene will stay loaded). After this call, the world must have zero entities and the GPU tracker must show a flat allocation total.

3. **Implement `PrefabAsset` and `Instantiate`.** A prefab is a saved entity graph: a small `.omnixscene` file containing a single entity tree (root entity + children). `PrefabAsset` stores the deserialized entity data in memory. `SceneManager::Instantiate(AssetHandle prefabHandle) → EntityID` creates all entities from the prefab in the current world, generates fresh `EntityID`s (the saved IDs are relative, not absolute), and returns the root entity ID. Supports nested prefabs (a prefab that references another prefab by handle).

4. **Implement transform hierarchy.** Add `ParentComponent { EntityID parentID; }` and `ChildrenComponent { std::vector<EntityID> childIDs; }` to the ECS (both reflected). Implement `HierarchySystem` that runs every frame: for all entities with `ParentComponent`, compute world matrix by multiplying parent's world matrix × own local matrix. Use a topological traversal starting from root entities (those without `ParentComponent`). Cache world matrices in a `WorldTransformComponent` so the renderer reads world transforms, not local transforms.

5. **Write hierarchy integration test.** Create a parent entity with three child entities. Move the parent. Verify after `HierarchySystem::Execute()` that all three children's `WorldTransformComponent` world matrices reflect the parent's new position. Move a child independently — verify only that child's world matrix changes. Unparent a child — verify it retains its last world matrix as a new independent local matrix.

**Technical Notes:**
- Scene transition (unload old, load new) must handle the case where both scenes share some assets. The reference-counting system in `AssetManager` ensures shared assets stay GPU-resident during the transition — they are not unloaded by `UnloadScene()` if their reference count is still > 0 (because the new scene has already incremented it before the old scene decrements).
- The `HierarchySystem` topological traversal must not have infinite loops (circular parent relationships). Add an assertion: the traversal depth must not exceed 64 levels — any depth greater than this almost certainly indicates a circular reference bug.
- Prefab instancing must generate fresh `EntityID`s to avoid collisions with existing entities. The `SceneSerializer::Load()` path also generates fresh IDs for prefab entities using an ID offset rather than the stored IDs.

**Dependency Gate:**
Scene loads and unloads without leaked entities or GPU resources. Prefab instancing creates correct entity hierarchies. Transform hierarchy propagates correctly after parent moves.

**Validation Check:**
Load scene A. Unload it. Load scene B. After the transition: (a) entity count matches scene B's entity count (zero scene A entities remain); (b) GPU memory tracker shows zero net change from before scene A was loaded (all resources from A are freed, all from B are loaded); (c) a parent entity in scene B moved via the inspector causes all children's world transforms to update.

**Expected Deliverable:** `SceneManager::LoadScene/UnloadScene` committed. `PrefabAsset` committed. `HierarchySystem` committed.

---

### Week 35 — Sandbox World Construction & Full Integration Test

**Context:**
All six phases are now implemented. The sandbox world is the proof: a real, authored, interactive scene that exercises every system simultaneously. It is not a tech demo designed to impress — it is an integration test designed to find the edge cases and interaction bugs that only emerge when everything runs together.

**Primary Objective:**
Build the Final v0.2 Sandbox World and run a 10-minute continuous stability test.

**Linear Tasks:**

1. **Author the sandbox scene.** Hand-author `Assets/Scenes/sandbox.omnixscene` (or build it procedurally in a setup function, then save it). Required content: (a) a directional light entity (`DirectionalLight`, `Transform`); (b) a camera entity (`Camera`, `Transform`); (c) at least 3 distinct mesh entities with different materials (`MeshRenderer`, `Transform`, `NameComponent`); (d) 50+ dynamic simulation entities with `Transform`, `Velocity`, and `MeshRenderer` — these will be animated by `VelocitySystem` and `RotationSystem`; (e) 5 parent-child entity groups (at least 2 levels deep) to exercise the hierarchy.

2. **Implement `VelocitySystem` and `RotationSystem`.** `VelocitySystem`: for all entities with `Transform` + `Velocity`, apply `transform.position += velocity.linear * deltaTime`. Wrap positions within a sphere of radius 50 units (boundary). `RotationSystem`: for all entities with `Transform` + `Velocity`, apply a small rotation each frame (`transform.rotation = glm::normalize(transform.rotation * glm::angleAxis(0.01f, glm::vec3(0,1,0)))`). These two systems together produce continuous visible motion.

3. **Implement ImGui scene file menu.** Add an `ImGui::BeginMainMenuBar()` with `File → Load Scene` (opens a file dialog or text input for the scene path), `File → Save Scene` (saves current world to the loaded scene path), `File → Reload Scene` (unload and reload the current scene). Wire these to `SceneManager::LoadScene`, `SceneSerializer::Save`, and the combination.

4. **Run the 10-minute stability test.** Boot the engine, load `sandbox.omnixscene`. Let it run for 10 continuous minutes at full frame rate with: window resize events every 60 seconds, hot-reload of the test texture every 2 minutes (automated), save-and-reload of the scene every 5 minutes. Monitor: GPU memory tracker for leaks (total bytes must remain flat ± 5%), ECS entity count (must remain constant after each save-reload cycle), Vulkan validation log (zero errors throughout), and CPU profiler (frame time must not increase monotonically — no memory-growth-driven slowdown).

5. **Fix all issues discovered during the stability test.** The sandbox test will expose real bugs. The last day of Week 35 is reserved for fixing the bugs found. If a bug cannot be fixed in one day, log it as a known issue with a clear description and a proposed fix — do not let it block the Week 36 sign-off unless it violates a Final Validation Criterion.

**Technical Notes:**
- The 50+ dynamic entities should stress-test the ECS iteration at a real-world scale. If `VelocitySystem` + `RotationSystem` on 50 entities costs more than 0.5ms, the ECS layout has a problem — investigate and fix before the sign-off.
- The wrap-around boundary for simulation entities prevents them from flying off to infinity, which would eventually cause floating-point precision issues in the transform hierarchy.
- The save-and-reload test (every 5 minutes) is the most critical integration test: it exercises the entire stack — ECS → Serializer → File I/O → Asset Reference Table → Registry → Loader → GPU Upload — in a single operation.

**Dependency Gate:**
Sandbox world loads, simulates, renders, and stays stable for 10 continuous minutes under the described stress conditions. GPU memory is flat. No validation errors.

**Validation Check:**
Run `GpuMemoryTracker::GetStats().totalAllocatedBytes` at minute 0 and minute 10. Difference must be < 5%. ECS entity count at minute 0 and minute 10 must be identical. Vulkan validation log entry count at minute 10 must be 0 (new errors since start).

**Expected Deliverable:** `sandbox.omnixscene` committed. `VelocitySystem`, `RotationSystem` committed. 10-minute stability test result archived.

---

### Week 36 — Final Validation, Hardening & v0.2 Sign-Off

**Context:**
This is the culmination of 36 weeks of work. The final validation week does not add features. It proves, through rigorous testing, that every validation criterion from the Omnix v0.2 task breakdown is met. It produces the final documentation. And it officially signs off the Runtime Genesis milestone.

**Primary Objective:**
Execute the complete v0.2 Final Validation Criteria checklist and deliver the Runtime Genesis milestone.

**Linear Tasks:**

1. **Boot reliability test.** Cold-start the engine (clean engine cache, first run behavior) 20 times back to back. Each run must: complete `EngineRuntime::Initialize()` successfully, load `sandbox.omnixscene`, run for at least 30 seconds, shut down cleanly. Pass criteria: 20/20 successful starts. Failure of any run is a blocking bug that must be fixed before sign-off. Log the boot time (from process start to first rendered frame) for all 20 runs — establish the baseline boot time for future regression tracking.

2. **Architecture audit.** Re-execute the Week 1–2 audit procedures against the final codebase. Regenerate the dependency graph. Verify: (a) zero circular dependencies; (b) zero undocumented globals (re-run the global state grep — all hits are either documented in `GLOBAL_STATE_AUDIT.md` as `KEEP_GLOBAL` or were eliminated); (c) every subsystem has an entry in `SUBSYSTEMS.md` with a current owner; (d) every cross-subsystem communication follows one of the four patterns from `COMMUNICATION_STANDARDS.md`.

3. **Determinism validation.** Run the ECS determinism test from Week 27, but this time against the full sandbox scene (not the synthetic test world). Capture `ECSSnapshot` at frames 100, 500, and 1,000 across 5 independent runs. All 5 runs must produce identical checksums at all three frames.

4. **Scene serialization full validation.** In the running sandbox: (a) record `ECSSnapshot` checksum at frame 100; (b) save scene; (c) cold restart engine; (d) load scene; (e) run to frame 100; (f) record new `ECSSnapshot` checksum. The two checksums must match (deterministic save/load produces identical simulation state).

5. **Final documentation pass.** Finalize and commit: (a) `SUBSYSTEMS.md` — final state with all subsystems, owners, and public API surfaces; (b) `RUNTIME_LIFECYCLE.md` — final startup/shutdown sequence; (c) `ECS_DETERMINISM.md` — guarantees and limitations; (d) `GPU_RESOURCE_OWNERSHIP.md` — final resource ownership table; (e) `v0.2_ARCHITECTURE.md` — a top-level 2-page architecture summary with the dependency graph, the communication patterns, the phase integration order, and the `EngineRuntime` lifecycle diagram. This is the document a new engineer reads on day 1.

**Dependency Gate:**
ALL eight Final Validation Criteria return PASS. No exceptions.

**Final Validation Criteria:**

| # | Criterion | Pass Condition |
|---|-----------|----------------|
| 1 | Engine boots reliably | 20/20 cold starts succeed without crash or validation error |
| 2 | Runtime architecture stable | Dependency graph is a DAG, zero circular deps, all subsystems owned |
| 3 | Assets load deterministically | Same AssetHandle resolves to identical data across 10 fresh engine restarts |
| 4 | Renderer stable under stress | 10-min sandbox run with resize/reload stress — zero Vulkan validation errors |
| 5 | ECS execution deterministic | Snapshot checksums match across 5 independent runs at frames 100, 500, 1000 |
| 6 | Scene serialization functional | Save → cold restart → load → snapshot checksums match |
| 7 | Runtime debugging tools operational | Console, stats panel, ECS inspector, CPU profiler, and RenderDoc all function in the sandbox |
| 8 | Engine architecture scalable | A new subsystem can be added to EngineRuntime without modifying any existing subsystem |

**Expected Deliverable:**
- All five documentation files finalized and committed
- v0.2 tag created in the git repository: `git tag -a v0.2.0 -m "Runtime Genesis — v0.2 milestone complete"`
- 10-minute stability run result and all 8 Final Validation Criteria results archived in `docs/v0.2_VALIDATION_RESULTS.md`

---

## Appendix A — Phase Integration Rationale

| Phase | Months | Dependency Justification |
|-------|--------|--------------------------|
| Phase 1 — Architecture | 1–2 | Foundation for all phases. EngineRuntime must own subsystems before any new system is added. |
| Phase 4 — Asset Pipeline | 3–4 | Precedes renderer because the material system (Phase 3) depends on AssetHandle and AssetManager being functional. |
| Phase 3 — Renderer | 5–6 | Follows assets because GPU resource upload (Week 15) requires AssetManager. Material system requires handle resolution. |
| Phase 2 — ECS | 7 | Follows renderer because ECS determinism testing (Week 27) requires a stable runtime loop. |
| Phase 5 — Tooling | 8 | Follows ECS because the inspector depends on component reflection (Week 28). Follows renderer because ImGui needs a stable Vulkan backend. |
| Phase 6 — Scene | 9 | Follows tooling because scene serialization depends on component reflection. Follows everything because it integrates all systems. |

---

## Appendix B — Full Dependency Chain

```
Week 1  — Subsystem Inventory
Week 2  — Dependency Graph
Week 3  — Global State Audit
Week 4  — Circular Dep Elimination
          ↓
Week 5  — EngineRuntime Core
Week 6  — RuntimeContext & DI
Week 7  — Deterministic Lifecycle
Week 8  — Communication Standards
          ↓
Week 9  — AssetHandle Type
Week 10 — AssetRegistry
Week 11 — Loader Interface
Week 12 — Directory & Metadata
          ↓
Week 13 — Texture Importer
Week 14 — Mesh Importer
Week 15 — Runtime GPU Loading
Week 16 — Hot Reloading
          ↓
Week 17 — GPU Resource Audit
Week 18 — Deferred Destruction Queue
Week 19 — Descriptor Allocator
Week 20 — GPU Memory Tracker
          ↓
Week 21 — Material Format
Week 22 — Material Runtime + PipelineCache
Week 23 — Shader Reflection
Week 24 — Renderer Hardening
          ↓
Week 25 — ECS Benchmarks
Week 26 — EntityCommandBuffer
Week 27 — ECS Determinism
Week 28 — Component Reflection
          ↓
Week 29 — ImGui Backend
Week 30 — Debug Console
Week 31 — Stats + ECS Inspector
Week 32 — Profiler + RenderDoc
          ↓
Week 33 — Scene Serialization
Week 34 — Scene Loading + Prefabs
Week 35 — Sandbox World
Week 36 — Final Sign-Off ✓
```

---

## Appendix C — Key Documents Produced

| Document | Created | Purpose |
|----------|---------|---------|
| `SUBSYSTEMS.md` | Week 1 | Authoritative subsystem inventory |
| `ARCHITECTURE.md` | Week 2 | Dependency graph diagrams |
| `GLOBAL_STATE_AUDIT.md` | Week 3 | Global state inventory |
| `COMMUNICATION_STANDARDS.md` | Week 8 | Cross-subsystem communication rules |
| `CODING_STANDARDS.md` | Week 9 | Coding rules (no raw paths, etc.) |
| `RUNTIME_LIFECYCLE.md` | Week 7 | Startup/shutdown sequence |
| `GPU_RESOURCE_OWNERSHIP.md` | Week 17 | GPU resource ownership table |
| `ECS_BENCHMARK_BASELINE.md` | Week 25 | ECS performance baseline |
| `ECS_DETERMINISM.md` | Week 27 | ECS determinism guarantees |
| `v0.2_ARCHITECTURE.md` | Week 36 | Final architecture summary |
| `v0.2_VALIDATION_RESULTS.md` | Week 36 | Final sign-off evidence |

---

*Omnix Studio Engine v0.2 — Runtime Genesis | Detailed 36-Week Roadmap*
*"Architectural Stability Before Expansion" — the hardest threshold in engine development is Architectural Coherence. This document is the map to crossing it.*
