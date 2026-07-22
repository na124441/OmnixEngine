#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace eng::core {

    class BinaryWriter
    {
    public:
        BinaryWriter() = default;
        ~BinaryWriter() = default;

        void BeginFile(const char magic[8], uint32_t versionMajor, uint32_t versionMinor);
        bool SaveToFile(const std::string& filepath);

        // Serialization functions
        void WriteU8(uint8_t val);
        void WriteU16(uint16_t val);
        void WriteU32(uint32_t val);
        void WriteU64(uint64_t val);
        void WriteI32(int32_t val);
        void WriteF32(float val);
        void WriteF64(double val);
        void WriteBool(bool val);
        void WriteString(const std::string& str);
        void WriteBytes(const uint8_t* data, size_t size);

        // Standard helpers
        void WriteUInt32(uint32_t value);
        void WriteUInt64(uint64_t value);
        void WriteFloat(float value);
        void WriteBytes(const void* data, size_t size);
        void WriteFixedString(const std::string& value, size_t maxBytes);
        uint64_t Tell();
        void Seek(uint64_t position);

        const std::vector<uint8_t>& GetBuffer() const { return m_Buffer; }
        std::vector<uint8_t>& GetBuffer() { return m_Buffer; }

    private:
        std::vector<uint8_t> m_Buffer;
        size_t m_Offset = 0;
    };

} // namespace eng::core
