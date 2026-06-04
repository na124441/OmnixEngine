#include "Runtime/Public/MeshImportTests.h"
#include "Runtime/Public/MeshImporter.h"
#include "Runtime/Public/MeshMetadata.h"
#include "Runtime/Public/MeshValidation.h"
#include "Runtime/Public/OmnixMeshFormat.h"
#include "Core/Logger.h"
#include <vector>
#include <string>
#include <filesystem>
#include <fstream>
#include <cmath>

namespace eng::runtime {

    bool RunMeshImportTests() noexcept {
        LOG_INFO("================================================================================");
        LOG_INFO("                       RUNNING OMNIX MESH IMPORT TESTS                          ");
        LOG_INFO("================================================================================");

        std::filesystem::create_directories("TestMeshes");
        std::filesystem::create_directories("Cache/Meshes");

        MeshImporter importer;

        // -----------------------------------------------------------------------------
        // Test 1 — OBJ Cube Import
        // -----------------------------------------------------------------------------
        LOG_INFO("[MeshTest] Running Test 1: OBJ Cube Import...");
        {
            // Write a simple quad-based cube OBJ
            std::string objFile = "TestMeshes/cube.obj";
            std::ofstream file(objFile);
            file << "# Cube model\n"
                 << "v 0 0 0\n"
                 << "v 1 0 0\n"
                 << "v 1 1 0\n"
                 << "v 0 1 0\n"
                 << "v 0 0 1\n"
                 << "v 1 0 1\n"
                 << "v 1 1 1\n"
                 << "v 0 1 1\n"
                 << "vt 0 0\n"
                 << "vt 1 0\n"
                 << "vt 1 1\n"
                 << "vt 0 1\n"
                 << "vn 0 0 -1\n"
                 << "vn 0 0 1\n"
                 << "vn 0 -1 0\n"
                 << "vn 0 1 0\n"
                 << "vn -1 0 0\n"
                 << "vn 1 0 0\n"
                 << "# Faces (quads - 4 indices, will be triangulated)\n"
                 << "f 1/1/1 2/2/1 3/3/1 4/4/1\n" // quad
                 << "f 5/1/2 6/2/2 7/3/2 8/4/2\n"
                 << "f 1/1/3 2/2/3 6/3/3 5/4/3\n"
                 << "f 2/1/4 3/2/4 7/3/4 6/4/4\n"
                 << "f 3/1/5 4/2/5 8/3/5 7/4/5\n"
                 << "f 4/1/6 1/2/6 5/3/6 8/4/6\n";
            file.close();

            std::string cacheFile = "Cache/Meshes/cube.omnixmesh";
            MeshMetadata metadata;
            if (!importer.ImportMesh(objFile, cacheFile, metadata, true)) {
                LOG_ERROR("[MeshTest] Test 1 FAILED: OBJ import failed!");
                return false;
            }

            // A cube with 6 faces (each triangulated into 2 triangles = 12 triangles = 36 indices)
            if (metadata.indexCount != 36 || metadata.vertexCount == 0 || metadata.submeshCount != 1) {
                LOG_ERROR("[MeshTest] Test 1 FAILED: Mesh indices/vertices count mismatch!");
                return false;
            }

            if (!std::filesystem::exists(cacheFile) || !std::filesystem::exists(cacheFile + ".meta")) {
                LOG_ERROR("[MeshTest] Test 1 FAILED: Serialized file or metadata sidecar not written.");
                return false;
            }

            LOG_INFO("[MeshTest] Test 1 Passed: OBJ cube imported and triangulated successfully.");
        }

        // -----------------------------------------------------------------------------
        // Test 2 — GLTF Mesh Import
        // -----------------------------------------------------------------------------
        LOG_INFO("[MeshTest] Running Test 2: GLTF Mesh Import...");
        {
            // Write a valid ascii gltf file and a raw bin buffer
            std::string gltfFile = "TestMeshes/triangle.gltf";
            std::string binFile = "TestMeshes/triangle.bin";

            // Positions: (0,0,0), (1,0,0), (0,1,0) (3 vertices, 36 bytes)
            // Indices: 0, 1, 2 (3 uint16_t, 6 bytes)
            std::ofstream bin(binFile, std::ios::binary);
            float pos[9] = { 0,0,0, 1,0,0, 0,1,0 };
            uint16_t idx[3] = { 0, 1, 2 };
            bin.write(reinterpret_cast<const char*>(pos), sizeof(pos));
            bin.write(reinterpret_cast<const char*>(idx), sizeof(idx));
            bin.close();

            std::ofstream gltf(gltfFile);
            gltf << "{\n"
                 << "  \"asset\": { \"version\": \"2.0\" },\n"
                 << "  \"meshes\": [\n"
                 << "    {\n"
                 << "      \"primitives\": [\n"
                 << "        {\n"
                 << "          \"attributes\": { \"POSITION\": 0 },\n"
                 << "          \"indices\": 1,\n"
                 << "          \"material\": 0\n"
                 << "        }\n"
                 << "      ]\n"
                 << "    }\n"
                 << "  ],\n"
                 << "  \"buffers\": [ { \"uri\": \"triangle.bin\", \"byteLength\": 42 } ],\n"
                 << "  \"bufferViews\": [\n"
                 << "    { \"buffer\": 0, \"byteOffset\": 0, \"byteLength\": 36, \"target\": 34962 },\n"
                 << "    { \"buffer\": 0, \"byteOffset\": 36, \"byteLength\": 6, \"target\": 34963 }\n"
                 << "  ],\n"
                 << "  \"accessors\": [\n"
                 << "    { \"bufferView\": 0, \"byteOffset\": 0, \"componentType\": 5126, \"count\": 3, \"type\": \"VEC3\" },\n"
                 << "    { \"bufferView\": 1, \"byteOffset\": 0, \"componentType\": 5123, \"count\": 3, \"type\": \"SCALAR\" }\n"
                 << "  ],\n"
                 << "  \"materials\": [ { \"name\": \"RedMaterial\" } ]\n"
                 << "}\n";
            gltf.close();

            std::string cacheFile = "Cache/Meshes/triangle.omnixmesh";
            MeshMetadata metadata;
            if (!importer.ImportMesh(gltfFile, cacheFile, metadata, true)) {
                LOG_ERROR("[MeshTest] Test 2 FAILED: GLTF import failed!");
                return false;
            }

            if (metadata.vertexCount != 3 || metadata.indexCount != 3 || metadata.submeshCount != 1) {
                LOG_ERROR("[MeshTest] Test 2 FAILED: GLTF geometry count mismatch!");
                return false;
            }

            LOG_INFO("[MeshTest] Test 2 Passed: GLTF primitive buffers extracted and mapped successfully.");
        }

        // -----------------------------------------------------------------------------
        // Test 3 — Missing Normals Test
        // -----------------------------------------------------------------------------
        LOG_INFO("[MeshTest] Running Test 3: Missing Normals Test...");
        {
            // OBJ without normals (no vn)
            std::string objFile = "TestMeshes/no_normals.obj";
            std::ofstream file(objFile);
            file << "v 0 0 0\n"
                 << "v 1 0 0\n"
                 << "v 0 1 0\n"
                 << "f 1 2 3\n";
            file.close();

            std::string cacheFile = "Cache/Meshes/no_normals.omnixmesh";
            MeshMetadata metadata;
            if (!importer.ImportMesh(objFile, cacheFile, metadata, true)) {
                LOG_ERROR("[MeshTest] Test 3 FAILED: Import failed!");
                return false;
            }

            if (!metadata.hasNormals) {
                LOG_ERROR("[MeshTest] Test 3 FAILED: Normals were not generated for missing normal mesh!");
                return false;
            }

            // Verify normals are normalized and finite
            OmnixMesh loaded;
            DeserializeMesh(loaded, cacheFile);
            for (const auto& v : loaded.vertices) {
                float len = std::sqrt(v.normal.x * v.normal.x + v.normal.y * v.normal.y + v.normal.z * v.normal.z);
                if (std::abs(len - 1.0f) > 0.05f) {
                    LOG_ERROR("[MeshTest] Test 3 FAILED: Generated normal is not unit length!");
                    return false;
                }
            }

            LOG_INFO("[MeshTest] Test 3 Passed: Normals successfully generated and verified.");
        }

        // -----------------------------------------------------------------------------
        // Test 4 — Tangent Generation Test
        // -----------------------------------------------------------------------------
        LOG_INFO("[MeshTest] Running Test 4: Tangent Generation Test...");
        {
            // OBJ with positions and UVs (no tangents possible in OBJ)
            std::string objFile = "TestMeshes/no_tangents.obj";
            std::ofstream file(objFile);
            file << "v 0 0 0\n"
                 << "v 1 0 0\n"
                 << "v 0 1 0\n"
                 << "vt 0 0\n"
                 << "vt 1 0\n"
                 << "vt 0 1\n"
                 << "f 1/1 2/2 3/3\n";
            file.close();

            std::string cacheFile = "Cache/Meshes/no_tangents.omnixmesh";
            MeshMetadata metadata;
            if (!importer.ImportMesh(objFile, cacheFile, metadata, true)) {
                LOG_ERROR("[MeshTest] Test 4 FAILED: Import failed!");
                return false;
            }

            if (!metadata.hasTangents) {
                LOG_ERROR("[MeshTest] Test 4 FAILED: Tangents were not generated!");
                return false;
            }

            // Verify tangent lengths and handedness
            OmnixMesh loaded;
            DeserializeMesh(loaded, cacheFile);
            for (const auto& v : loaded.vertices) {
                float len = std::sqrt(v.tangent.x * v.tangent.x + v.tangent.y * v.tangent.y + v.tangent.z * v.tangent.z);
                if (std::abs(len - 1.0f) > 0.05f) {
                    LOG_ERROR("[MeshTest] Test 4 FAILED: Generated tangent is not unit length!");
                    return false;
                }
                if (v.tangent.w != 1.0f && v.tangent.w != -1.0f) {
                    LOG_ERROR("[MeshTest] Test 4 FAILED: Tangent handedness is invalid: %f", v.tangent.w);
                    return false;
                }
            }

            LOG_INFO("[MeshTest] Test 4 Passed: Tangents successfully generated and validated.");
        }

        // -----------------------------------------------------------------------------
        // Test 5 — Invalid Index Test
        // -----------------------------------------------------------------------------
        LOG_INFO("[MeshTest] Running Test 5: Invalid Index Test...");
        {
            // OBJ referencing face index out of bounds
            std::string objFile = "TestMeshes/broken.obj";
            std::ofstream file(objFile);
            file << "v 0 0 0\n"
                 << "v 1 0 0\n"
                 << "f 1 2 99\n"; // Vertex index 99 does not exist!
            file.close();

            std::string cacheFile = "Cache/Meshes/broken.omnixmesh";
            MeshMetadata metadata;

            // Import should fail safely and return false
            bool ok = importer.ImportMesh(objFile, cacheFile, metadata, true);
            if (ok) {
                LOG_ERROR("[MeshTest] Test 5 FAILED: Mesh with out of bounds indices was accepted!");
                return false;
            }
            LOG_INFO("[MeshTest] Test 5 Passed: Out of bounds indices successfully detected and rejected.");
        }

        // -----------------------------------------------------------------------------
        // Test 6 — Round Trip Test
        // -----------------------------------------------------------------------------
        LOG_INFO("[MeshTest] Running Test 6: Round Trip Test...");
        {
            std::string sourceFile = "TestMeshes/cube.obj";
            std::string cacheFile = "Cache/Meshes/cube.omnixmesh";

            MeshMetadata meta1;
            if (!importer.ImportMesh(sourceFile, cacheFile, meta1, true)) {
                LOG_ERROR("[MeshTest] Failed import step in Round Trip test");
                return false;
            }

            MeshMetadata meta2;
            if (!LoadMeshMetadata(meta2, cacheFile + ".meta")) {
                LOG_ERROR("[MeshTest] Test 6 FAILED: Could not load metadata!");
                return false;
            }

            if (meta2.vertexCount != meta1.vertexCount ||
                meta2.indexCount != meta1.indexCount ||
                meta2.bounds.min.x != meta1.bounds.min.x ||
                meta2.bounds.max.z != meta1.bounds.max.z ||
                meta2.sphere.radius != meta1.sphere.radius ||
                meta2.hasNormals != meta1.hasNormals ||
                meta2.hasTangents != meta1.hasTangents ||
                meta2.sourceTimestamp != meta1.sourceTimestamp) {
                LOG_ERROR("[MeshTest] Test 6 FAILED: Loaded metadata fields mismatch!");
                return false;
            }
            LOG_INFO("[MeshTest] Test 6 Passed: Round trip metadata loading succeeded.");
        }

        // Clean up
        try {
            std::filesystem::remove_all("TestMeshes");
            std::filesystem::remove_all("Cache");
        } catch (...) {}

        LOG_INFO("================================================================================");
        LOG_INFO("                   ALL OMNIX MESH IMPORT TESTS PASSED                           ");
        LOG_INFO("================================================================================");
        return true;
    }

} // namespace eng::runtime
