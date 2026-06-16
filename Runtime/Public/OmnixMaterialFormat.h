#pragma once
#include "Runtime/Public/FileHeader.h"
#include "Runtime/Public/AssetHandle.h"
#include "Runtime/Public/BinaryReader.h"
#include "Runtime/Public/BinaryWriter.h"
#include <vector>
#include <string>
#include <cstring>
#include <glm/glm.hpp>

#pragma pack(push, 1)

enum class MaterialTextureSlot : uint32_t
{
    Albedo,
    Normal,
    Metallic,
    Roughness,
    Emissive,
    AmbientOcclusion
};

struct MaterialTextureBinding
{
    MaterialTextureSlot slot;
    AssetHandle texture;

    bool operator==(const MaterialTextureBinding& o) const noexcept {
        return slot == o.slot && texture == o.texture;
    }
};

struct MaterialScalarParameter
{
    char name[64] = {0};
    float value = 0.0f;

    bool operator==(const MaterialScalarParameter& o) const noexcept {
        return std::strncmp(name, o.name, 64) == 0 && value == o.value;
    }
};

struct MaterialVectorParameter
{
    char name[64] = {0};
    float value[4] = {0.0f};

    bool operator==(const MaterialVectorParameter& o) const noexcept {
        return std::strncmp(name, o.name, 64) == 0 &&
               value[0] == o.value[0] && value[1] == o.value[1] &&
               value[2] == o.value[2] && value[3] == o.value[3];
    }
};

struct OmnixMaterialHeader
{
    FileHeader file;

    AssetHandle shader;
    uint32_t textureBindingCount = 0;
    uint32_t scalarParameterCount = 0;
    uint32_t vectorParameterCount = 0;

    uint32_t blendMode = 0;
    uint32_t cullMode = 0;
    uint32_t depthTest = 0;
};

#pragma pack(pop)

constexpr char MAGIC_MAT[8] = {'O', 'M', 'X', 'M', 'A', 'T', '\0', '\0'};
constexpr uint32_t OMNIX_MATERIAL_VERSION_MAJOR = 1;
constexpr uint32_t OMNIX_MATERIAL_VERSION_MINOR = 0;

struct OmnixMaterial
{
    OmnixMaterialHeader header;
    std::string name;
    std::vector<MaterialTextureBinding> textures;
    std::vector<MaterialScalarParameter> scalars;
    std::vector<MaterialVectorParameter> vectors;

    // Direct PNG texture paths (written/read as plain strings in the file).
    // These take priority over the texture binding AssetHandles in the renderer.
    std::string albedoTexturePath;  // e.g. "Assets/Textures/brick.png"
    std::string normalTexturePath;  // optional
    std::string metallicRoughnessTexturePath;
    std::string aoTexturePath;
    std::string emissiveTexturePath;

    glm::vec4 baseColorFactor{0.65f, 0.65f, 0.65f, 1.0f};
    float metallicFactor = 0.0f;
    float roughnessFactor = 0.6f;
    float normalScale = 1.0f;
    float emissiveStrength = 1.0f;
    uint32_t blendMode = 0; // 0=Opaque, 1=Mask, 2=Blend
    uint32_t shadingModel = 0; // 0=Lit, 1=Unlit
};

inline bool SerializeMaterial(const OmnixMaterial& mat, const std::string& filepath) {
    eng::runtime::BinaryWriter writer;
    writer.BeginFile(MAGIC_MAT, OMNIX_MATERIAL_VERSION_MAJOR, OMNIX_MATERIAL_VERSION_MINOR);

    writer.WriteU64(mat.header.shader.value);
    writer.WriteU32(mat.header.textureBindingCount);
    writer.WriteU32(mat.header.scalarParameterCount);
    writer.WriteU32(mat.header.vectorParameterCount);

    writer.WriteU32(mat.header.blendMode);
    writer.WriteU32(mat.header.cullMode);
    writer.WriteU32(mat.header.depthTest);

    writer.WriteString(mat.name);

    // Textures
    writer.WriteBytes(reinterpret_cast<const uint8_t*>(mat.textures.data()), mat.textures.size() * sizeof(MaterialTextureBinding));

    // Scalars
    writer.WriteBytes(reinterpret_cast<const uint8_t*>(mat.scalars.data()), mat.scalars.size() * sizeof(MaterialScalarParameter));

    // Vectors
    writer.WriteBytes(reinterpret_cast<const uint8_t*>(mat.vectors.data()), mat.vectors.size() * sizeof(MaterialVectorParameter));

    // Texture paths
    writer.WriteString(mat.albedoTexturePath);
    writer.WriteString(mat.normalTexturePath);
    writer.WriteString(mat.metallicRoughnessTexturePath);
    writer.WriteString(mat.aoTexturePath);
    writer.WriteString(mat.emissiveTexturePath);

    // Factors & settings
    writer.WriteFloat(mat.baseColorFactor.x);
    writer.WriteFloat(mat.baseColorFactor.y);
    writer.WriteFloat(mat.baseColorFactor.z);
    writer.WriteFloat(mat.baseColorFactor.w);
    writer.WriteFloat(mat.metallicFactor);
    writer.WriteFloat(mat.roughnessFactor);
    writer.WriteFloat(mat.normalScale);
    writer.WriteFloat(mat.emissiveStrength);
    writer.WriteU32(mat.blendMode);
    writer.WriteU32(mat.shadingModel);

    return writer.SaveToFile(filepath);
}

