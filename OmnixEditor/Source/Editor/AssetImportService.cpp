#include "Editor/AssetImportService.h"
#include "Runtime/MeshImporter.h"
#include "Runtime/TextureImporter.h"
#include "Runtime/OmnixMaterialFormat.h"
#include "Core/Logging/Logger.h"
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <ctime>
#include <iomanip>
#include <unordered_map>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace eng::runtime {

// Static variables definition
std::vector<ImportLogEntry> AssetImportService::s_LogEntries;
std::vector<std::string> AssetImportService::s_DroppedFiles;
std::mutex AssetImportService::s_LogMutex;
std::mutex AssetImportService::s_DropMutex;

static std::string GetCurrentTimestampString() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    struct tm timeinfo;
#ifdef _WIN32
    localtime_s(&timeinfo, &in_time_t);
#else
    localtime_r(&timeinfo, &in_time_t);
#endif
    ss << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

static inline std::string Trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, last - first + 1);
}

struct ParsedMaterial {
    std::string name;
    std::string albedoTex;
    std::string normalTex;
    std::string metallicRoughnessTex;
    std::string aoTex;
    std::string emissiveTex;
};

void AssetImportService::LogInfo(const std::string& msg) {
    std::lock_guard<std::mutex> lock(s_LogMutex);
    s_LogEntries.push_back({ ImportLogSeverity::Info, msg, GetCurrentTimestampString() });
    CORE_LOG_INFO("[ImportService] %s", msg.c_str());
}

void AssetImportService::LogWarning(const std::string& msg) {
    std::lock_guard<std::mutex> lock(s_LogMutex);
    s_LogEntries.push_back({ ImportLogSeverity::Warning, msg, GetCurrentTimestampString() });
    CORE_LOG_WARN("[ImportService] %s", msg.c_str());
}

void AssetImportService::LogError(const std::string& msg) {
    std::lock_guard<std::mutex> lock(s_LogMutex);
    s_LogEntries.push_back({ ImportLogSeverity::Error, msg, GetCurrentTimestampString() });
    CORE_LOG_ERROR("[ImportService] %s", msg.c_str());
}

void AssetImportService::ClearLogs() {
    std::lock_guard<std::mutex> lock(s_LogMutex);
    s_LogEntries.clear();
}

const std::vector<ImportLogEntry>& AssetImportService::GetLogEntries() {
    std::lock_guard<std::mutex> lock(s_LogMutex);
    return s_LogEntries;
}

void AssetImportService::AddDroppedFile(const std::string& path) {
    std::lock_guard<std::mutex> lock(s_DropMutex);
    s_DroppedFiles.push_back(path);
}

bool AssetImportService::HasDroppedFiles() {
    std::lock_guard<std::mutex> lock(s_DropMutex);
    return !s_DroppedFiles.empty();
}

std::string AssetImportService::PopDroppedFile() {
    std::lock_guard<std::mutex> lock(s_DropMutex);
    if (s_DroppedFiles.empty()) return "";
    std::string res = s_DroppedFiles.front();
    s_DroppedFiles.erase(s_DroppedFiles.begin());
    return res;
}

