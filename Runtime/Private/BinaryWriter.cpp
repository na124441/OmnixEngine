#include "Runtime/Public/BinaryWriter.h"
#include "Runtime/Public/FileHeader.h"
#include "Runtime/Public/Checksum.h"
#include <fstream>
#include <cstring>

namespace eng::runtime {

    void BinaryWriter::BeginFile(const char magic[8], uint32_t versionMajor, uint32_t versionMinor) {
        m_Buffer.clear();
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
            m_Buffer.insert(m_Buffer.end(), data, data + size);
        }
    }

} // namespace eng::runtime
