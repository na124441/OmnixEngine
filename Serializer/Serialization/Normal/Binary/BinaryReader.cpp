#include <stdexcept>
#include <fstream>
#include <cstring>
#include <vector>
#include <string>
#include "BinaryReader.h"


BinaryReader::BinaryReader(const std::string& filename)
{
    m_input.open(filename, std::ios::binary | std::ios::in);
    if (!m_input.is_open())
    {
        throw std::runtime_error("Failed to open file for reading: " + filename);
    }
}

BinaryReader::~BinaryReader()
{
    if (m_input.is_open())
    {
        m_input.close();
    }
}

// Helper methods

uint32_t BinaryReader::ConvertEndianness_U32(uint32_t value) const
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

uint16_t BinaryReader::ConvertEndianness_U16(uint16_t value) const
{
    if (m_endianness == Endianness::Big)
    {
        return ((value & 0xFF00) >> 8) | ((value & 0x00FF) << 8);
    }
    return value;
}

float BinaryReader::BitCastU32ToFloat(uint32_t value) const
{
    float result;
    std::memcpy(&result, &value, sizeof(float));
    return result;
}

void BinaryReader::ReadBytes(uint8_t* data, size_t length)
{
    if (!m_input.read(reinterpret_cast<char*>(data), length))
    {
        throw std::runtime_error("Failed to read " + std::to_string(length) + " bytes from input stream");
    }
    m_currentOffset += length;
}

int64_t BinaryReader::GetCurrentStreamPosition() const
{
    return m_currentOffset;
}

void BinaryReader::SeekTo(int64_t position)
{
    if (!m_input.seekg(position, std::ios::beg))
    {
        throw std::runtime_error("Failed to seek to position: " + std::to_string(position));
    }
    m_currentOffset = position;
}

bool BinaryReader::IsAtEOF() const
{
    return m_input.eof();
}

// Algorithm implementations

// 1. BeginDocument()
void BinaryReader::BeginDocument()
{
    if (m_isDocumentOpen)
    {
        throw std::runtime_error("Document already open");
    }

    m_currentOffset = 0;
    m_isDocumentOpen = true;

    // i. ReadU32() --> magicNumber
    uint32_t magicNumber = ReadU32();

    // vi. Return Error if magicNumber invalid
    if (magicNumber != MAGIC_NUMBER)
    {
        throw std::runtime_error("Invalid magic number: 0x" + std::to_string(magicNumber) +
            " (expected 0x" + std::to_string(MAGIC_NUMBER) + ")");
    }

    // ii. ReadU16() --> formatVersion
    uint16_t formatVersion = ReadU16();

    // vi. Return Error if formatVersion invalid
    if (formatVersion != FORMAT_VERSION)
    {
        throw std::runtime_error("Unsupported format version: " + std::to_string(formatVersion) +
            " (expected " + std::to_string(FORMAT_VERSION) + ")");
    }

    // iii. ReadU8() --> endiannessFlag
    uint8_t endiannessFlag = ReadU8();

    // iv. Set ByteOrder Accordingly
    m_endianness = (endiannessFlag == 0) ? Endianness::Little : Endianness::Big;

    // v. Set Scope Stack (reset if not empty)
    while (!m_scopeStack.empty())
    {
        m_scopeStack.pop();
    }
}

// 2. EndDocument()
void BinaryReader::EndDocument()
{
    if (!m_isDocumentOpen)
    {
        throw std::runtime_error("No document open");
    }

    // i. Ensure no unclosed scopes remain
    if (!m_scopeStack.empty())
    {
        throw std::runtime_error("Cannot end document: " + std::to_string(m_scopeStack.size()) +
            " scopes still open");
    }

    // ii. Ensure InputStream at EOF
    m_input.peek();  // Peek to set EOF flag if at end
    if (!IsAtEOF())
    {
        // Optional: Allow reading past EOF without strict error
        // throw std::runtime_error("Document ended but stream is not at EOF");
    }

    m_isDocumentOpen = false;
}

