#include "Runtime/Public/BinaryReader.h"
#include "Runtime/Public/FileHeader.h"
#include "Runtime/Public/Checksum.h"
#include <fstream>
#include <cstring>
#include <stdexcept>

namespace eng::runtime {

    bool BinaryReader::LoadFromFile(const std::string& filepath) {
        std::ifstream file(filepath, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            return false;
        }
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        m_Buffer.resize(static_cast<size_t>(size));
        if (size > 0) {
            if (!file.read(reinterpret_cast<char*>(m_Buffer.data()), size)) {
                return false;
            }
        }

        m_Offset = 0;
        return true;
    }

    bool BinaryReader::LoadFromMemory(const uint8_t* data, size_t size) {
        if (size > 0 && data != nullptr) {
            m_Buffer.assign(data, data + size);
        } else {
            m_Buffer.clear();
        }
        m_Offset = 0;
        return true;
    }

    bool BinaryReader::ValidateHeaderAndChecksum(const char expectedMagic[8], uint32_t expectedVersionMajor, uint32_t expectedVersionMinor) {
        if (m_Buffer.size() < sizeof(FileHeader)) {
            return false;
        }
        const FileHeader* header = reinterpret_cast<const FileHeader*>(m_Buffer.data());

        // Check magic (char[8])
        if (std::memcmp(header->magic, expectedMagic, 8) != 0) {
            return false;
        }

        // Check version mismatch
        if (header->versionMajor != expectedVersionMajor) {
            return false;
        }

        // Check file size consistency
        if (header->fileSize != m_Buffer.size()) {
            return false;
        }

        // Validate checksum
        uint64_t computed = ComputeFileChecksum(m_Buffer.data(), m_Buffer.size());
        if (computed != header->checksum) {
            return false;
        }

        // Advance past FileHeader
        m_Offset = sizeof(FileHeader);
        return true;
    }

    uint8_t BinaryReader::ReadU8() {
        uint8_t val = 0;
        ReadBytes(reinterpret_cast<uint8_t*>(&val), sizeof(val));
        return val;
    }

    uint16_t BinaryReader::ReadU16() {
        uint16_t val = 0;
        ReadBytes(reinterpret_cast<uint8_t*>(&val), sizeof(val));
        return val;
    }

    uint32_t BinaryReader::ReadU32() {
        uint32_t val = 0;
        ReadBytes(reinterpret_cast<uint8_t*>(&val), sizeof(val));
        return val;
    }

    uint64_t BinaryReader::ReadU64() {
        uint64_t val = 0;
        ReadBytes(reinterpret_cast<uint8_t*>(&val), sizeof(val));
        return val;
    }

    int32_t BinaryReader::ReadI32() {
        int32_t val = 0;
        ReadBytes(reinterpret_cast<uint8_t*>(&val), sizeof(val));
        return val;
    }

    float BinaryReader::ReadF32() {
        float val = 0.0f;
        ReadBytes(reinterpret_cast<uint8_t*>(&val), sizeof(val));
        return val;
    }

    double BinaryReader::ReadF64() {
        double val = 0.0;
        ReadBytes(reinterpret_cast<uint8_t*>(&val), sizeof(val));
        return val;
    }

    bool BinaryReader::ReadBool() {
        return ReadU8() != 0;
    }

    std::string BinaryReader::ReadString() {
        uint32_t len = ReadU32();
        std::string str(len, '\0');
        if (len > 0) {
            ReadBytes(reinterpret_cast<uint8_t*>(&str[0]), len);
        }
        return str;
    }

    void BinaryReader::ReadBytes(uint8_t* outData, size_t size) {
        if (size == 0) return;
        if (m_Offset + size > m_Buffer.size()) {
            throw std::runtime_error("BinaryReader out-of-bounds read");
        }
        std::memcpy(outData, m_Buffer.data() + m_Offset, size);
        m_Offset += size;
    }

    void BinaryReader::Seek(size_t offset) {
        if (offset > m_Buffer.size()) {
            throw std::runtime_error("BinaryReader out-of-bounds seek");
        }
        m_Offset = offset;
    }

} // namespace eng::runtime
