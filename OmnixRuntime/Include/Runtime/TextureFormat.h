#pragma once
#include <cstdint>
#include <string>

enum class TextureSourceFormat : uint32_t
{
    Unknown,
    PNG,
    JPG,
    TGA,
    DDS,
    HDR
};

enum class TextureRuntimeFormat : uint32_t
{
    Unknown,
    RGBA8,
    RGB8,
    RGBA16F,
    BC1,
    BC3,
    BC5,
    BC7
};

enum class TextureCompressionType : uint32_t
{
    None,
    BC1,
    BC3,
    BC5,
    BC7,
    ASTC,
    ETC2
};

inline const char* TextureSourceFormatToString(TextureSourceFormat format) noexcept {
    switch (format) {
        case TextureSourceFormat::PNG: return "PNG";
        case TextureSourceFormat::JPG: return "JPG";
        case TextureSourceFormat::TGA: return "TGA";
        case TextureSourceFormat::DDS: return "DDS";
        case TextureSourceFormat::HDR: return "HDR";
        default: return "Unknown";
    }
}

inline TextureSourceFormat StringToTextureSourceFormat(const std::string& str) noexcept {
    if (str == "PNG") return TextureSourceFormat::PNG;
    if (str == "JPG") return TextureSourceFormat::JPG;
    if (str == "TGA") return TextureSourceFormat::TGA;
    if (str == "DDS") return TextureSourceFormat::DDS;
    if (str == "HDR") return TextureSourceFormat::HDR;
    return TextureSourceFormat::Unknown;
}

inline const char* TextureRuntimeFormatToString(TextureRuntimeFormat format) noexcept {
    switch (format) {
        case TextureRuntimeFormat::RGBA8: return "RGBA8";
        case TextureRuntimeFormat::RGB8: return "RGB8";
        case TextureRuntimeFormat::RGBA16F: return "RGBA16F";
        case TextureRuntimeFormat::BC1: return "BC1";
        case TextureRuntimeFormat::BC3: return "BC3";
        case TextureRuntimeFormat::BC5: return "BC5";
        case TextureRuntimeFormat::BC7: return "BC7";
        default: return "Unknown";
    }
}

inline TextureRuntimeFormat StringToTextureRuntimeFormat(const std::string& str) noexcept {
    if (str == "RGBA8") return TextureRuntimeFormat::RGBA8;
    if (str == "RGB8") return TextureRuntimeFormat::RGB8;
    if (str == "RGBA16F") return TextureRuntimeFormat::RGBA16F;
    if (str == "BC1") return TextureRuntimeFormat::BC1;
    if (str == "BC3") return TextureRuntimeFormat::BC3;
    if (str == "BC5") return TextureRuntimeFormat::BC5;
    if (str == "BC7") return TextureRuntimeFormat::BC7;
    return TextureRuntimeFormat::Unknown;
}
