# Deterministic Startup Order

This document defines the authoritative, deterministic boot sequence of **Omnix Studio Engine**. Subsystems must initialize in this sequence to guarantee no NullPointer dereferences or Static Initialization order failures.

---

## 🏛️ Startup Order Timeline

| Order | Subsystem | Purpose | Dependencies | Fail-Fast Policy |
| :--- | :--- | :--- | :--- | :--- |
| 1 | **Logging** | Setup trace file outputs. | None | **CRITICAL** (crash on failure) |
| 2 | **Timer** | Initialize high-performance clock counters. | None | **CRITICAL** (crash on failure) |
| 3 | **Input** | Setup mappings and spin up listener threads. | None | **HIGH** (graceful exit) |
| 4 | **ECS (World)** | Allocate Sparce component pools and sign up Coordinator. | None (pure simulation) | **CRITICAL** (crash on failure) |
| 5 | **Scheduler** | Instantiate Thread pools and Task Queues. | None | **HIGH** (graceful exit) |
| 6 | **RHI / Window** | Open OS Windows and acquire Vulkan handles. | None (Windows / OS calls) | **CRITICAL** (crash on failure) |
| 7 | **Renderer** | Compile shader states, allocate pipeline layouts, and configure render passes. | RHI / Window | **CRITICAL** (crash on failure) |
| 8 | **Assets** | Connect asset cache handlers and pre-compile tables. | RHI Device | **HIGH** (graceful exit) |
| 9 | **Scene** | Construct hierarchical tree nodes and set camera frustums. | ECS | **HIGH** (graceful exit) |

---

## 🔄 Fail-Fast Execution Pattern

The startup orchestrator in `EngineRuntime::Initialize()` handles failures strictly:

1. Subsystem initialization calls are chained using logical assertions.
2. If any subsystem returns a failing result:
   * **Stop immediately**. Do not attempt to initialize subsequent layers.
   * Call `EngineRuntime::Shutdown()` to cleanly tear down the subsystems that successfully initialized prior to the failure.
   * Log the exact name of the subsystem that failed to boot.
