#include <string>
#include <iostream>
#include <vector>
#include <stack>
#include <stdexcept>
#include "Serializer/Serialization/Normal/Binary/BinaryWriter.h"

BinaryWriter::BinaryWriter(const std::string& filename, BinaryWriter::Endianness endianness)
    : m_endianness(endianness)
{
    m_output.open(filename, std::ios::binary | std::ios::out);
    if (!m_output.is_open())
    {
        throw std::runtime_error("Failed to open file for writing: " + filename);
    }
}

BinaryWriter::~BinaryWriter()
{
    try
    {
        if (m_isDocumentOpen)
        {
            EndDocument();
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception in ~BinaryWriter: " << e.what() << std::endl;
    }

    if (m_output.is_open())
    {
        m_output.close();
    }
}

// Helper methods

uint32_t BinaryWriter::ConvertEndianness_U32(uint32_t value) const
{
    if (m_endianness == Endianness::Big)
    {
        return ((value & 0xFF000000) >> 24) |
               ((value & 0x00FF0000) >> 8) |
               ((value & 0x0000FF00) << 8) |
               ((value & 0x000000FF) << 24);
    }
    return value;  // Little endian - no conversion needed
}

uint32_t BinaryWriter::BitCastFloatToU32(float value) const
{
    uint32_t result;
    std::memcpy(&result, &value, sizeof(float));
    return result;
}

void BinaryWriter::WriteBytes(const uint8_t* data, size_t length)
{
    if (!m_output.write(reinterpret_cast<const char*>(data), length))
    {
        throw std::runtime_error("Failed to write bytes to output stream");
    }
    m_currentOffset += length;
}

int64_t BinaryWriter::GetCurrentStreamPosition() const
{
    return m_currentOffset;
}

void BinaryWriter::SeekTo(int64_t position)
{
    if (!m_output.seekp(position, std::ios::beg))
    {
        throw std::runtime_error("Failed to seek to position: " + std::to_string(position));
    }
    m_currentOffset = position;
}

// Algorithm implementations

// 1. BeginDocument()
void BinaryWriter::BeginDocument()
{
    if (m_isDocumentOpen)
    {
        throw std::runtime_error("Document already open");
    }

    m_currentOffset = 0;
    m_isDocumentOpen = true;

    // i. Write magic number (eg. 0x4C54253 = 'LTRS')
    WriteU32(MAGIC_NUMBER);

    // ii. Write format version (u16)
    WriteU16(FORMAT_VERSION);

    // iii. Write endianness flag
    uint8_t endiannessFlag = (m_endianness == Endianness::Little) ? 0 : 1;
    WriteU8(endiannessFlag);

    // iv. Reset internal state
    while (!m_scopeStack.empty())
    {
        m_scopeStack.pop();
    }
}

// 2. EndDocument()
void BinaryWriter::EndDocument()
{
    if (!m_isDocumentOpen)
    {
        throw std::runtime_error("No document open");
    }

    // i. Ensure all scopes are closed
    if (!m_scopeStack.empty())
    {
        throw std::runtime_error("Cannot end document: " + std::to_string(m_scopeStack.size()) +
                                 " scopes still open");
    }

    // ii. Flush output stream
    m_output.flush();
    m_isDocumentOpen = false;
}

// 3. BeginScope(scopeID)
void BinaryWriter::BeginScope(uint32_t scopeID)
{
    if (!m_isDocumentOpen)
    {
        throw std::runtime_error("No document open");
    }

    // i. WriteU32(scopeID)
    WriteU32(scopeID);

    // ii. Record current position as sizeOffset
    int64_t sizeOffset = GetCurrentStreamPosition();

    // iii. WriteU32(0) - Placeholder for scope size
    WriteU32(0);

    // iv. Push sizeOffset onto scope stack
    m_scopeStack.push(sizeOffset);
}

// 4. EndScope()
void BinaryWriter::EndScope()
{
    if (m_scopeStack.empty())
    {
        throw std::runtime_error("Cannot end scope: no scope is open");
    }

    // i. currentPosition = GetCurrentStreamPosition()
    int64_t currentPosition = GetCurrentStreamPosition();

    // ii. sizeOffset = Pop from scope stack
    int64_t sizeOffset = m_scopeStack.top();
    m_scopeStack.pop();

    // iii. scopeSize = currentPosition - (sizeOffset + sizeof(U32))
    uint32_t scopeSize = static_cast<uint32_t>(currentPosition - (sizeOffset + sizeof(uint32_t)));

    // iv. Seek to sizeOffset
    SeekTo(sizeOffset);

    // v. WriteU32(scopeSize)
    WriteU32(scopeSize);

    // vi. Seek to currentPosition
    SeekTo(currentPosition);
}

// 5. WriteU32(value)
void BinaryWriter::WriteU32(uint32_t value)
{
    // i. Convert value to fixed endianness
    uint32_t convertedValue = ConvertEndianness_U32(value);

    // ii & iii. Write 4 bytes to output stream and advance CurrentOffset
    uint8_t buffer[4];
    buffer[0] = (convertedValue >> 0) & 0xFF;
    buffer[1] = (convertedValue >> 8) & 0xFF;
    buffer[2] = (convertedValue >> 16) & 0xFF;
    buffer[3] = (convertedValue >> 24) & 0xFF;

    WriteBytes(buffer, 4);
}

// 6. WriteF32(value)
void BinaryWriter::WriteF32(float value)
{
    // i. Bit-Cast float to 32 bit IEEE 754
    uint32_t bitCasted = BitCastFloatToU32(value);

    // ii. Convert Endianness
    uint32_t convertedValue = ConvertEndianness_U32(bitCasted);

    // iii. Write 4 bytes
    uint8_t buffer[4];
    buffer[0] = (convertedValue >> 0) & 0xFF;
    buffer[1] = (convertedValue >> 8) & 0xFF;
    buffer[2] = (convertedValue >> 16) & 0xFF;
    buffer[3] = (convertedValue >> 24) & 0xFF;

    WriteBytes(buffer, 4);
}

// 7. WriteString(string)
void BinaryWriter::WriteString(const std::string& value)
{
    // i. WriteU32(length in bytes)
    uint32_t length = static_cast<uint32_t>(value.length());
    WriteU32(length);

    // ii. WriteBytes(string)
    if (length > 0)
    {
        WriteBytes(reinterpret_cast<const uint8_t*>(value.data()), length);
    }
}

// Additional write methods

void BinaryWriter::WriteU16(uint16_t value)
{
    uint16_t convertedValue = value;
    if (m_endianness == Endianness::Big)
    {
        convertedValue = ((value & 0xFF00) >> 8) | ((value & 0x00FF) << 8);
    }

    uint8_t buffer[2];
    buffer[0] = (convertedValue >> 0) & 0xFF;
    buffer[1] = (convertedValue >> 8) & 0xFF;

    WriteBytes(buffer, 2);
}

void BinaryWriter::WriteU8(uint8_t value)
{
    WriteBytes(&value, 1);
}

void BinaryWriter::WriteF64(double value)
{
    uint64_t bitCasted;
    std::memcpy(&bitCasted, &value, sizeof(double));

    // Convert endianness for 8 bytes
    if (m_endianness == Endianness::Big)
    {
        bitCasted = ((bitCasted & 0xFF00000000000000ULL) >> 56) |
                    ((bitCasted & 0x00FF000000000000ULL) >> 40) |
                    ((bitCasted & 0x0000FF0000000000ULL) >> 24) |
                    ((bitCasted & 0x000000FF00000000ULL) >> 8) |
                    ((bitCasted & 0x00000000FF000000ULL) << 8) |
                    ((bitCasted & 0x0000000000FF0000ULL) << 24) |
                    ((bitCasted & 0x000000000000FF00ULL) << 40) |
                    ((bitCasted & 0x00000000000000FFULL) << 56);
    }

    uint8_t buffer[8];
    for (int i = 0; i < 8; ++i)
    {
        buffer[i] = (bitCasted >> (i * 8)) & 0xFF;
    }

    WriteBytes(buffer, 8);
}

void BinaryWriter::WriteBytes(const std::vector<uint8_t>& data)
{
    if (!data.empty())
    {
        WriteBytes(data.data(), data.size());
    }
}

void BinaryWriter::WriteU64(uint64_t value)
{
    uint64_t convertedValue = value;
    if (m_endianness == Endianness::Big)
    {
        convertedValue = ((value & 0xFF00000000000000ULL) >> 56) |
                         ((value & 0x00FF000000000000ULL) >> 40) |
                         ((value & 0x0000FF0000000000ULL) >> 24) |
                         ((value & 0x000000FF00000000ULL) >> 8) |
                         ((value & 0x00000000FF000000ULL) << 8) |
                         ((value & 0x0000000000FF0000ULL) << 24) |
                         ((value & 0x000000000000FF00ULL) << 40) |
                         ((value & 0x00000000000000FFULL) << 56);
    }

    uint8_t buffer[8];
    for (int i = 0; i < 8; ++i)
    {
        buffer[i] = (convertedValue >> (i * 8)) & 0xFF;
    }

    WriteBytes(buffer, 8);
}

void BinaryWriter::WriteI32(int32_t value)
{
    uint32_t unsignedVal;
    std::memcpy(&unsignedVal, &value, sizeof(int32_t));
    WriteU32(unsignedVal);
}

void BinaryWriter::WriteBool(bool value)
{
    WriteU8(value ? 1 : 0);
}