#include "TextWriter.h"
#include <sstream>
#include <iomanip>

TextWriter::TextWriter(const std::string& filename)
{
    m_output.open(filename, std::ios::out);
    if (!m_output.is_open())
    {
        throw std::runtime_error("Failed to open file: " + filename);
    }
}

TextWriter::~TextWriter()
{
    if (m_output.is_open())
    {
        m_output.close();
    }
}

// Helper methods implementation
void TextWriter::WriteIndent()
{
    for (int i = 0; i < m_indentLevel * INDENT_SIZE; ++i)
    {
        m_output << ' ';
    }
}

void TextWriter::WriteToken(const std::string& token)
{
    m_output << token;
}

void TextWriter::WriteSpace()
{
    m_output << ' ';
}

void TextWriter::WriteNewline()
{
    m_output << '\n';
}

void TextWriter::IncreaseIndentLevel()
{
    ++m_indentLevel;
}

void TextWriter::DecreaseIndentLevel()
{
    if (m_indentLevel > 0)
    {
        --m_indentLevel;
    }
}

// Algorithm implementations

// 1. BeginScope(name)
void TextWriter::BeginScope(const std::string& name)
{
    WriteIndent();              // i
    WriteToken(name);           // ii
    WriteNewline();             // iii
    IncreaseIndentLevel();       // iv
}

// 2. EndScope()
void TextWriter::EndScope()
{
    DecreaseIndentLevel();       // i
    WriteIndent();              // ii
    WriteToken("}");            // iii
    WriteNewline();             // iv
}

// 3. WriteKey(key)
void TextWriter::WriteKey(const std::string& key)
{
    WriteIndent();              // i
    WriteToken(key);            // ii
    WriteSpace();               // iii
}

// 4. WriteValue - Canonical conversion for primitives

void TextWriter::WriteValue(int value)
{
    // Canonical Rule a: int --> Decimal
    WriteToken(std::to_string(value));
    WriteSpace();
}

void TextWriter::WriteValue(float value)
{
    // Canonical Rule b: float --> Fixed point with 6 decimal places
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(FLOAT_PRECISION) << value;
    WriteToken(oss.str());
    WriteSpace();
}

void TextWriter::WriteValue(double value)
{
    // Canonical Rule b: double --> Fixed point with 6 decimal places
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(FLOAT_PRECISION) << value;
    WriteToken(oss.str());
    WriteSpace();
}

void TextWriter::WriteValue(bool value)
{
    // Canonical Rule c: bool --> "true" or "false"
    WriteToken(value ? "true" : "false");
    WriteSpace();
}

void TextWriter::WriteValue(const std::string& value)
{
    // Canonical Rule d: string --> Quoted string with escape sequences
    std::string escaped = "\"";
    for (char c : value)
    {
        switch (c)
        {
            case '\\': escaped += "\\\\"; break;
            case '"':  escaped += "\\\""; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default:   escaped += c; break;
        }
    }
    escaped += "\"";
    WriteToken(escaped);
    WriteSpace();
}

// 5. WriteValue for vec3
void TextWriter::WriteValue(const glm::vec3& vec)
{
    // For each element in order: WriteValue(element)
    WriteValue(vec.x);
    WriteValue(vec.y);
    WriteValue(vec.z);
}

// 5. WriteValue for arrays

void TextWriter::WriteValue(const std::vector<float>& array)
{
    // For each element in order: WriteValue(element)
    for (float element : array)
    {
        WriteValue(element);
    }
}

void TextWriter::WriteValue(const std::vector<int>& array)
{
    // For each element in order: WriteValue(element)
    for (int element : array)
    {
        WriteValue(element);
    }
}

void TextWriter::WriteValue(const std::vector<bool>& array)
{
    // For each element in order: WriteValue(element)
    for (bool element : array)
    {
        WriteValue(element);
    }
}

void TextWriter::WriteValue(const std::vector<std::string>& array)
{
    // For each element in order: WriteValue(element)
    for (const std::string& element : array)
    {
        WriteValue(element);
    }
}