#include "Runtime/Public/GLTFImporter.h"
#include "Core/Logger.h"

// Define TINYGLTF_NO_STB_IMAGE_WRITE and TINYGLTF_NO_STB_IMAGE since they are already linked from RenderingEngine
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include "tiny_gltf.h"

#include <filesystem>
#include <cstring>
#include <algorithm>

namespace eng::runtime {

    bool ParseGLTF(const std::string& path, RawGLTFData& outData) {
        if (!std::filesystem::exists(path)) {
            LOG_ERROR("[GLTFImporter] GLTF file does not exist: %s", path.c_str());
            return false;
        }

        tinygltf::Model model;
        tinygltf::TinyGLTF loader;
        std::string err;
        std::string warn;

        std::string ext = std::filesystem::path(path).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
            return std::tolower(c);
        });

        bool success = false;
        if (ext == ".glb") {
            success = loader.LoadBinaryFromFile(&model, &err, &warn, path);
        } else {
            success = loader.LoadASCIIFromFile(&model, &err, &warn, path);
        }

        if (!warn.empty()) {
            LOG_WARN("[GLTFImporter] Warning parsing GLTF: %s", warn.c_str());
        }
        if (!success) {
            LOG_ERROR("[GLTFImporter] Failed to parse GLTF: %s", err.c_str());
            return false;
        }

        outData.positions.clear();
        outData.normals.clear();
        outData.tangents.clear();
        outData.uvs.clear();
        outData.indices.clear();
        outData.submeshes.clear();
        outData.hasNormals = false;
        outData.hasTangents = false;
        outData.hasUVs = false;

        // Iterate through all meshes in the GLTF file
        for (const auto& mesh : model.meshes) {
            for (const auto& primitive : mesh.primitives) {
                if (primitive.mode != TINYGLTF_MODE_TRIANGLES) {
                    continue; // We only support triangle list meshes for v0.2
                }

                uint32_t baseVertex = static_cast<uint32_t>(outData.positions.size());
                uint32_t baseIndex = static_cast<uint32_t>(outData.indices.size());

                // Find Position attribute accessor
                if (primitive.attributes.find("POSITION") == primitive.attributes.end()) {
                    continue;
                }

                int posAccessorIdx = primitive.attributes.at("POSITION");
                const auto& posAccessor = model.accessors[posAccessorIdx];
                const auto& posView = model.bufferViews[posAccessor.bufferView];
                const auto& posBuffer = model.buffers[posView.buffer];
                const uint8_t* posRaw = posBuffer.data.data() + posView.byteOffset + posAccessor.byteOffset;
                size_t posStride = posAccessor.ByteStride(posView);

                // Add vertices
                size_t vertCount = posAccessor.count;
                outData.positions.resize(baseVertex + vertCount);
                for (size_t i = 0; i < vertCount; ++i) {
                    std::memcpy(&outData.positions[baseVertex + i], posRaw + i * posStride, sizeof(Vec3));
                }

                // Get optional Normal attribute accessor
                bool primHasNormals = false;
                if (primitive.attributes.find("NORMAL") != primitive.attributes.end()) {
                    int normAccessorIdx = primitive.attributes.at("NORMAL");
                    const auto& normAccessor = model.accessors[normAccessorIdx];
                    const auto& normView = model.bufferViews[normAccessor.bufferView];
                    const auto& normBuffer = model.buffers[normView.buffer];
                    const uint8_t* normRaw = normBuffer.data.data() + normView.byteOffset + normAccessor.byteOffset;
                    size_t normStride = normAccessor.ByteStride(normView);

                    outData.normals.resize(baseVertex + vertCount);
                    for (size_t i = 0; i < vertCount; ++i) {
                        std::memcpy(&outData.normals[baseVertex + i], normRaw + i * normStride, sizeof(Vec3));
                    }
                    primHasNormals = true;
                    outData.hasNormals = true;
                } else {
                    outData.normals.resize(baseVertex + vertCount, { 0, 0, 0 });
                }

                // Get optional Tangent attribute accessor
                bool primHasTangents = false;
                if (primitive.attributes.find("TANGENT") != primitive.attributes.end()) {
                    int tangentAccessorIdx = primitive.attributes.at("TANGENT");
                    const auto& tangentAccessor = model.accessors[tangentAccessorIdx];
                    const auto& tangentView = model.bufferViews[tangentAccessor.bufferView];
                    const auto& tangentBuffer = model.buffers[tangentView.buffer];
                    const uint8_t* tangentRaw = tangentBuffer.data.data() + tangentView.byteOffset + tangentAccessor.byteOffset;
                    size_t tangentStride = tangentAccessor.ByteStride(tangentView);

                    outData.tangents.resize(baseVertex + vertCount);
                    for (size_t i = 0; i < vertCount; ++i) {
                        std::memcpy(&outData.tangents[baseVertex + i], tangentRaw + i * tangentStride, sizeof(Vec4));
                    }
                    primHasTangents = true;
                    outData.hasTangents = true;
                } else {
                    outData.tangents.resize(baseVertex + vertCount, { 0, 0, 0, 1.0f });
                }

                // Get optional UV attribute accessor
                bool primHasUVs = false;
                if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end()) {
                    int uvAccessorIdx = primitive.attributes.at("TEXCOORD_0");
                    const auto& uvAccessor = model.accessors[uvAccessorIdx];
                    const auto& uvView = model.bufferViews[uvAccessor.bufferView];
                    const auto& uvBuffer = model.buffers[uvView.buffer];
                    const uint8_t* uvRaw = uvBuffer.data.data() + uvView.byteOffset + uvAccessor.byteOffset;
                    size_t uvStride = uvAccessor.ByteStride(uvView);

                    outData.uvs.resize(baseVertex + vertCount);
                    for (size_t i = 0; i < vertCount; ++i) {
                        std::memcpy(&outData.uvs[baseVertex + i], uvRaw + i * uvStride, sizeof(Vec2));
                    }
                    primHasUVs = true;
                    outData.hasUVs = true;
                } else {
                    outData.uvs.resize(baseVertex + vertCount, { 0, 0 });
                }

                // Index Buffer Extraction
                uint32_t primIndexCount = 0;
                if (primitive.indices >= 0) {
                    const auto& indexAccessor = model.accessors[primitive.indices];
                    const auto& indexView = model.bufferViews[indexAccessor.bufferView];
                    const auto& indexBuffer = model.buffers[indexView.buffer];
                    const uint8_t* indexRaw = indexBuffer.data.data() + indexView.byteOffset + indexAccessor.byteOffset;
                    size_t indexStride = indexAccessor.ByteStride(indexView);

                    primIndexCount = static_cast<uint32_t>(indexAccessor.count);

                    for (size_t i = 0; i < primIndexCount; ++i) {
                        uint32_t idx = 0;
                        if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                            std::memcpy(&idx, indexRaw + i * indexStride, sizeof(uint32_t));
                        } else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                            uint16_t shortVal = 0;
                            std::memcpy(&shortVal, indexRaw + i * indexStride, sizeof(uint16_t));
                            idx = shortVal;
                        } else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                            uint8_t byteVal = 0;
                            std::memcpy(&byteVal, indexRaw + i * indexStride, sizeof(uint8_t));
                            idx = byteVal;
                        }
                        outData.indices.push_back(baseVertex + idx);
                    }
                } else {
                    // Generate sequential indices if no index buffer provided
                    primIndexCount = static_cast<uint32_t>(vertCount);
                    for (uint32_t i = 0; i < primIndexCount; ++i) {
                        outData.indices.push_back(baseVertex + i);
                    }
                }

                // Add to submesh table
                RawGLTFSubmesh sub;
                sub.indexStart = baseIndex;
                sub.indexCount = primIndexCount;
                sub.materialIndex = (primitive.material >= 0) ? static_cast<uint32_t>(primitive.material) : 0;
                outData.submeshes.push_back(sub);
            }
        }

        if (outData.positions.empty()) {
            LOG_ERROR("[GLTFImporter] No geometries extracted from GLTF model.");
            return false;
        }

        return true;
    }

} // namespace eng::runtime
