#include "Runtime/Public/BinaryWriter.h"
#include "Runtime/Public/FileHeader.h"
#include "Runtime/Public/Checksum.h"
#include <fstream>
#include <cstring>

namespace eng::runtime {

    void BinaryWriter::BeginFile(const char magic[8], uint32_t versionMajor, uint32_t versionMinor) {
        m_Buffer.clear();
        m_Offset = 0;
        FileHeader header;
        std::memcpy(header.magic, magic, 8);
        header.versionMajor = versionMajor;
        header.versionMinor = versionMinor;
        header.checksum = 0;
        header.fileSize = 0;
        WriteBytes(reinterpret_cast<const uint8_t*>(&header), sizeof(header));
    }

    bool BinaryWriter::SaveToFile(const std::string& filepath) {
        if (m_Buffer.size() < sizeof(FileHeader)) {
            return false;
        }

        // Update fileSize in the header
        FileHeader* header = reinterpret_cast<FileHeader*>(m_Buffer.data());
        header->fileSize = static_cast<uint64_t>(m_Buffer.size());

        // Compute checksum of the entire buffer
        header->checksum = ComputeFileChecksum(m_Buffer.data(), m_Buffer.size());

        // Write buffer to file
        std::ofstream file(filepath, std::ios::binary);
        if (!file.is_open()) {
            return false;
        }
        file.write(reinterpret_cast<const char*>(m_Buffer.data()), m_Buffer.size());
        return true;
    }

    void BinaryWriter::WriteU8(uint8_t val) {
        m_Buffer.push_back(val);
    }

    void BinaryWriter::WriteU16(uint16_t val) {
        WriteBytes(reinterpret_cast<const uint8_t*>(&val), sizeof(val));
    }

    void BinaryWriter::WriteU32(uint32_t val) {
        WriteBytes(reinterpret_cast<const uint8_t*>(&val), sizeof(val));
    }

    void BinaryWriter::WriteU64(uint64_t val) {
        WriteBytes(reinterpret_cast<const uint8_t*>(&val), sizeof(val));
    }

    void BinaryWriter::WriteI32(int32_t val) {
        WriteBytes(reinterpret_cast<const uint8_t*>(&val), sizeof(val));
    }

    void BinaryWriter::WriteF32(float val) {
        WriteBytes(reinterpret_cast<const uint8_t*>(&val), sizeof(val));
    }

    void BinaryWriter::WriteF64(double val) {
        WriteBytes(reinterpret_cast<const uint8_t*>(&val), sizeof(val));
    }

    void BinaryWriter::WriteBool(bool val) {
        WriteU8(val ? 1 : 0);
    }

    void BinaryWriter::WriteString(const std::string& str) {
        WriteU32(static_cast<uint32_t>(str.size()));
        if (!str.empty()) {
            WriteBytes(reinterpret_cast<const uint8_t*>(str.data()), str.size());
        }
    }

    void BinaryWriter::WriteBytes(const uint8_t* data, size_t size) {
        if (size > 0 && data != nullptr) {
            if (m_Offset + size > m_Buffer.size()) {
                m_Buffer.resize(m_Offset + size);
            }
            std::memcpy(m_Buffer.data() + m_Offset, data, size);
            m_Offset += size;
        }
    }

    void BinaryWriter::WriteUInt32(uint32_t value) {
        WriteU32(value);
    }

    void BinaryWriter::WriteUInt64(uint64_t value) {
        WriteU64(value);
    }

    void BinaryWriter::WriteFloat(float value) {
        WriteF32(value);
    }

    void BinaryWriter::WriteBytes(const void* data, size_t size) {
        WriteBytes(static_cast<const uint8_t*>(data), size);
    }

    void BinaryWriter::WriteFixedString(const std::string& value, size_t maxBytes) {
        std::vector<char> temp(maxBytes, '\0');
        size_t copyLen = std::min(value.size(), maxBytes - 1);
        std::memcpy(temp.data(), value.data(), copyLen);
        WriteBytes(reinterpret_cast<const uint8_t*>(temp.data()), maxBytes);
    }

    uint64_t BinaryWriter::Tell() {
        return static_cast<uint64_t>(m_Offset);
    }

    void BinaryWriter::Seek(uint64_t position) {
        m_Offset = static_cast<size_t>(position);
    }

} // namespace eng::runtime
