#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace eng::runtime {

    class BinaryReader
    {
    public:
        BinaryReader() = default;
        ~BinaryReader() = default;

        /**
         * @brief Loads a binary file from disk into memory buffer.
         */
        bool LoadFromFile(const std::string& filepath);

        /**
         * @brief Loads binary data from raw memory buffer.
         */
        bool LoadFromMemory(const uint8_t* data, size_t size);

        /**
         * @brief Validates magic number, version mismatch, and FNV-1a checksum.
         * Moves read pointer past the FileHeader if successful.
         */
        bool ValidateHeaderAndChecksum(const char expectedMagic[8], uint32_t expectedVersionMajor, uint32_t expectedVersionMinor);

        // Deserialization functions
        uint8_t ReadU8();
        uint16_t ReadU16();
        uint32_t ReadU32();
        uint64_t ReadU64();
        int32_t ReadI32();
        float ReadF32();
        double ReadF64();
        bool ReadBool();
        std::string ReadString();
        void ReadBytes(uint8_t* outData, size_t size);

        size_t GetOffset() const { return m_Offset; }
        void Seek(size_t offset);
        size_t GetBufferSize() const { return m_Buffer.size(); }

    private:
        std::vector<uint8_t> m_Buffer;
        size_t m_Offset = 0;
    };

} // namespace eng::runtime
