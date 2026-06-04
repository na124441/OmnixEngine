#pragma once
#include <string>

enum class AssetType
{
    Unknown,
    Texture,
    Mesh,
    Material,
    Shader,
    Scene,
    Prefab,
    Audio
};

inline const char* AssetTypeToString(AssetType type) noexcept {
    switch (type) {
        case AssetType::Texture: return "Texture";
        case AssetType::Mesh: return "Mesh";
        case AssetType::Material: return "Material";
        case AssetType::Shader: return "Shader";
        case AssetType::Scene: return "Scene";
        case AssetType::Prefab: return "Prefab";
        case AssetType::Audio: return "Audio";
        default: return "Unknown";
    }
}

inline AssetType StringToAssetType(const std::string& str) noexcept {
    if (str == "Texture") return AssetType::Texture;
    if (str == "Mesh") return AssetType::Mesh;
    if (str == "Material") return AssetType::Material;
    if (str == "Shader") return AssetType::Shader;
    if (str == "Scene") return AssetType::Scene;
    if (str == "Prefab") return AssetType::Prefab;
    if (str == "Audio") return AssetType::Audio;
    return AssetType::Unknown;
}