// 3. BeginScope(expectedScopeID)
void BinaryReader::BeginScope(uint32_t expectedScopeID)
{
    if (!m_isDocumentOpen)
    {
        throw std::runtime_error("No document open");
    }

    // i. ReadU32() --> scopeID
    uint32_t scopeID = ReadU32();

    // ii. If scopeID != expectedScopeID --> Error
    if (scopeID != expectedScopeID)
    {
        throw std::runtime_error("Scope ID mismatch: expected 0x" + std::to_string(expectedScopeID) +
            ", got 0x" + std::to_string(scopeID));
    }

    // iii. ReadU32() --> scopeSize
    uint32_t scopeSize = ReadU32();

    // iv. ScopeEnd = CurrentOffset + scopesSize
    int64_t scopeEnd = GetCurrentStreamPosition() + scopeSize;

    // v. Push(scopeID, scopeEnd) onto scope stack
    m_scopeStack.push(ScopeInfo(scopeID, scopeEnd));
}

// 4. EndScope()
void BinaryReader::EndScope()
{
    if (m_scopeStack.empty())
    {
        throw std::runtime_error("Cannot end scope: no scope is open");
    }

    // i. Pop top scope
    ScopeInfo scopeInfo = m_scopeStack.top();
    m_scopeStack.pop();

    // ii. If CurrentOffset != scopeEnd --> Error
    if (GetCurrentStreamPosition() != scopeInfo.scopeEnd)
    {
        throw std::runtime_error("Scope end position mismatch: expected " + std::to_string(scopeInfo.scopeEnd) +
            ", current offset " + std::to_string(GetCurrentStreamPosition()));
    }
}

// 5. SkipScope()
void BinaryReader::SkipScope()
{
    // i. ReadU32() --> scopeID
    uint32_t scopeID = ReadU32();

    // ii. ReadU32() --> scopeSize
    uint32_t scopeSize = ReadU32();

    // iii. Seek forward by scopeSize bytes
    SeekTo(GetCurrentStreamPosition() + scopeSize);
}

// 6. ReadU32()
uint32_t BinaryReader::ReadU32()
{
    uint8_t buffer[4];

    // i. Read 4 bytes
    ReadBytes(buffer, 4);

    // Reconstruct value from bytes
    uint32_t value = (static_cast<uint32_t>(buffer[0]) << 0) |
        (static_cast<uint32_t>(buffer[1]) << 8) |
        (static_cast<uint32_t>(buffer[2]) << 16) |
        (static_cast<uint32_t>(buffer[3]) << 24);

    // ii. Convert from ByteOrder
    uint32_t convertedValue = ConvertEndianness_U32(value);

    // iii & iv. Advance CurrentOffset (done in ReadBytes) and Return value
    return convertedValue;
}

// 7. ReadF32()
float BinaryReader::ReadF32()
{
    uint8_t buffer[4];

    // i. Read 4 bytes
    ReadBytes(buffer, 4);

    // Reconstruct value from bytes
    uint32_t value = (static_cast<uint32_t>(buffer[0]) << 0) |
        (static_cast<uint32_t>(buffer[1]) << 8) |
        (static_cast<uint32_t>(buffer[2]) << 16) |
        (static_cast<uint32_t>(buffer[3]) << 24);

    // ii. Convert endianness
    uint32_t convertedValue = ConvertEndianness_U32(value);

    // iii. Bit-Cast to float
    float result = BitCastU32ToFloat(convertedValue);

    // iv. Return value
    return result;
}

// 8. ReadString()
std::string BinaryReader::ReadString()
{
    // i. ReadU32() --> length
    uint32_t length = ReadU32();

    if (length == 0)
    {
        return "";
    }

    // ii. ReadBytes(length)
    std::vector<uint8_t> buffer(length);
    ReadBytes(buffer.data(), length);

    // iii. Construct string
    std::string result(reinterpret_cast<const char*>(buffer.data()), length);
    return result;
}

