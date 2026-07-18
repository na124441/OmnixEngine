//           ByteStream ---> Typed Values ---> Structural Traversal
// 1.BeginDocument()
//  i.ReadU32() --> magicNumber
//  ii.ReadU16() --> formatVersion
//  iii.ReadU8() --> endiannessFlag
//  iv.Set ByteOrder Accordingly
//  v.Set Scope Stack
//  vi.Return Error if magicNumber or formatVersion invalid
// 
// 2. EndDocument()
//  i. Ensure no unclosed scopes remain
//  ii.Ensure InputStream at EOF
// 
// 3. BeginScope(expectedScopeID)
//  i.ReadU32() --> scopeID
//  ii.If scopeID != expectedScopeID --> Error
//  iii.ReadU32() --> scopeSize
//  iv. ScopeEnd = CurrentOffset + scopesSize
//  v.Push(scopeID , scopeEnd) onto scope stack
// 
// 4. EndScope()
//  i.Pop top scope
//  ii.If CurrentOffset != scopeEnd --> Error
// 
// 5. SkipScope()
//  i.ReadU32() --> scopeID
//  ii.ReadU32() --> scopeSize
//  iii.Seek forward by scopeSize bytes
//  
// 6. ReadU32()
//  i.Read 4 bytes 
//  ii.Convert from ByteOrder
//  iii.Advance CurrentOffset
//  iv.Return value
// 
// 7. ReadF32()
//  i.Read 4 bytes
//  ii.Convert endianess
//  iii.Bit-Cast to float
//  iv.Return value
// 
// 8.ReadString()
//  i.ReadU32() --> length
//  ii.ReadBytes(length) 
//  iii.Construct string
// 
//
#pragma once

#include <string>
#include <vector>
#include <stack>
#include <fstream>
#include <cstdint>
#include <cstring>
#include <iostream>

class BinaryReader
{
public:
    enum class Endianness
    {
        Little,
        Big
    };

private:
    struct ScopeInfo
    {
        uint32_t scopeID;
        int64_t scopeEnd;

        ScopeInfo(uint32_t id, int64_t end) : scopeID(id), scopeEnd(end) {}
    };

    std::ifstream m_input;
    int64_t m_currentOffset = 0;
    Endianness m_endianness = Endianness::Little;
    std::stack<ScopeInfo> m_scopeStack;
    bool m_isDocumentOpen = false;

    static constexpr uint32_t MAGIC_NUMBER = 0x4C54253;  // 'LTRS'
    static constexpr uint16_t FORMAT_VERSION = 1;

    // Helper methods
    uint64_t ConvertEndianness_U64(uint64_t value) const;
    uint32_t ConvertEndianness_U32(uint32_t value) const;
    uint16_t ConvertEndianness_U16(uint16_t value) const;
    float BitCastU32ToFloat(uint32_t value) const;
    double BitCastU64ToDouble(uint64_t value) const;
    int64_t GetCurrentStreamPosition() const;
    void SeekTo(int64_t position);
    bool IsAtEOF() const;

public:
    BinaryReader(const std::string& filename);
    ~BinaryReader();

    // Main API methods
    void BeginDocument();
    void EndDocument();
    void BeginScope(uint32_t expectedScopeID);
    void EndScope();
    void SkipScope();
    uint32_t PeekScopeID();

    // Read methods
    uint64_t ReadU64();
    uint32_t ReadU32();
    uint16_t ReadU16();
    uint8_t ReadU8();
    int32_t ReadI32();
    float ReadF32();
    double ReadF64();
    std::string ReadString();
    std::vector<uint8_t> ReadBytes(size_t length);
    void ReadBytes(uint8_t* data, size_t length); // Move this up from private
    bool ReadBool();

    // Utility methods
    Endianness GetEndianness() const { return m_endianness; }
    bool IsDocumentOpen() const { return m_isDocumentOpen; }
    int64_t GetCurrentOffset() const { return m_currentOffset; }
};