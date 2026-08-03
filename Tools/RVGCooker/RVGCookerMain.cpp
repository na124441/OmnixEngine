#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include "MeshCanonicalizer.h"
#include "RVGWriter.h"

void PrintUsage() {
    std::cout << "Usage: OmnixRVGCooker <source_file> <output_file> [options]\n\n"
              << "Options:\n"
              << "  --vertex-limit <limit>      Max vertices per cluster (default: 64)\n"
              << "  --triangle-limit <limit>    Max triangles per cluster (default: 128)\n"
              << "  --hierarchy-fanout <count>  Branching factor of DAG (default: 4)\n"
              << "  --simplification <settings> Path to simplification config or preset\n"
              << "  --page-size <bytes>         Virtual page memory size (default: 4096)\n"
              << "  --compression <mode>        Compression mode (e.g., none, lz4, zstd) (default: none)\n"
              << "  --debug-metadata            Include diagnostic info in cooked output\n"
              << "  --deterministic             Enable strictly deterministic cooking output\n"
              << "  --validation-only           Run mesh checks without writing cooked output\n";
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        PrintUsage();
        return 1;
    }

    std::string sourceFile = argv[1];
    std::string outputFile = argv[2];

    if (sourceFile == "-h" || sourceFile == "--help") {
        PrintUsage();
        return 0;
    }

    uint32_t vertexLimit = 64;
    uint32_t triangleLimit = 80;
    uint32_t hierarchyFanout = 4;
    std::string simplificationSettings = "default";
    uint32_t pageSize = 4096;
    std::string compressionMode = "none";
    bool debugMetadata = false;
    bool deterministic = false;
    bool validationOnly = false;

    for (int i = 3; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--vertex-limit" && i + 1 < argc) {
            vertexLimit = std::stoul(argv[++i]);
        } else if (arg == "--triangle-limit" && i + 1 < argc) {
            triangleLimit = std::stoul(argv[++i]);
        } else if (arg == "--hierarchy-fanout" && i + 1 < argc) {
            hierarchyFanout = std::stoul(argv[++i]);
        } else if (arg == "--simplification" && i + 1 < argc) {
            simplificationSettings = argv[++i];
        } else if (arg == "--page-size" && i + 1 < argc) {
            pageSize = std::stoul(argv[++i]);
        } else if (arg == "--compression" && i + 1 < argc) {
            compressionMode = argv[++i];
        } else if (arg == "--debug-metadata") {
            debugMetadata = true;
        } else if (arg == "--deterministic") {
            deterministic = true;
        } else if (arg == "--validation-only") {
            validationOnly = true;
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            PrintUsage();
            return 1;
        }
    }

    std::cout << "OmnixRVGCooker Starting...\n"
              << " - Source: " << sourceFile << "\n"
              << " - Output: " << outputFile << "\n"
              << " - Vertex Limit: " << vertexLimit << "\n"
              << " - Triangle Limit: " << triangleLimit << "\n"
              << " - Hierarchy Fanout: " << hierarchyFanout << "\n"
              << " - Simplification Settings: " << simplificationSettings << "\n"
              << " - Page Size: " << pageSize << " bytes\n"
              << " - Compression Mode: " << compressionMode << "\n"
              << " - Debug Metadata: " << (debugMetadata ? "ON" : "OFF") << "\n"
              << " - Deterministic Mode: " << (deterministic ? "ON" : "OFF") << "\n"
              << " - Validation Only: " << (validationOnly ? "ON" : "OFF") << "\n";

    // Run Mesh Canonicalization
    eng::cooker::MeshCanonicalizer canonicalizer;
    if (!canonicalizer.Canonicalize(sourceFile, outputFile, pageSize, vertexLimit, triangleLimit)) {
        std::cerr << "Error: Mesh Canonicalization failed.\n";
        return 1;
    }

    if (validationOnly) {
        std::cout << "Validation successful. Exiting (Validation Only).\n";
        return 0;
    }

    std::cout << "OmnixRVGCooker completed successfully.\n";
    return 0;
}
