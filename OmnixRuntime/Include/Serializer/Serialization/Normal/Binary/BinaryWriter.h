#pragma once

#include <string>
#include <vector>
#include <stack>
#include <fstream>
#include <cstdint>
#include <cstring>
#include <iostream>

class BinaryWriter
{
public:
    enum class Endianness
    {
        Little,
        Big
    };

private:
    std::ofstream m_output;
    int64_t m_currentOffset = 0;
    Endianness m_endianness = Endianness::Little;
    std::stack<int64_t> m_scopeStack;
    bool m_isDocumentOpen = false;

    static constexpr uint32_t MAGIC_NUMBER = 0x4C54253;  // 'LTRS'
    static constexpr uint16_t FORMAT_VERSION = 1;

    // Helper methods
    uint64_t ConvertEndianness_U64(uint64_t value) const;
    uint32_t ConvertEndianness_U32(uint32_t value) const;
    uint16_t ConvertEndianness_U16(uint16_t value) const;
    uint32_t BitCastFloatToU32(float value) const;
    uint64_t BitCastDoubleToU64(double value) const;
    int64_t GetCurrentStreamPosition() const;
    void SeekTo(int64_t position);

public:
    BinaryWriter(const std::string& filename, Endianness endianness = Endianness::Little);
    ~BinaryWriter();

    // Main API methods
    void BeginDocument();
    void EndDocument();
    void BeginScope(uint32_t scopeID);
    void EndScope();

    // Write methods
    void WriteU64(uint64_t value);
    void WriteU32(uint32_t value);
    void WriteU16(uint16_t value);
    void WriteU8(uint8_t value);
    void WriteI32(int32_t value);
    void WriteF32(float value);
    void WriteF64(double value);
    void WriteString(const std::string& value);
    void WriteBytes(const std::vector<uint8_t>& data);
    void WriteBytes(const uint8_t* data, size_t length);
    void WriteBool(bool value);

    // Utility methods
    Endianness GetEndianness() const { return m_endianness; }
    bool IsDocumentOpen() const { return m_isDocumentOpen; }
};