std::string AssetImportService::ImportModel(const std::string& sourcePath, AssetRegistry* registry) {
    if (sourcePath.empty() || !registry) {
        LogError("Invalid source path or registry.");
        return "";
    }

    try {
        ClearLogs();
        LogInfo("Starting import of: " + sourcePath);

        std::filesystem::path srcPath(sourcePath);
        if (!std::filesystem::exists(srcPath)) {
            LogError("Source file does not exist: " + sourcePath);
            return "";
        }

        std::filesystem::path destDir("Assets/Models");
        std::filesystem::create_directories(destDir);

        std::filesystem::path destPath = destDir / srcPath.filename();

        // Prevent overwrite without confirmation on Windows
        if (std::filesystem::exists(destPath)) {
#ifdef _WIN32
            int msgboxID = MessageBoxA(
                NULL,
                ("The file " + destPath.filename().string() + " already exists in Assets/Models. Do you want to overwrite it?").c_str(),
                "Confirm Overwrite",
                MB_ICONQUESTION | MB_YESNO
            );
            if (msgboxID == IDNO) {
                LogInfo("Import cancelled by user (overwrite declined).");
                return "";
            }
#endif
        }

        std::filesystem::copy_file(srcPath, destPath, std::filesystem::copy_options::overwrite_existing);
        LogInfo("Copied model file to " + destPath.string());

        // Parse OBJ for mtllib and usemtl slots
        std::string mtllibFilename = "";
        std::vector<std::string> materialSlots;
        {
            std::ifstream objFile(destPath);
            if (!objFile.is_open()) {
                LogError("Failed to open copied OBJ for parsing: " + destPath.string());
                return "";
            }
            std::string line;
            while (std::getline(objFile, line)) {
                line = Trim(line);
                if (line.empty() || line[0] == '#') continue;

                std::stringstream ss(line);
                std::string prefix;
                ss >> prefix;
                if (prefix == "mtllib") {
                    std::string rest;
                    std::getline(ss, rest);
                    mtllibFilename = Trim(rest);
                } else if (prefix == "usemtl") {
                    std::string matName;
                    std::getline(ss, matName);
                    matName = Trim(matName);
                    if (!matName.empty() && std::find(materialSlots.begin(), materialSlots.end(), matName) == materialSlots.end()) {
                        materialSlots.push_back(matName);
                    }
                }
            }
        }

        if (materialSlots.empty()) {
            materialSlots.push_back(destPath.stem().string() + "_material");
        }
        LogInfo("Detected " + std::to_string(materialSlots.size()) + " material slots.");

        // Parse companion MTL file if present
        std::unordered_map<std::string, ParsedMaterial> mtlMaterials;
        if (!mtllibFilename.empty()) {
            std::filesystem::path mtlPath = srcPath.parent_path() / mtllibFilename;
            if (!std::filesystem::exists(mtlPath)) {
                mtlPath = destPath.parent_path() / mtllibFilename;
            }

            if (std::filesystem::exists(mtlPath)) {
                // Copy mtl file to Assets/Models alongside the OBJ
                std::filesystem::path mtlDestPath = destPath;
                mtlDestPath.replace_extension(".mtl");
                std::filesystem::copy_file(mtlPath, mtlDestPath, std::filesystem::copy_options::overwrite_existing);

                std::ifstream mtlFile(mtlPath);
                if (mtlFile.is_open()) {
                    std::string line;
                    ParsedMaterial currentMat{};
                    bool hasCurrent = false;

                    while (std::getline(mtlFile, line)) {
                        line = Trim(line);
                        if (line.empty() || line[0] == '#') continue;

                        std::stringstream ss(line);
                        std::string cmd;
                        ss >> cmd;

                        if (cmd == "newmtl") {
                            if (hasCurrent) {
                                mtlMaterials[currentMat.name] = currentMat;
                            }
                            currentMat = ParsedMaterial{};
                            std::string rest;
                            std::getline(ss, rest);
                            currentMat.name = Trim(rest);
                            hasCurrent = true;
                        } else if (hasCurrent) {
                            if (cmd == "map_Kd") {
                                std::string rest;
                                std::getline(ss, rest);
                                currentMat.albedoTex = Trim(rest);
                            } else if (cmd == "map_Bump" || cmd == "bump") {
                                std::string rest;
                                std::getline(ss, rest);
                                currentMat.normalTex = Trim(rest);
                            } else if (cmd == "map_Ks" || cmd == "map_Pm" || cmd == "map_Pr") {
                                std::string rest;
                                std::getline(ss, rest);
                                currentMat.metallicRoughnessTex = Trim(rest);
                            } else if (cmd == "map_Ka") {
                                std::string rest;
                                std::getline(ss, rest);
                                currentMat.aoTex = Trim(rest);
                            } else if (cmd == "map_Ke") {
                                std::string rest;
                                std::getline(ss, rest);
                                currentMat.emissiveTex = Trim(rest);
                            }
                        }
                    }
                    if (hasCurrent) {
                        mtlMaterials[currentMat.name] = currentMat;
                    }
                    LogInfo("Parsed " + std::to_string(mtlMaterials.size()) + " materials from MTL.");
                }
            } else {
                LogWarning("Companion MTL file not found: " + mtllibFilename);
            }
        }

        // Setup default/fallback material
        std::filesystem::create_directories("Assets/Materials");
        std::string defaultMatPath = "Assets/Materials/default.omnixmat";
        if (!std::filesystem::exists(defaultMatPath)) {
            OmnixMaterial dmat;
            dmat.name = "default";
            dmat.header.blendMode = 0;
            dmat.header.cullMode = 0;
            dmat.header.depthTest = 1;
            SerializeMaterial(dmat, defaultMatPath);
        }
        AssetHandle defaultMatHandle = registry->RegisterAsset(defaultMatPath, AssetType::Material);

        std::vector<AssetHandle> materialHandles;
        std::vector<AssetHandle> dependencies;

        // Process material slots
        for (const auto& slotName : materialSlots) {
            auto it = mtlMaterials.find(slotName);
            if (it == mtlMaterials.end()) {
                LogWarning("Material slot '" + slotName + "' details not found in MTL. Assigning default fallback.");
                materialHandles.push_back(defaultMatHandle);
                continue;
            }

            const auto& parsedMat = it->second;

            auto resolve_and_copy_texture = [&](const std::string& texFilename) -> std::string {
                if (texFilename.empty()) return "";
                std::filesystem::path srcTexPath = srcPath.parent_path() / texFilename;
                if (!std::filesystem::exists(srcTexPath)) {
                    srcTexPath = destPath.parent_path() / texFilename;
                }
                if (!std::filesystem::exists(srcTexPath)) {
                    LogWarning("Referenced texture file not found: " + texFilename);
                    return "";
                }

                std::filesystem::create_directories("Assets/Textures");
                std::filesystem::path destTexPath = std::filesystem::path("Assets/Textures") / srcTexPath.filename();

                if (!std::filesystem::exists(destTexPath)) {
                    std::filesystem::copy_file(srcTexPath, destTexPath, std::filesystem::copy_options::overwrite_existing);
                    LogInfo("Copied texture: " + srcTexPath.string() + " -> " + destTexPath.string());
                }

                std::string relTexPath = destTexPath.string();
                std::replace(relTexPath.begin(), relTexPath.end(), '\\', '/');

                // Register texture
                AssetHandle texHandle = registry->RegisterAsset(relTexPath, AssetType::Texture);
                dependencies.push_back(texHandle);

                // Compile to .omnixtex sidecar
                std::string texCachePath = "Cache/Textures/" + destTexPath.stem().string() + ".omnixtex";
                TextureMetadata texMeta;
                TextureImporter texImporter;
                if (texImporter.ImportTexture(relTexPath, texCachePath, texMeta, true)) {
                    auto* registeredMeta = const_cast<AssetMetadata*>(registry->GetMetadata(texHandle));
                    if (registeredMeta) {
                        registeredMeta->importedPath = texCachePath;
                        registeredMeta->isImported = true;
                        registeredMeta->isDirty = false;
                        registry->UpdateMetadata(*registeredMeta);
                    }
                }
                return relTexPath;
            };

            std::string albedoPath = resolve_and_copy_texture(parsedMat.albedoTex);
            std::string normalPath = resolve_and_copy_texture(parsedMat.normalTex);
            std::string metallicRoughnessPath = resolve_and_copy_texture(parsedMat.metallicRoughnessTex);
            std::string aoPath = resolve_and_copy_texture(parsedMat.aoTex);
            std::string emissivePath = resolve_and_copy_texture(parsedMat.emissiveTex);

            std::string matFilename = slotName + ".omnixmat";
            std::filesystem::path destMatPath = std::filesystem::path("Assets/Materials") / matFilename;

            OmnixMaterial dmat;
            dmat.name = slotName;
            dmat.albedoTexturePath = albedoPath;
            dmat.normalTexturePath = normalPath;
            dmat.metallicRoughnessTexturePath = metallicRoughnessPath;
            dmat.aoTexturePath = aoPath;
            dmat.emissiveTexturePath = emissivePath;
            dmat.header.blendMode = 0;
            dmat.header.cullMode = 0;
            dmat.header.depthTest = 1;

            if (!SerializeMaterial(dmat, destMatPath.string())) {
                LogError("Failed to serialize material: " + destMatPath.string());
                materialHandles.push_back(defaultMatHandle);
                continue;
            }

            std::string relMatPath = destMatPath.string();
            std::replace(relMatPath.begin(), relMatPath.end(), '\\', '/');

            // Register material
            AssetHandle matHandle = registry->RegisterAsset(relMatPath, AssetType::Material);
            materialHandles.push_back(matHandle);
            dependencies.push_back(matHandle);
            LogInfo("Generated and registered material: " + relMatPath);
        }

        // Compile mesh using MeshImporter
        std::filesystem::create_directories("Assets/Meshes");
        std::string cacheMeshPath = "Assets/Meshes/" + destPath.stem().string() + ".omnixmesh";

        MeshMetadata meshMeta;
        MeshImporter meshImporter;
        if (!meshImporter.ImportMesh(destPath.string(), cacheMeshPath, meshMeta, materialHandles, true)) {
            LogError("MeshImporter failed to compile: " + destPath.string());
            return "";
        }
        LogInfo("Successfully compiled mesh to: " + cacheMeshPath);

        std::string relPath = destPath.string();
        std::replace(relPath.begin(), relPath.end(), '\\', '/');

        // Register mesh
        AssetHandle meshHandle = registry->RegisterAsset(relPath, AssetType::Mesh);
        auto* registeredMeshMeta = const_cast<AssetMetadata*>(registry->GetMetadata(meshHandle));
        if (registeredMeshMeta) {
            registeredMeshMeta->importedPath = cacheMeshPath;
            registeredMeshMeta->isImported = true;
            registeredMeshMeta->isDirty = false;
            registeredMeshMeta->dependencies = dependencies;
            registry->UpdateMetadata(*registeredMeshMeta);
        }

        registry->SaveRegistry("AssetRegistry.json");
        LogInfo("Model import completed successfully: " + relPath);
        return relPath;

    } catch (const std::exception& e) {
        LogError("Exception during import: " + std::string(e.what()));
        return "";
    }
}

} // namespace eng::runtime
