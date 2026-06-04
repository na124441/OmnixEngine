#include "TextReader.h"
#include <stdexcept>

TextReader::TextReader(const std::string& filename)
{
    m_input.open(filename, std::ios::in);
    if (!m_input.is_open())
    {
        throw std::runtime_error("Failed to open file: " + filename);
    }
}

TextReader::~TextReader()
{
    if (m_input.is_open())
    {
        m_input.close();
    }
}

// Helper methods

void TextReader::SkipWhitespace(bool skipNewlines)
{
    char ch;
    while (m_input.get(ch))
    {
        if (ch == '\n')
        {
            if (!skipNewlines)
            {
                m_input.putback(ch);
                m_isAtLineStart = true;
                return;
            }
            m_isAtLineStart = true;
        }
        else if (ch != ' ' && ch != '\t' && ch != '\r')
        {
            m_input.putback(ch);
            m_isAtLineStart = false;
            return;
        }
    }
}

void TextReader::SkipComments()
{
    char ch;
    if (m_input.peek() == '/')
    {
        m_input.get(ch); // consume '/'
        if (m_input.peek() == '/')
        {
            // Skip line comment
            while (m_input.get(ch) && ch != '\n');
            m_isAtLineStart = true;
        }
        else if (m_input.peek() == '*')
        {
            // Skip block comment
            m_input.get(ch); // consume '*'
            while (m_input.get(ch))
            {
                if (ch == '*' && m_input.peek() == '/')
                {
                    m_input.get(ch); // consume '/'
                    break;
                }
            }
        }
        else
        {
            m_input.putback(ch);
        }
    }
}

bool TextReader::ReadToken(std::string& token)
{
    SkipWhitespace(true);
    SkipComments();
    SkipWhitespace(true);

    token.clear();
    char ch;

    // Handle quoted strings
    if (m_input.peek() == '"')
    {
        m_input.get(ch); // consume opening quote
        while (m_input.get(ch))
        {
            if (ch == '"')
            {
                token.push_back(ch);
                return true;
            }
            if (ch == '\\')
            {
                token.push_back(ch);
                if (m_input.get(ch))
                {
                    token.push_back(ch);
                }
            }
            else
            {
                token.push_back(ch);
            }
        }
        return false; // Unterminated string
    }

    // Handle regular tokens
    while (m_input.get(ch))
    {
        if (std::isspace(ch) || ch == '{' || ch == '}')
        {
            if (!token.empty())
            {
                m_input.putback(ch);
                return true;
            }
            if (ch == '{' || ch == '}')
            {
                token += ch;
                return true;
            }
        }
        else
        {
            token += ch;
        }
    }

    return !token.empty();
}

void TextReader::ValidateToken(const std::string& actual, const std::string& expected, const std::string& context)
{
    if (actual != expected)
    {
        ThrowError("Expected '" + expected + "' for " + context + ", got '" + actual + "'");
    }
}

void TextReader::ThrowError(const std::string& message)
{
    throw std::runtime_error("TextReader Error: " + message);
}

// Algorithm implementations

// 1. BeginDocument()
void TextReader::BeginDocument()
{
    m_indentLevel = 0;          // i
    m_isAtLineStart = true;     // ii
    // iii. Optionally write header comments (skip newlines and comments at start)
    SkipWhitespace(true);
    SkipComments();
}

// 2. EndDocument()
void TextReader::EndDocument()
{
    // i. Flush OutputStream (already flushed by closing)
    // ii. Ensure no unclassed scope remain
    if (!m_scopeStack.empty())
    {
        ThrowError("Document ended with unclosed scopes remaining");
    }
}

// 3. BeginScope(name)
void TextReader::BeginScope(const std::string& expectedName)
{
    std::string tokenName;
    if (!ReadToken(tokenName))  // i
    {
        ThrowError("Expected scope name '" + expectedName + "', reached end of file");
    }

    ValidateToken(tokenName, expectedName, "scope name");  // ii

    std::string tokenBrace;
    if (!ReadToken(tokenBrace))  // iii
    {
        ThrowError("Expected '{' after scope name '" + expectedName + "', reached end of file");
    }

    ValidateToken(tokenBrace, "{", "scope opening");  // iv

    m_scopeStack.push(expectedName);  // v
    m_indentLevel++;
}

// 4. EndScope()
void TextReader::EndScope()
{
    std::string token;
    if (!ReadToken(token))  // i
    {
        ThrowError("Expected '}', reached end of file");
    }

    ValidateToken(token, "}", "scope closing");  // ii

    if (m_scopeStack.empty())  // ii (Pop)
    {
        ThrowError("Unexpected '}': no matching scope opening");
    }

    m_scopeStack.pop();
    m_indentLevel--;
}

