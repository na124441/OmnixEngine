#pragma once

#include "Runtime/Package.h"
#include "Runtime/PackageManager.h"
#include <vector>
#include <string>
#include <cstdint>
#include <algorithm>

namespace eng::runtime {

    enum class CompressionType : uint32_t {
        None = 0,
        LZ4 = 1,
        Zlib = 2
    };

    inline uint32_t ComputeChecksum32(const uint8_t* data, size_t size) noexcept {
        uint32_t hash = 2166136261U;
        for (size_t i = 0; i < size; ++i) {
            hash ^= data[i];
            hash *= 16777619U;
        }
        return hash;
    }

    class PackageCompressor {
    public:
        static std::vector<uint8_t> CompressPayload(const std::vector<uint8_t>& rawData, CompressionType compType) {
            if (compType == CompressionType::None || rawData.empty()) {
                return rawData;
            }
            // RLE / XOR transform for test simulation
            std::vector<uint8_t> compressed = rawData;
            for (size_t i = 0; i < compressed.size(); ++i) {
                compressed[i] ^= 0x5A;
            }
            return compressed;
        }

        static std::vector<uint8_t> DecompressPayload(const std::vector<uint8_t>& compressedData, CompressionType compType) {
            if (compType == CompressionType::None || compressedData.empty()) {
                return compressedData;
            }
            // De-XOR transform
            std::vector<uint8_t> decompressed = compressedData;
            for (size_t i = 0; i < decompressed.size(); ++i) {
                decompressed[i] ^= 0x5A;
            }
            return decompressed;
        }
    };

    class PackageEncryptor {
    public:
        static std::vector<uint8_t> EncryptPayload(const std::vector<uint8_t>& data, uint32_t key = 0xDEADBEEF) {
            std::vector<uint8_t> encrypted = data;
            uint8_t keyByte = static_cast<uint8_t>(key & 0xFF);
            for (size_t i = 0; i < encrypted.size(); ++i) {
                encrypted[i] ^= keyByte;
            }
            return encrypted;
        }

        static std::vector<uint8_t> DecryptPayload(const std::vector<uint8_t>& encryptedData, uint32_t key = 0xDEADBEEF) {
            return EncryptPayload(encryptedData, key); // Symmetric XOR cipher
        }
    };

} // namespace eng::runtime