// Additional read methods

uint16_t BinaryReader::ReadU16()
{
    uint8_t buffer[2];
    ReadBytes(buffer, 2);

    uint16_t value = (static_cast<uint16_t>(buffer[0]) << 0) |
        (static_cast<uint16_t>(buffer[1]) << 8);

    return ConvertEndianness_U16(value);
}

uint8_t BinaryReader::ReadU8()
{
    uint8_t buffer[1];
    ReadBytes(buffer, 1);
    return buffer[0];
}

double BinaryReader::ReadF64()
{
    uint8_t buffer[8];
    ReadBytes(buffer, 8);

    uint64_t value = (static_cast<uint64_t>(buffer[0]) << 0) |
        (static_cast<uint64_t>(buffer[1]) << 8) |
        (static_cast<uint64_t>(buffer[2]) << 16) |
        (static_cast<uint64_t>(buffer[3]) << 24) |
        (static_cast<uint64_t>(buffer[4]) << 32) |
        (static_cast<uint64_t>(buffer[5]) << 40) |
        (static_cast<uint64_t>(buffer[6]) << 48) |
        (static_cast<uint64_t>(buffer[7]) << 56);

    // Convert endianness for 8 bytes
    if (m_endianness == Endianness::Big)
    {
        value = ((value & 0xFF00000000000000ULL) >> 56) |
            ((value & 0x00FF000000000000ULL) >> 40) |
            ((value & 0x0000FF0000000000ULL) >> 24) |
            ((value & 0x000000FF00000000ULL) >> 8) |
            ((value & 0x00000000FF000000ULL) << 8) |
            ((value & 0x0000000000FF0000ULL) << 24) |
            ((value & 0x000000000000FF00ULL) << 40) |
            ((value & 0x00000000000000FFULL) << 56);
    }

    double result;
    std::memcpy(&result, &value, sizeof(double));
    return result;
}

std::vector<uint8_t> BinaryReader::ReadBytes(size_t length)
{
    std::vector<uint8_t> buffer(length);
    if (length > 0)
    {
        ReadBytes(buffer.data(), length);
    }
    return buffer;
}

uint64_t BinaryReader::ReadU64()
{
    uint8_t buffer[8];
    ReadBytes(buffer, 8);

    uint64_t value = (static_cast<uint64_t>(buffer[0]) << 0) |
        (static_cast<uint64_t>(buffer[1]) << 8) |
        (static_cast<uint64_t>(buffer[2]) << 16) |
        (static_cast<uint64_t>(buffer[3]) << 24) |
        (static_cast<uint64_t>(buffer[4]) << 32) |
        (static_cast<uint64_t>(buffer[5]) << 40) |
        (static_cast<uint64_t>(buffer[6]) << 48) |
        (static_cast<uint64_t>(buffer[7]) << 56);

    // Convert endianness for 8 bytes
    if (m_endianness == Endianness::Big)
    {
        value = ((value & 0xFF00000000000000ULL) >> 56) |
            ((value & 0x00FF000000000000ULL) >> 40) |
            ((value & 0x0000FF0000000000ULL) >> 24) |
            ((value & 0x000000FF00000000ULL) >> 8) |
            ((value & 0x00000000FF000000ULL) << 8) |
            ((value & 0x0000000000FF0000ULL) << 24) |
            ((value & 0x000000000000FF00ULL) << 40) |
            ((value & 0x00000000000000FFULL) << 56);
    }

    return value;
}

int32_t BinaryReader::ReadI32()
{
    uint32_t uVal = ReadU32();
    int32_t val;
    std::memcpy(&val, &uVal, sizeof(int32_t));
    return val;
}

bool BinaryReader::ReadBool()
{
    return ReadU8() != 0;
}