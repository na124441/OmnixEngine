#include <iostream>
#include <vector>
#include <chrono>
#include <numeric>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <string>
#include <fstream>
#include <memory>

// Omnix Engine Core & Systems Headers
#include "Core/World.h"
#include "ECS/Coordinator.h"
#include "ECS/ECSComponents.h"

// Scene Transform
#define Transform SceneTransform
#include "Scene/Transform.h"
#undef Transform

// Serializer ECS and Components
#include "Serializer/ECS/ECS.h"
#include "Serializer/ECS/SchemaRegistry.h"
#include "Serializer/ECS/SerializationBridge.h"
#include "Serializer/Serialization/Normal/NormalSerializer.h"
#include "Serializer/Serialization/Normal/NormalDeserializer.h"
#include "Runtime/Public/AssetManager.h"
#include "Runtime/Public/PackageManager.h"
#include "Runtime/Public/PackageBuilder.h"
#include "Runtime/Public/World/WorldZone.h"
#include "Core/Memory/LinearAllocator.h"
#include "Core/Memory/PoolAllocator.h"
#include "Core/Memory/StackAllocator.h"
#include "Core/Memory/AllocationTracker.h"
#include "Core/Logger.h"

namespace {

struct BenchmarkStats {
    std::string name;
    std::string configuration;
    std::string unit = "us";
    size_t runs = 0;
    double mean = 0.0;
    double median = 0.0;
    double stddev = 0.0;
    double minVal = 0.0;
    double maxVal = 0.0;
    double p95 = 0.0;
    double p99 = 0.0;
    double throughput = 0.0; // ops/s or items/s
    std::string throughputUnit = "ops/s";
    double memoryMB = 0.0;
};

template<typename Func>
BenchmarkStats RunBenchmark(const std::string& name,
                            const std::string& config,
                            size_t warmupRuns,
                            size_t measuredRuns,
                            double opsPerIteration,
                            Func&& fn)
{
    // Warmup
    for (size_t i = 0; i < warmupRuns; ++i) {
        fn();
    }

    std::vector<double> latenciesUs;
    latenciesUs.reserve(measuredRuns);

    for (size_t i = 0; i < measuredRuns; ++i) {
        auto t0 = std::chrono::high_resolution_clock::now();
        fn();
        auto t1 = std::chrono::high_resolution_clock::now();
        double elapsedUs = std::chrono::duration<double, std::micro>(t1 - t0).count();
        latenciesUs.push_back(elapsedUs);
    }

    std::sort(latenciesUs.begin(), latenciesUs.end());

    double sum = std::accumulate(latenciesUs.begin(), latenciesUs.end(), 0.0);
    double mean = sum / measuredRuns;

    double variance = 0.0;
    for (double val : latenciesUs) {
        variance += (val - mean) * (val - mean);
    }
    double stddev = std::sqrt(variance / measuredRuns);

    double median = latenciesUs[measuredRuns / 2];
    double minVal = latenciesUs.front();
    double maxVal = latenciesUs.back();
    double p95 = latenciesUs[static_cast<size_t>(measuredRuns * 0.95)];
    double p99 = latenciesUs[static_cast<size_t>(measuredRuns * 0.99)];

    double totalTimeSec = sum / 1e6;
    double throughput = (opsPerIteration * measuredRuns) / (totalTimeSec > 0.0 ? totalTimeSec : 1e-9);

    BenchmarkStats stats;
    stats.name = name;
    stats.configuration = config;
    stats.runs = measuredRuns;
    stats.mean = mean;
    stats.median = median;
    stats.stddev = stddev;
    stats.minVal = minVal;
    stats.maxVal = maxVal;
    stats.p95 = p95;
    stats.p99 = p99;
    stats.throughput = throughput;
    stats.memoryMB = 0.0;
    return stats;
}

void PrintHeader() {
    std::cout << "\n========================================================================================================================\n";
    std::cout << "                                  OMNIX ENGINE STANDALONE BENCHMARK HARNESS v1.0                                       \n";
    std::cout << "========================================================================================================================\n";
}

void PrintTable(const std::vector<BenchmarkStats>& results) {
    std::cout << "\n" << std::left 
              << std::setw(32) << "Workload"
              << std::setw(26) << "Configuration"
              << std::right
              << std::setw(12) << "Median (us)"
              << std::setw(12) << "Mean (us)"
              << std::setw(12) << "p95 (us)"
              << std::setw(12) << "p99 (us)"
              << std::setw(18) << "Throughput"
              << "\n";
    std::cout << std::string(124, '-') << "\n";

    for (const auto& r : results) {
        std::cout << std::left
                  << std::setw(32) << r.name
                  << std::setw(26) << r.configuration
                  << std::right
                  << std::fixed << std::setprecision(2)
                  << std::setw(12) << r.median
                  << std::setw(12) << r.mean
                  << std::setw(12) << r.p95
                  << std::setw(12) << r.p99
                  << std::setw(16) << r.throughput << " " << r.throughputUnit
                  << "\n";
    }
    std::cout << std::string(124, '-') << "\n";
}

} // namespace

