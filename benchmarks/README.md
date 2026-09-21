# Omnix Engine Benchmarks

This directory provides the standalone, reproducible benchmark harness and empirical performance measurements for Omnix Engine.

## Directory Structure

```text
benchmarks/
├── README.md                 # Benchmark suite overview and execution instructions
├── bench_main.cpp            # Standalone C++ benchmark executable source
└── benchmark_results.csv     # Raw empirical timing measurements (microseconds, percentiles, throughput)
```

## Workload Coverage

The benchmark harness tests core engine subsystems under isolated and realistic load conditions:

1. **ECS Entity Lifecycle**:
   - Entity allocation throughput (10,000 and 50,000 entity batches via `Coordinator`).
   - Component attachment throughput (`TransformComponent` and `RigidBodyComponent` via dense pools).
   - Linear simulation system iteration (sequential position integration over 10,000 active entities).
2. **Spatial & Matrix Math**:
   - 4x4 TRS matrix construction (`Matrix4x4::TRS` over 10,000 position, rotation, scale inputs).
   - Deep transform hierarchy matrix multiplication (100-level parent-child dependency chain).
3. **State Serialization**:
   - Binary ECS snapshot serialization (`NormalSerializer` over 2,000 entities with schema reflection).
   - Binary ECS snapshot deserialization (`NormalDeserializer` restoring entity and component state).
4. **Memory Allocators**:
   - Linear allocator arena throughput (1,000 sequential 64-byte allocations).
   - Pool allocator fixed-size block throughput (1,000 allocate/free cycles of 32-byte blocks).
   - Stack allocator scoped marker throughput (500 push/pop cycles of 128-byte frames).
5. **Asset Packaging & Lookup**:
   - Package manager archive mounting and asset handle lookup (`PackageManager` reverse-stack search across 100 mounted entries).

## Building and Running

### Build

```powershell
# From repository root in MSVC Developer Command Prompt:
cmake --build build_ninja --target omnix_benchmarks --config Release
```

### Run

```powershell
.\build_ninja\omnix_benchmarks.exe
```

The executable outputs formatted console tables and automatically refreshes `benchmarks/benchmark_results.csv`.

For detailed methodology, test environment specifications, and architectural analysis of these results, see [`docs/benchmarks.md`](../docs/benchmarks.md).
