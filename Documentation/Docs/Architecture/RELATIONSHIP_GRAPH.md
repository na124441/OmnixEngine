# Subsystem Relationship Graph

This document provides a visual mapping of the engine architecture, detailing subsystem boundaries, dependencies, communication methods, and system health status.

---

## 📊 Subsystem Relationship Diagram

The diagram below shows how control and data flow through the engine. 

* **Solid Arrows ($\rightarrow$)** denote compile-time `#include` dependencies.
* **Dashed Arrows ($-\rightarrow$)** denote runtime pointers, references, or message-passing.
* Nodes are color-coded by their stability and maturity status:
  * <span style="color:#22c55e">**Green (Stable)**</span>: Mature implementations, clean APIs, no known structural issues.
  * <span style="color:#eab308">**Yellow (Fragile / Partial)**</span>: Working systems containing architectural debt or mixed boundaries.
  * <span style="color:#ef4444">**Red (Prototype / Critical Risk)**</span>: Temporary mock implementations, skeletal headers, or severe structural mismatches.

```mermaid
graph TD
    %% Define Node Colors and Styles
    classDef stable fill:#dcfce7,stroke:#22c55e,stroke-width:2px,color:#14532d;
    classDef fragile fill:#fef9c3,stroke:#eab308,stroke-width:2px,color:#713f12;
    classDef dangerous fill:#fee2e2,stroke:#ef4444,stroke-width:2px,color:#7f1d1d;

    %% Subsystems
    App["Application Core<br/>(Application.cpp / main.cpp)"]:::stable
    Renderer["Renderer<br/>(Vulkan Device / Pipelines)"]:::fragile
    ECS["ECS Coordinator<br/>(Coordinator / Array Pools)"]:::fragile
    Scheduler["Scheduler<br/>(FrameScheduler / FrameContext)"]:::dangerous
    Scene["Scene System<br/>(SceneManager / Transforms)"]:::fragile
    Assets["Asset Manager<br/>(AssetCache dummy)"]:::dangerous
    Input["Input System<br/>(InputManager / Key Bindings)"]:::stable
    Physics["Physics System<br/>(ECS PhysicsSystem Update)"]:::fragile
    Serializer["Serializer<br/>(ISerializer / ECS Snapshots)"]:::stable
    Window["Window System<br/>(SDL3 / Platform Windows)"]:::stable
    EventBus["Event Bus<br/>(EventManager pub/sub)"]:::stable
    Logger["Logger<br/>(Core log utilities)"]:::stable
    Timer["Timer<br/>(Core high-res timer)"]:::stable

    %% Dependency & Communication Links
    App -->|Creates| Window
    App -->|Inits| Logger
    App -->|Uses| Timer
    App -->|Ticks| Input
    App -->|Ticks| ECS
    App -->|Sets World| Renderer
    App -.->|Detaches| InputThread["CLI Input Thread"]:::dangerous

    Window -->|Provides Surface| Renderer
    
    Input -.->|Publishes events| EventBus
    
    ECS -->|Refers Types| Serializer
    ECS -.->|Updates| Physics
    ECS -.->|Updates| Scene
    
    Physics -.->|Iterates Components| ECS
    
    Scene -->|Uses| ECS
    Scene -.->|Requests shapes| Assets
    
    Renderer -->|Draws scene| Scene
    Renderer -->|Queries mesh/textures| Assets
    
    Scheduler -.->|Ticked by| App
    Scheduler -.->|Orchestrates| Renderer
    Scheduler -.->|Orchestrates| ECS
    
    Serializer -.->|Queries duplicate| DupECS["Duplicate ECS<br/>(Serializer/ECS/ECS.h)"]:::dangerous
    DupECS -.->|Requires schemas| Registry["Schema Registry"]:::stable

    %% Apply Style Groups
    style App font-weight:bold;
    style Renderer font-weight:bold;
    style ECS font-weight:bold;
    style Scheduler font-weight:bold;
    style Assets font-weight:bold;
```

---

## 📈 Status Summary

* **Logger, Timer, Event Bus, Window System, Input System** are healthy and ready for v0.2 platform bindings.
* **Renderer & Scene Systems** work but suffer from cross-boundary leakages (e.g. Renderer directly polling transforms while Scene node hierarchies duplicate Transform states).
* **Asset Manager & Scheduler** are critical blockers. The Asset Manager has no database backend (acts as a stub), and the Scheduler has zero scheduling implementation, resulting in hardcoded sequential game loop execution.
* **Serializer** works correctly in isolation but is completely decoupled from the active gameplay ECS, depending instead on duplicate class declarations.
