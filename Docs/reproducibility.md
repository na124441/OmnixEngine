# Omnix Engine Build & Verification Guide

This guide provides concrete, copy-pasteable instructions for cloning, configuring dependencies, compiling, and empirically verifying **Omnix Engine v0.4** on Windows x64.

---

## 1. Prerequisites & Host Requirements

Before building, ensure the following software tools are installed on your host system:

* **Host OS**: Microsoft Windows 10/11 (64-bit)
* **Compiler**: Microsoft Visual Studio 2022 / 2026 with C++ Desktop Development workload (MSVC `v143` or later, C++17 compliant)
* **Build Generator**: [CMake 3.25+](https://cmake.org/download/) and [Ninja 1.11+](https://ninja-build.org/)
* **Vulkan SDK**: [LunarG Vulkan SDK 1.3+](https://vulkan.lunarg.com/) with `glslc` on system `PATH`
* **Package Manager**: [vcpkg](https://github.com/microsoft/vcpkg) installed and bootstrapped

---

## 2. Dependency Resolution via vcpkg

Omnix Engine relies on external dependencies managed through `vcpkg`. Install the required libraries:

```powershell
# In your vcpkg directory (e.g. C:\vcpkg)
.\vcpkg.exe install glfw3:x64-windows
.\vcpkg.exe install glm:x64-windows
.\vcpkg.exe install unofficial-omniverse-physx-sdk:x64-windows
```

Ensure the environment variable `VCPKG_ROOT` points to your vcpkg installation:
```powershell
$env:VCPKG_ROOT = "C:\vcpkg" # adjust to your path
```

---

## 3. Project Configuration & Compilation

Open the **x64 Native Tools Command Prompt for VS** (or load the MSVC environment in PowerShell) and run:

```powershell
# 1. Clone or navigate to the Omnix Engine root
cd d:\OmnixEngine

# 2. Create the build directory
mkdir build_ninja
cd build_ninja

# 3. Configure the project with CMake and Ninja
cmake -G "Ninja" `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake" `
  ..

# 4. Compile the entire solution (Runtime, Tests, Benchmarks)
ninja -j 8
```

### Build Artifacts Generated
Upon completion, the following binary targets are produced in `build_ninja/`:
* `Application.exe` — Main interactive engine runtime and editor with Vulkan rendering.
* `omnix_benchmarks.exe` — Standalone empirical performance benchmark suite.
* `transform_tests.exe` — Unit verification suite for spatial transform hierarchy and SIMD math.
* `sampler.exe` — Low-level graphics sampler and pipeline testing utility.

---

## 4. Running Verification Test Suites

### 4.1 Transform & Math Verification Suite
Executes unit tests verifying affine TRS matrix composition, parent-child transform cascading, local vs. world space transitions, and floating-point precision:

```powershell
cd d:\OmnixEngine\build_ninja
.\transform_tests.exe
```

*Expected Result*:
```
[PASS] TransformIdentityTest
[PASS] LocalToWorldComposition
[PASS] DeepHierarchyChain
[PASS] InverseTransformPropagation
ALL TESTS PASSED (100% Success)
```

### 4.2 Memory Allocator Integrity Check
Tests custom memory allocator arenas (`LinearAllocator`, `PoolAllocator`, `StackAllocator`), checking pointer alignment, boundary overrun detection, and reset behavior:

```powershell
.\Application.exe --test-memory --headless
```

*Expected Result*:
```
[INFO] [MemorySuite] LinearAllocator allocated 1000 chunks cleanly.
[INFO] [MemorySuite] PoolAllocator recycled 1000 nodes without fragmentation.
[INFO] [MemorySuite] StackAllocator verified LIFO marker resets.
[INFO] [MemorySuite] All allocator verification tests succeeded.
```

### 4.3 Deep Transform Stress Test
Runs a 100-level hierarchy tree propagation test:

```powershell
.\Application.exe --test-scene --headless
```

*Expected Result*:
```
[INFO] [SceneSuite] Traversed depth-100 hierarchy in 13.7 microseconds.
[INFO] [SceneSuite] Transform verification passed.
```

---

## 5. Running the Empirical Benchmark Harness

To reproduce the numbers reported in [`benchmarks.md`](file:///d:/OmnixEngine/docs/benchmarks.md):

```powershell
cd d:\OmnixEngine\build_ninja
.\omnix_benchmarks.exe
```

The benchmark runner executes 12 statistical workloads:
1. Allocator throughput (`Linear`, `Stack`, `Pool`)
2. Affine TRS matrix composition throughput
3. Deep transform tree traversal
4. Package archive lookup throughput
5. ECS entity instantiation (10k and 50k)
6. ECS component attachment throughput
7. ECS system update iteration
8. ECS binary serialization and deserialization

### Output Verification
The harness will output formatted telemetry directly to the console and overwrite [`benchmarks/benchmark_results.csv`](file:///d:/OmnixEngine/benchmarks/benchmark_results.csv) with updated raw metrics:

```
[Omnix Benchmarks] Running 12 empirical workloads...
========================================================================================================
Workload                                       Count     Median (us)       Mean (us)        p95 (us)        Throughput
========================================================================================================
Linear Allocator (64B)                          1000           11.70           11.76           11.80    85,034,013.61 allocs/s
Stack Allocator (128B)                           500            5.70            6.91           20.20    72,337,962.96 push-pop/s
Pool Allocator (32B)                            1000           17.90           22.27           37.90    44,901,441.34 alloc-ops/s
Matrix4x4 TRS Construction                     10000         4695.60         4722.47         5497.40     2,117,534.61 matrices/s
Package Handle Lookup                           1000          710.90          800.53         1419.50     1,249,164.62 queries/s
ECS Entity Spawn (50k)                         50000        49189.80        49199.58        52023.70     1,016,268.84 entities/s
ECS Entity Spawn (10k)                         10000        44106.80        45573.92        52564.10       219,423.76 entities/s
ECS System Iteration (10k)                     10000        44208.60        47234.43        56557.00       211,709.99 ent-ticks/s
ECS Binary Serialize (2k)                       2000        16823.10        17159.13        18751.40       116,556.04 entities/s
Transform Hierarchy (Depth 100)                    1           13.70           15.38           28.50        65,030.08 traversals/s
ECS Component Attach (10k)                     10000       152651.00       155039.58       174683.10        64,499.66 entities/s
ECS Binary Deserialize (2k)                     2000        41203.60        43101.55        58478.60        46,402.04 entities/s
========================================================================================================
[Omnix Benchmarks] Telemetry exported to benchmarks/benchmark_results.csv
```

---

## 6. Troubleshooting Common Build Issues

1. **`Vulkan headers or glslc not found`**:
   * Verify Vulkan SDK is installed and `VULKAN_SDK` environment variable is set. Check with `glslc --version`.
2. **`PhysX headers / libraries missing`**:
   * Ensure `unofficial-omniverse-physx-sdk:x64-windows` was installed via vcpkg and `CMAKE_TOOLCHAIN_FILE` was provided.
3. **`GLFW window initialization fails in CI/Headless`**:
   * Launch with the `--headless` CLI argument to bypass OS windowing and Vulkan swapchain creation.
