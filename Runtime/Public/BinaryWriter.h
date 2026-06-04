#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace eng::runtime {

    class BinaryWriter
    {
    public:
        BinaryWriter() = default;
        ~BinaryWriter() = default;

        /**
         * @brief Starts serializing a new file with a placeholder FileHeader.
         */
        void BeginFile(const char magic[8], uint32_t versionMajor, uint32_t versionMinor);

        /**
         * @brief Writes the final buffer to disk. Computes checksum and updates the header before writing.
         */
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

        const std::vector<uint8_t>& GetBuffer() const { return m_Buffer; }
        std::vector<uint8_t>& GetBuffer() { return m_Buffer; }

    private:
        std::vector<uint8_t> m_Buffer;
    };

} // namespace eng::runtime
