#include "Runtime/AssetRegistry.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <unordered_set>

namespace eng::runtime {

    AssetHandle GenerateAssetUUID(const std::string& path, AssetType type) noexcept {
        std::string canonical = path;
        // Normalize backslashes to forward slashes
        std::replace(canonical.begin(), canonical.end(), '\\', '/');
        // Convert to lowercase to ensure case insensitivity on Windows paths
        std::transform(canonical.begin(), canonical.end(), canonical.begin(), [](unsigned char c) {
            return std::tolower(c);
        });
        
        std::string toHash = canonical + "|" + std::to_string(static_cast<int>(type));
        uint64_t hashVal = HashFNV1a(toHash);
        if (hashVal == 0) {
            hashVal = 1;
        }
        return AssetHandle{hashVal};
    }

    AssetHandle AssetRegistry::RegisterAsset(const std::string& path, AssetType type) {
        AssetHandle handle = GenerateAssetUUID(path, type);
        
        auto it = m_Assets.find(handle);
        if (it != m_Assets.end()) {
            return handle;
        }

        AssetMetadata meta;
        meta.handle = handle;
        meta.type = type;
        meta.sourcePath = path;
        meta.isDirty = true;
        meta.isImported = false;
        meta.sourceMissing = false;

        m_Assets[handle] = std::move(meta);
        return handle;
    }

    const AssetMetadata* AssetRegistry::GetMetadata(AssetHandle handle) const {
        auto it = m_Assets.find(handle);
        if (it != m_Assets.end()) {
            return &it->second;
        }
        return nullptr;
    }

    bool AssetRegistry::Contains(AssetHandle handle) const {
        return m_Assets.find(handle) != m_Assets.end();
    }

    void AssetRegistry::UpdateMetadata(const AssetMetadata& metadata) {
        m_Assets[metadata.handle] = metadata;
    }

    void AssetRegistry::Clear() noexcept {
        m_Assets.clear();
    }

    bool AssetRegistry::SaveRegistry(const std::string& filepath) {
        nlohmann::json root = nlohmann::json::object();
        nlohmann::json assetsArray = nlohmann::json::array();

        for (const auto& [handle, meta] : m_Assets) {
            nlohmann::json assetJson = nlohmann::json::object();
            assetJson["handle"] = handle.value;
            assetJson["type"] = AssetTypeToString(meta.type);
            assetJson["sourcePath"] = meta.sourcePath;
            assetJson["importedPath"] = meta.importedPath;
            assetJson["importTimestamp"] = meta.importTimestamp;
            assetJson["isImported"] = meta.isImported;
            assetJson["isDirty"] = meta.isDirty;
            assetJson["sourceMissing"] = meta.sourceMissing;

            nlohmann::json depsArray = nlohmann::json::array();
            for (const auto& dep : meta.dependencies) {
                depsArray.push_back(dep.value);
            }
            assetJson["dependencies"] = depsArray;

            assetsArray.push_back(assetJson);
        }
        root["assets"] = assetsArray;

        std::ofstream file(filepath);
        if (!file.is_open()) {
            return false;
        }
        file << root.dump(4);
        return true;
    }

    bool AssetRegistry::LoadRegistry(const std::string& filepath) {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            return false;
        }

        nlohmann::json root;
        try {
            file >> root;
        } catch (const std::exception&) {
            return false;
        }

        if (!root.contains("assets") || !root["assets"].is_array()) {
            return false;
        }

        m_Assets.clear();
        for (const auto& assetJson : root["assets"]) {
            AssetMetadata meta;
            if (assetJson.contains("handle") && assetJson["handle"].is_number()) {
                meta.handle = AssetHandle{assetJson["handle"].get<uint64_t>()};
            }
            if (assetJson.contains("type") && assetJson["type"].is_string()) {
                meta.type = StringToAssetType(assetJson["type"].get<std::string>());
            }
            if (assetJson.contains("sourcePath") && assetJson["sourcePath"].is_string()) {
                meta.sourcePath = assetJson["sourcePath"].get<std::string>();
            }
            if (assetJson.contains("importedPath") && assetJson["importedPath"].is_string()) {
                meta.importedPath = assetJson["importedPath"].get<std::string>();
            }
            if (assetJson.contains("importTimestamp") && assetJson["importTimestamp"].is_number()) {
                meta.importTimestamp = assetJson["importTimestamp"].get<uint64_t>();
            }
            if (assetJson.contains("isImported") && assetJson["isImported"].is_boolean()) {
                meta.isImported = assetJson["isImported"].get<bool>();
            }
            if (assetJson.contains("isDirty") && assetJson["isDirty"].is_boolean()) {
                meta.isDirty = assetJson["isDirty"].get<bool>();
            }
            if (assetJson.contains("sourceMissing") && assetJson["sourceMissing"].is_boolean()) {
                meta.sourceMissing = assetJson["sourceMissing"].get<bool>();
            }
            if (assetJson.contains("dependencies") && assetJson["dependencies"].is_array()) {
                for (const auto& depJson : assetJson["dependencies"]) {
                    if (depJson.is_number()) {
                        meta.dependencies.push_back(AssetHandle{depJson.get<uint64_t>()});
                    }
                }
            }
            m_Assets[meta.handle] = std::move(meta);
        }
        return true;
    }

    void AssetRegistry::ScanProjectAssets() {
        std::vector<std::string> dirsToScan = {
            "Assets/Models",
            "Assets/Meshes",
            "Assets/Materials",
            "Assets/Textures"
        };

        std::unordered_set<uint64_t> scannedHandles;

        for (const auto& dirPathStr : dirsToScan) {
            if (!std::filesystem::exists(dirPathStr)) {
                continue;
            }

            for (const auto& entry : std::filesystem::recursive_directory_iterator(dirPathStr)) {
                if (!entry.is_regular_file()) {
                    continue;
                }

                std::string ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
                    return std::tolower(c);
                });

                AssetType type = AssetType::Unknown;
                if (ext == ".obj") {
                    type = AssetType::Mesh;
                } else if (ext == ".omnixmat") {
                    type = AssetType::Material;
                } else if (ext == ".png" || ext == ".jpg" || ext == ".jpeg") {
                    type = AssetType::Texture;
                }

                if (type != AssetType::Unknown) {
                    std::string relPath = entry.path().string();
                    std::replace(relPath.begin(), relPath.end(), '\\', '/');

                    AssetHandle handle = RegisterAsset(relPath, type);
                    scannedHandles.insert(handle.value);

                    auto it = m_Assets.find(handle);
                    if (it != m_Assets.end()) {
                        it->second.sourceMissing = false;
                    }
                }
            }
        }

        for (auto& [handle, meta] : m_Assets) {
            if (scannedHandles.find(handle.value) == scannedHandles.end()) {
                meta.sourceMissing = true;
            }
        }

        SaveRegistry("AssetRegistry.json");
    }

} // namespace eng::runtime
