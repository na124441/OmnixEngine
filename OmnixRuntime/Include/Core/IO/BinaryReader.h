#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace eng::core {

    class BinaryReader
    {
    public:
        BinaryReader() = default;
        ~BinaryReader() = default;

        bool LoadFromFile(const std::string& filepath);
        bool LoadFromMemory(const uint8_t* data, size_t size);
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

        // Standard helpers
        uint32_t ReadUInt32();
        uint64_t ReadUInt64();
        float ReadFloat();
        void ReadBytes(void* outData, size_t size);
        std::string ReadFixedString(size_t maxBytes);
        uint64_t Tell();

        size_t GetOffset() const { return m_Offset; }
        void Seek(size_t offset);
        size_t GetBufferSize() const { return m_Buffer.size(); }

    private:
        std::vector<uint8_t> m_Buffer;
        size_t m_Offset = 0;
    };

} // namespace eng::core
