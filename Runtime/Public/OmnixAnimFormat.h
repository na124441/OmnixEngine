#pragma once
#include "Runtime/Public/FileHeader.h"
#include "Runtime/Public/OmnixMeshFormat.h" // reuse Vec3, Vec4
#include "Runtime/Public/BinaryReader.h"
#include "Runtime/Public/BinaryWriter.h"
#include <vector>
#include <string>

#pragma pack(push, 1)

struct Vector3Keyframe
{
    float time = 0.0f;
    Vec3 value;

    bool operator==(const Vector3Keyframe& o) const noexcept {
        return time == o.time && value == o.value;
    }
};

struct QuaternionKeyframe
{
    float time = 0.0f;
    Vec4 value;

    bool operator==(const QuaternionKeyframe& o) const noexcept {
        return time == o.time && value == o.value;
    }
};

struct BoneTrackHeader
{
    uint32_t boneIndex = 0;
    uint32_t positionKeyCount = 0;
    uint32_t rotationKeyCount = 0;
    uint32_t scaleKeyCount = 0;

    bool operator==(const BoneTrackHeader& o) const noexcept {
        return boneIndex == o.boneIndex && positionKeyCount == o.boneIndex &&
               positionKeyCount == o.positionKeyCount && rotationKeyCount == o.rotationKeyCount &&
               scaleKeyCount == o.scaleKeyCount;
    }
};

struct OmnixAnimHeader
{
    FileHeader file;

    float durationSeconds = 0.0f;
    float ticksPerSecond = 0.0f;

    uint32_t boneTrackCount = 0;
    uint32_t compressionType = 0;
    uint32_t hasRootMotion = 0;
};

#pragma pack(pop)

struct BoneTrack
{
    BoneTrackHeader header;
    std::string boneName;
    std::vector<Vector3Keyframe> positionKeys;
    std::vector<QuaternionKeyframe> rotationKeys;
    std::vector<Vector3Keyframe> scaleKeys;

    bool operator==(const BoneTrack& o) const noexcept {
        return header.boneIndex == o.header.boneIndex && boneName == o.boneName &&
               positionKeys == o.positionKeys && rotationKeys == o.rotationKeys && scaleKeys == o.scaleKeys;
    }
};

constexpr char MAGIC_ANIM[8] = {'O', 'M', 'X', 'A', 'N', 'I', 'M', '\0'};
constexpr uint32_t OMNIX_ANIM_VERSION_MAJOR = 1;
constexpr uint32_t OMNIX_ANIM_VERSION_MINOR = 0;

struct OmnixAnim
{
    OmnixAnimHeader header;
    std::string name;
    std::vector<BoneTrack> boneTracks;
};

inline bool SerializeAnim(const OmnixAnim& anim, const std::string& filepath) {
    eng::runtime::BinaryWriter writer;
    writer.BeginFile(MAGIC_ANIM, OMNIX_ANIM_VERSION_MAJOR, OMNIX_ANIM_VERSION_MINOR);

    writer.WriteF32(anim.header.durationSeconds);
    writer.WriteF32(anim.header.ticksPerSecond);
    writer.WriteU32(anim.header.boneTrackCount);
    writer.WriteU32(anim.header.compressionType);
    writer.WriteU32(anim.header.hasRootMotion);

    writer.WriteString(anim.name);

    for (const auto& track : anim.boneTracks) {
        writer.WriteU32(track.header.boneIndex);
        writer.WriteU32(static_cast<uint32_t>(track.positionKeys.size()));
        writer.WriteU32(static_cast<uint32_t>(track.rotationKeys.size()));
        writer.WriteU32(static_cast<uint32_t>(track.scaleKeys.size()));

        writer.WriteString(track.boneName);

        // Position Keys
        if (!track.positionKeys.empty()) {
            writer.WriteBytes(reinterpret_cast<const uint8_t*>(track.positionKeys.data()), track.positionKeys.size() * sizeof(Vector3Keyframe));
        }

        // Rotation Keys
        if (!track.rotationKeys.empty()) {
            writer.WriteBytes(reinterpret_cast<const uint8_t*>(track.rotationKeys.data()), track.rotationKeys.size() * sizeof(QuaternionKeyframe));
        }

        // Scale Keys
        if (!track.scaleKeys.empty()) {
            writer.WriteBytes(reinterpret_cast<const uint8_t*>(track.scaleKeys.data()), track.scaleKeys.size() * sizeof(Vector3Keyframe));
        }
    }

    return writer.SaveToFile(filepath);
}

inline bool DeserializeAnim(OmnixAnim& anim, const std::string& filepath) {
    eng::runtime::BinaryReader reader;
    if (!reader.LoadFromFile(filepath)) {
        return false;
    }

    if (!reader.ValidateHeaderAndChecksum(MAGIC_ANIM, OMNIX_ANIM_VERSION_MAJOR, OMNIX_ANIM_VERSION_MINOR)) {
        return false;
    }

    try {
        anim.header.durationSeconds = reader.ReadF32();
        anim.header.ticksPerSecond = reader.ReadF32();
        anim.header.boneTrackCount = reader.ReadU32();
        anim.header.compressionType = reader.ReadU32();
        anim.header.hasRootMotion = reader.ReadU32();

        anim.name = reader.ReadString();

        anim.boneTracks.resize(anim.header.boneTrackCount);
        for (uint32_t i = 0; i < anim.header.boneTrackCount; ++i) {
            auto& track = anim.boneTracks[i];
            track.header.boneIndex = reader.ReadU32();
            track.header.positionKeyCount = reader.ReadU32();
            track.header.rotationKeyCount = reader.ReadU32();
            track.header.scaleKeyCount = reader.ReadU32();

            track.boneName = reader.ReadString();

            // Position Keys
            track.positionKeys.resize(track.header.positionKeyCount);
            if (track.header.positionKeyCount > 0) {
                reader.ReadBytes(reinterpret_cast<uint8_t*>(track.positionKeys.data()), track.header.positionKeyCount * sizeof(Vector3Keyframe));
            }

            // Rotation Keys
            track.rotationKeys.resize(track.header.rotationKeyCount);
            if (track.header.rotationKeyCount > 0) {
                reader.ReadBytes(reinterpret_cast<uint8_t*>(track.rotationKeys.data()), track.header.rotationKeyCount * sizeof(QuaternionKeyframe));
            }

            // Scale Keys
            track.scaleKeys.resize(track.header.scaleKeyCount);
            if (track.header.scaleKeyCount > 0) {
                reader.ReadBytes(reinterpret_cast<uint8_t*>(track.scaleKeys.data()), track.header.scaleKeyCount * sizeof(Vector3Keyframe));
            }
        }

    } catch (const std::exception&) {
        return false;
    }

    return true;
}