inline bool DeserializeMaterial(OmnixMaterial& mat, const std::string& filepath) {
    eng::runtime::BinaryReader reader;
    if (!reader.LoadFromFile(filepath)) {
        return false;
    }

    if (!reader.ValidateHeaderAndChecksum(MAGIC_MAT, OMNIX_MATERIAL_VERSION_MAJOR, OMNIX_MATERIAL_VERSION_MINOR)) {
        return false;
    }

    try {
        mat.header.shader = AssetHandle{reader.ReadU64()};
        mat.header.textureBindingCount = reader.ReadU32();
        mat.header.scalarParameterCount = reader.ReadU32();
        mat.header.vectorParameterCount = reader.ReadU32();

        mat.header.blendMode = reader.ReadU32();
        mat.header.cullMode = reader.ReadU32();
        mat.header.depthTest = reader.ReadU32();

        mat.name = reader.ReadString();

        // Textures
        mat.textures.resize(mat.header.textureBindingCount);
        if (mat.header.textureBindingCount > 0) {
            reader.ReadBytes(reinterpret_cast<uint8_t*>(mat.textures.data()), mat.header.textureBindingCount * sizeof(MaterialTextureBinding));
        }

        // Scalars
        mat.scalars.resize(mat.header.scalarParameterCount);
        if (mat.header.scalarParameterCount > 0) {
            reader.ReadBytes(reinterpret_cast<uint8_t*>(mat.scalars.data()), mat.header.scalarParameterCount * sizeof(MaterialScalarParameter));
        }

        // Vectors
        mat.vectors.resize(mat.header.vectorParameterCount);
        if (mat.header.vectorParameterCount > 0) {
            reader.ReadBytes(reinterpret_cast<uint8_t*>(mat.vectors.data()), mat.header.vectorParameterCount * sizeof(MaterialVectorParameter));
        }

        // Texture paths (safely bounds-checked)
        mat.albedoTexturePath = reader.ReadString();
        mat.normalTexturePath = reader.ReadString();

        if (reader.GetOffset() < reader.GetBufferSize()) {
            mat.metallicRoughnessTexturePath = reader.ReadString();
        } else {
            mat.metallicRoughnessTexturePath = "";
        }

        if (reader.GetOffset() < reader.GetBufferSize()) {
            mat.aoTexturePath = reader.ReadString();
        } else {
            mat.aoTexturePath = "";
        }

        if (reader.GetOffset() < reader.GetBufferSize()) {
            mat.emissiveTexturePath = reader.ReadString();
        } else {
            mat.emissiveTexturePath = "";
        }

        if (reader.GetOffset() < reader.GetBufferSize()) {
            mat.baseColorFactor.x = reader.ReadFloat();
            mat.baseColorFactor.y = reader.ReadFloat();
            mat.baseColorFactor.z = reader.ReadFloat();
            mat.baseColorFactor.w = reader.ReadFloat();
        } else {
            mat.baseColorFactor = glm::vec4(0.65f, 0.65f, 0.65f, 1.0f);
        }

        if (reader.GetOffset() < reader.GetBufferSize()) {
            mat.metallicFactor = reader.ReadFloat();
        } else {
            mat.metallicFactor = 0.0f;
        }

        if (reader.GetOffset() < reader.GetBufferSize()) {
            mat.roughnessFactor = reader.ReadFloat();
        } else {
            mat.roughnessFactor = 0.6f;
        }

        // Sanitization and clamping (Task 2.2)
        mat.metallicFactor = glm::clamp(mat.metallicFactor, 0.0f, 1.0f);
        mat.roughnessFactor = glm::clamp(mat.roughnessFactor, 0.04f, 1.0f);
        mat.baseColorFactor.w = glm::clamp(mat.baseColorFactor.w, 0.0f, 1.0f);

        if (reader.GetOffset() < reader.GetBufferSize()) {
            mat.normalScale = reader.ReadFloat();
        } else {
            mat.normalScale = 1.0f;
        }

        if (reader.GetOffset() < reader.GetBufferSize()) {
            mat.emissiveStrength = reader.ReadFloat();
        } else {
            mat.emissiveStrength = 1.0f;
        }

        if (reader.GetOffset() < reader.GetBufferSize()) {
            mat.blendMode = reader.ReadU32();
        } else {
            mat.blendMode = 0;
        }

        if (reader.GetOffset() < reader.GetBufferSize()) {
            mat.shadingModel = reader.ReadU32();
        } else {
            mat.shadingModel = 0;
        }

    } catch (const std::exception&) {
        return false;
    }

    return true;
}