// 5. ReadKey(key)
void TextReader::ReadKey(const std::string& expectedKey)
{
    std::string tokenKey;
    if (!ReadToken(tokenKey))  // i
    {
        ThrowError("Expected key '" + expectedKey + "', reached end of file");
    }

    ValidateToken(tokenKey, expectedKey, "key");  // ii
}

// 6. ReadValue(type) - Type-specific implementations

int TextReader::ReadValue_Int()
{
    std::string rawToken;
    if (!ReadToken(rawToken))
    {
        ThrowError("Expected integer value, reached end of file");
    }

    try
    {
        return std::stoi(rawToken);
    }
    catch (const std::exception& e)
    {
        ThrowError("Failed to convert '" + rawToken + "' to integer: " + e.what());
        return 0;
    }
}

float TextReader::ReadValue_Float()
{
    std::string rawToken;
    if (!ReadToken(rawToken))
    {
        ThrowError("Expected float value, reached end of file");
    }

    try
    {
        return std::stof(rawToken);
    }
    catch (const std::exception& e)
    {
        ThrowError("Failed to convert '" + rawToken + "' to float: " + e.what());
        return 0.0f;
    }
}

double TextReader::ReadValue_Double()
{
    std::string rawToken;
    if (!ReadToken(rawToken))
    {
        ThrowError("Expected double value, reached end of file");
    }

    try
    {
        return std::stod(rawToken);
    }
    catch (const std::exception& e)
    {
        ThrowError("Failed to convert '" + rawToken + "' to double: " + e.what());
        return 0.0;
    }
}

bool TextReader::ReadValue_Bool()
{
    std::string rawToken;
    if (!ReadToken(rawToken))
    {
        ThrowError("Expected boolean value, reached end of file");
    }

    if (rawToken == "true")
    {
        return true;
    }
    else if (rawToken == "false")
    {
        return false;
    }
    else
    {
        ThrowError("Failed to convert '" + rawToken + "' to boolean (expected 'true' or 'false')");
        return false;
    }
}

std::string TextReader::ReadValue_String()
{
    std::string rawToken;
    if (!ReadToken(rawToken))
    {
        ThrowError("Expected string value, reached end of file");
    }

    // Remove surrounding quotes and unescape
    if (rawToken.front() == '"' && rawToken.back() == '"')
    {
        std::string unescaped;
        for (size_t i = 1; i < rawToken.length() - 1; ++i)
        {
            if (rawToken[i] == '\\' && i + 1 < rawToken.length() - 1)
            {
                switch (rawToken[i + 1])
                {
                case 'n': unescaped += '\n'; ++i; break;
                case 'r': unescaped += '\r'; ++i; break;
                case 't': unescaped += '\t'; ++i; break;
                case '\\': unescaped += '\\'; ++i; break;
                case '"': unescaped += '"'; ++i; break;
                default: unescaped += rawToken[i]; break;
                }
            }
            else
            {
                unescaped += rawToken[i];
            }
        }
        return unescaped;
    }

    return rawToken;
}

glm::vec3 TextReader::ReadValue_Vec3()
{
    float x = ReadValue_Float();
    float y = ReadValue_Float();
    float z = ReadValue_Float();
    return glm::vec3(x, y, z);
}

// 7. ReadValues(type, count)

std::vector<int> TextReader::ReadValues_Int(size_t count)
{
    std::vector<int> values;
    for (size_t i = 0; i < count; ++i)
    {
        values.push_back(ReadValue_Int());
    }
    return values;
}

std::vector<float> TextReader::ReadValues_Float(size_t count)
{
    std::vector<float> values;
    for (size_t i = 0; i < count; ++i)
    {
        values.push_back(ReadValue_Float());
    }
    return values;
}

std::vector<double> TextReader::ReadValues_Double(size_t count)
{
    std::vector<double> values;
    for (size_t i = 0; i < count; ++i)
    {
        values.push_back(ReadValue_Double());
    }
    return values;
}

std::vector<bool> TextReader::ReadValues_Bool(size_t count)
{
    std::vector<bool> values;
    for (size_t i = 0; i < count; ++i)
    {
        values.push_back(ReadValue_Bool());
    }
    return values;
}

std::vector<std::string> TextReader::ReadValues_String(size_t count)
{
    std::vector<std::string> values;
    for (size_t i = 0; i < count; ++i)
    {
        values.push_back(ReadValue_String());
    }
    return values;
}