int main(int argc, char* argv[]) {
    PrintHeader();

    // Disable excessive logging during benchmarks
    Logger::Init("benchmark_run.log", LogLevel::Error);

    std::vector<BenchmarkStats> results;

    // =========================================================================
    // 1. ECS Entity Lifecycle Benchmarks
    // =========================================================================
    std::cout << "[1/6] Running ECS Entity Lifecycle Benchmarks...\n";

    // 1.1 Spawn 10,000 Entities
    {
        auto stats = RunBenchmark("ECS Entity Spawn", "10,000 entities", 5, 25, 10000.0, []() {
            Coordinator coord;
            coord.Init();
            for (int i = 0; i < 10000; ++i) {
                Entity e = coord.CreateEntity();
                (void)e;
            }
        });
        stats.throughputUnit = "entities/s";
        results.push_back(stats);
    }

    // 1.2 Spawn 50,000 Entities
    {
        auto stats = RunBenchmark("ECS Entity Spawn", "50,000 entities", 3, 15, 50000.0, []() {
            Coordinator coord;
            coord.Init();
            for (int i = 0; i < 50000; ++i) {
                Entity e = coord.CreateEntity();
                (void)e;
            }
        });
        stats.throughputUnit = "entities/s";
        results.push_back(stats);
    }

    // 1.3 Add Component (Transform + RigidBody) to 10,000 Entities
    {
        auto stats = RunBenchmark("ECS Component Attach", "10,000 ent (2 comps)", 5, 25, 10000.0, []() {
            Coordinator coord;
            coord.Init();
            coord.RegisterComponent<TransformComponent>();
            coord.RegisterComponent<RigidBodyComponent>();
            for (int i = 0; i < 10000; ++i) {
                Entity e = coord.CreateEntity();
                TransformComponent tc{};
                tc.position = Vector3(1.0f, 2.0f, 3.0f);
                coord.AddComponent(e, tc);
                RigidBodyComponent rb{};
                rb.velocity = Vector3(0.0f, -9.8f, 0.0f);
                coord.AddComponent(e, rb);
            }
        });
        stats.throughputUnit = "entities/s";
        results.push_back(stats);
    }

    // 1.4 Linear ECS System Iteration (Movement simulation)
    {
        Coordinator coord;
        coord.Init();
        coord.RegisterComponent<TransformComponent>();
        coord.RegisterComponent<RigidBodyComponent>();

        for (int i = 0; i < 10000; ++i) {
            Entity e = coord.CreateEntity();
            TransformComponent tc{};
            tc.position = Vector3(float(i), 0.0f, 0.0f);
            coord.AddComponent(e, tc);
            RigidBodyComponent rb{};
            rb.velocity = Vector3(1.0f, 0.0f, 0.0f);
            coord.AddComponent(e, rb);
        }

        const auto& activeEnts = coord.GetActiveEntities();

        auto stats = RunBenchmark("ECS System Iteration", "10,000 entities (dt)", 10, 100, 10000.0, [&]() {
            float dt = 0.01667f;
            for (Entity e : activeEnts) {
                auto& tc = coord.GetComponent<TransformComponent>(e);
                const auto& rb = coord.GetComponent<RigidBodyComponent>(e);
                tc.position.x += rb.velocity.x * dt;
                tc.position.y += rb.velocity.y * dt;
                tc.position.z += rb.velocity.z * dt;
            }
        });
        stats.throughputUnit = "ent-ticks/s";
        results.push_back(stats);
    }

    // =========================================================================
    // 2. Spatial Math & Matrix Hierarchy Benchmarks
    // =========================================================================
    std::cout << "[2/6] Running Transform Matrix & Hierarchy Benchmarks...\n";

    // 2.1 4x4 TRS Matrix Construction (10,000 transforms)
    {
        std::vector<Vector3> positions(10000);
        std::vector<Quaternion> rotations(10000);
        std::vector<Vector3> scales(10000);
        for (size_t i = 0; i < 10000; ++i) {
            positions[i] = Vector3(float(i), 1.0f, 2.0f);
            rotations[i] = Quaternion(0.0f, 0.707f, 0.0f, 0.707f);
            scales[i] = Vector3(2.0f, 2.0f, 2.0f);
        }
        std::vector<Matrix4x4> matrices(10000);

        auto stats = RunBenchmark("Matrix4x4 TRS Construction", "10,000 transforms", 10, 100, 10000.0, [&]() {
            for (size_t i = 0; i < 10000; ++i) {
                matrices[i] = Matrix4x4::TRS(positions[i], rotations[i], scales[i]);
            }
        });
        stats.throughputUnit = "matrices/s";
        results.push_back(stats);
    }

    // 2.2 Deep Transform Hierarchy Matrix Multiplication (100 levels deep)
    {
        std::vector<Matrix4x4> chain(100);
        for (int i = 0; i < 100; ++i) {
            chain[i] = Matrix4x4::Translation(Vector3(1.0f, 0.5f, 0.0f));
        }

        auto stats = RunBenchmark("Transform Hierarchy Tree", "Depth 100 multiply chain", 20, 200, 1.0, [&]() {
            Matrix4x4 world = chain[0];
            for (size_t i = 1; i < chain.size(); ++i) {
                world = world * chain[i];
            }
        });
        stats.throughputUnit = "traversals/s";
        results.push_back(stats);
    }

    // =========================================================================
    // 3. Binary & Snapshot Serialization Benchmarks
    // =========================================================================
    std::cout << "[3/6] Running Serialization & Deserialization Benchmarks...\n";

    {
        ComponentSchemaRegistry registry;
        static FieldSchema transformFields[] = {
            { "position", offsetof(Transform, position), FieldType::VEC3, INTENT_SERIALIZABLE, sizeof(Vector3) },
            { "rotation", offsetof(Transform, rotation), FieldType::QUAT, INTENT_SERIALIZABLE, sizeof(Quaternion) },
            { "scale", offsetof(Transform, scale), FieldType::VEC3, INTENT_SERIALIZABLE, sizeof(Vector3) }
        };
        ComponentSchema transformSchema = { "Transform", sizeof(Transform), transformFields, 3 };
        registry.RegisterSchema(TRANSFORM_COMPONENT, transformSchema);

        ECS ecs;
        ecs.Initialize(&registry);

        for (uint32_t i = 0; i < 2000; ++i) {
            uint32_t ent = ecs.CreateEntity();
            Transform t{ Vector3(float(i), 1.0f, 2.0f), Quaternion(0.0f, 0.0f, 0.0f, 1.0f), Vector3(1.0f, 1.0f, 1.0f) };
            ecs.AddComponent(ent, t);
        }

        SerializationBridge bridge(ecs, registry);
        SnapshotContext ctx(SNAPSHOT_SAVE);
        bridge.Capture(ctx);
        const ECSSnapshot& snapshot = bridge.GetSnapshot();

        std::vector<uint8_t> buffer;
        NormalSerializer serializer;

        // 3.1 Serialization to Buffer
        auto serStats = RunBenchmark("ECS Binary Serialize", "2,000 entities in RAM", 10, 50, 2000.0, [&]() {
            buffer.clear();
            serializer.SerializeToBuffer(snapshot, buffer);
        });
        serStats.throughputUnit = "entities/s";
        results.push_back(serStats);

        // 3.2 Deserialization from Buffer
        NormalDeserializer deserializer;
        auto deserStats = RunBenchmark("ECS Binary Deserialize", "2,000 entities from RAM", 10, 50, 2000.0, [&]() {
            ECSSnapshot* loaded = deserializer.DeserializeToSnapshot(buffer.data(), buffer.size(), registry);
            delete loaded;
        });
        deserStats.throughputUnit = "entities/s";
        results.push_back(deserStats);

        ecs.Shutdown();
    }

    // =========================================================================
    // 4. Memory Allocator Microbenchmarks
    // =========================================================================
    std::cout << "[4/6] Running Custom Memory Allocator Benchmarks...\n";

    // 4.1 Linear Allocator (64 KB chunk allocations)
    {
        constexpr size_t ARENA_SIZE = 1024 * 1024; // 1 MB
        eng::memory::LinearAllocator allocator(ARENA_SIZE);

        auto stats = RunBenchmark("Linear Allocator", "1,000 allocs (64B)", 20, 100, 1000.0, [&]() {
            allocator.Reset();
            for (int i = 0; i < 1000; ++i) {
                void* p = allocator.Allocate(64, 8);
                (void)p;
            }
        });
        stats.throughputUnit = "allocs/s";
        results.push_back(stats);
    }

    // 4.2 Pool Allocator (32-byte uniform objects)
    {
        eng::memory::PoolAllocator pool(32, 1000);

        auto stats = RunBenchmark("Pool Allocator", "1,000 alloc/free (32B)", 20, 100, 1000.0, [&]() {
            void* ptrs[1000];
            for (int i = 0; i < 1000; ++i) {
                ptrs[i] = pool.Allocate();
            }
            for (int i = 0; i < 1000; ++i) {
                pool.Free(ptrs[i]);
            }
        });
        stats.throughputUnit = "alloc-ops/s";
        results.push_back(stats);
    }

    // 4.3 Stack Allocator
    {
        eng::memory::StackAllocator stack(1024 * 1024);

        auto stats = RunBenchmark("Stack Allocator", "500 push/pop (128B)", 20, 100, 500.0, [&]() {
            auto marker = stack.GetMarker();
            for (int i = 0; i < 500; ++i) {
                void* p = stack.Allocate(128, 8);
                (void)p;
            }
            stack.FreeToMarker(marker);
        });
        stats.throughputUnit = "push-pop/s";
        results.push_back(stats);
    }

    // =========================================================================
    // 5. Package Archive Mounting & Handle Resolution
    // =========================================================================
    std::cout << "[5/6] Running Package Manager & Asset Handle Resolution...\n";

    {
        eng::runtime::PackageManager pkgMgr;
        eng::runtime::PackageBuilder builder;
        std::vector<uint8_t> dummyData = { 1, 2, 3, 4, 5, 6, 7, 8 };

        for (uint64_t i = 1; i <= 100; ++i) {
            builder.AddAsset(AssetHandle{i}, AssetType::Mesh, dummyData, {});
        }

        std::string pkgPath = "bench_temp.omnixpackage";
        builder.Build(pkgPath);

        pkgMgr.MountPackage(pkgPath);

        auto stats = RunBenchmark("Package Handle Lookup", "1,000 queries in archive", 20, 100, 1000.0, [&]() {
            for (int i = 0; i < 1000; ++i) {
                uint64_t handleVal = (i % 100) + 1;
                bool exists = pkgMgr.Contains(AssetHandle{handleVal});
                (void)exists;
            }
        });
        stats.throughputUnit = "queries/s";
        results.push_back(stats);

        pkgMgr.UnmountPackage(pkgPath);
        std::filesystem::remove(pkgPath);
    }

    // =========================================================================
    // 6. Summary Output & CSV Export of Results
    // =========================================================================
    std::cout << "[6/6] Formatting Benchmark Results...\n";
    PrintTable(results);

    // Save results to CSV for reproducible audits
    std::filesystem::create_directories("benchmarks");
    std::ofstream csv("benchmarks/benchmark_results.csv");
    if (csv.is_open()) {
        csv << "Workload,Configuration,Runs,Median_us,Mean_us,StdDev_us,Min_us,Max_us,p95_us,p99_us,Throughput,ThroughputUnit\n";
        for (const auto& r : results) {
            csv << "\"" << r.name << "\",\"" << r.configuration << "\"," << r.runs << ","
                << r.median << "," << r.mean << "," << r.stddev << ","
                << r.minVal << "," << r.maxVal << "," << r.p95 << "," << r.p99 << ","
                << r.throughput << ",\"" << r.throughputUnit << "\"\n";
        }
        csv.close();
        std::cout << "[OK] Benchmark CSV exported to benchmarks/benchmark_results.csv\n";
    }

    Logger::Shutdown();
    std::cout << "\nOmnix Engine Benchmark Suite completed successfully.\n";
    return 0;
}