inline bool DeserializeMaterialFromMemory(OmnixMaterial& mat, const uint8_t* data, size_t size) {
    eng::runtime::BinaryReader reader;
    if (!reader.LoadFromMemory(data, size)) {
        return false;
    }

    if (!reader.ValidateHeaderAndChecksum(MAGIC_MAT, OMNIX_MATERIAL_VERSION_MAJOR, OMNIX_MATERIAL_VERSION_MINOR)) {
        return false;
    }

    try {
        mat.header.shader = AssetHandle{reader.ReadU64()};
        mat.header.textureBindingCount = reader.ReadU32();
        mat.header.scalarParameterCount = reader.ReadU32();
        mat.header.vectorParameterCount = reader.ReadU32();

        mat.header.blendMode = reader.ReadU32();
        mat.header.cullMode = reader.ReadU32();
        mat.header.depthTest = reader.ReadU32();

        mat.name = reader.ReadString();

        // Textures
        mat.textures.resize(mat.header.textureBindingCount);
        if (mat.header.textureBindingCount > 0) {
            reader.ReadBytes(reinterpret_cast<uint8_t*>(mat.textures.data()), mat.header.textureBindingCount * sizeof(MaterialTextureBinding));
        }

        // Scalars
        mat.scalars.resize(mat.header.scalarParameterCount);
        if (mat.header.scalarParameterCount > 0) {
            reader.ReadBytes(reinterpret_cast<uint8_t*>(mat.scalars.data()), mat.header.scalarParameterCount * sizeof(MaterialScalarParameter));
        }

        // Vectors
        mat.vectors.resize(mat.header.vectorParameterCount);
        if (mat.header.vectorParameterCount > 0) {
            reader.ReadBytes(reinterpret_cast<uint8_t*>(mat.vectors.data()), mat.header.vectorParameterCount * sizeof(MaterialVectorParameter));
        }

        // Texture paths (safely bounds-checked)
        mat.albedoTexturePath = reader.ReadString();
        mat.normalTexturePath = reader.ReadString();

        if (reader.GetOffset() < reader.GetBufferSize()) {
            mat.metallicRoughnessTexturePath = reader.ReadString();
        } else {
            mat.metallicRoughnessTexturePath = "";
        }

        if (reader.GetOffset() < reader.GetBufferSize()) {
            mat.aoTexturePath = reader.ReadString();
        } else {
            mat.aoTexturePath = "";
        }

        if (reader.GetOffset() < reader.GetBufferSize()) {
            mat.emissiveTexturePath = reader.ReadString();
        } else {
            mat.emissiveTexturePath = "";
        }

        if (reader.GetOffset() < reader.GetBufferSize()) {
            mat.baseColorFactor.x = reader.ReadFloat();
            mat.baseColorFactor.y = reader.ReadFloat();
            mat.baseColorFactor.z = reader.ReadFloat();
            mat.baseColorFactor.w = reader.ReadFloat();
        } else {
            mat.baseColorFactor = glm::vec4(0.65f, 0.65f, 0.65f, 1.0f);
        }

        if (reader.GetOffset() < reader.GetBufferSize()) {
            mat.metallicFactor = reader.ReadFloat();
        } else {
            mat.metallicFactor = 0.0f;
        }

        if (reader.GetOffset() < reader.GetBufferSize()) {
            mat.roughnessFactor = reader.ReadFloat();
        } else {
            mat.roughnessFactor = 0.6f;
        }

        // Sanitization and clamping (Task 2.2)
        mat.metallicFactor = glm::clamp(mat.metallicFactor, 0.0f, 1.0f);
        mat.roughnessFactor = glm::clamp(mat.roughnessFactor, 0.04f, 1.0f);
        mat.baseColorFactor.w = glm::clamp(mat.baseColorFactor.w, 0.0f, 1.0f);

        if (reader.GetOffset() < reader.GetBufferSize()) {
            mat.normalScale = reader.ReadFloat();
        } else {
            mat.normalScale = 1.0f;
        }

        if (reader.GetOffset() < reader.GetBufferSize()) {
            mat.emissiveStrength = reader.ReadFloat();
        } else {
            mat.emissiveStrength = 1.0f;
        }

        if (reader.GetOffset() < reader.GetBufferSize()) {
            mat.blendMode = reader.ReadU32();
        } else {
            mat.blendMode = 0;
        }

        if (reader.GetOffset() < reader.GetBufferSize()) {
            mat.shadingModel = reader.ReadU32();
        } else {
            mat.shadingModel = 0;
        }

    } catch (const std::exception&) {
        return false;
    }

    return true;
}
