//1.BeginDocument()
//	i. Read IndentLevel to 0
//	ii.Reset IsAtLineStart to true
//  iii. Optionally write header comments
// 
//2. EndDocument()
//	i. Flush OutputStream
//  ii. Ensure no  unclassed scope remain
// 
//3. BeginScope(name)
//	i. ReadTokens() --> tokenName
//  ii. If tokenName != expectedName --> Error
//  iii. ReadToken() --> tokenBrace
//  iv. If tokenBrace != "{" --> Error
//  v. Push expectedName onto scope stack
// 
// 4. EndScope()
//	i. ReadToken() --> token
//  ii. IF token != "}" --> Error
//  ii, Pop expectedName from scope stack
// 
// 5. ReadKey(key)
//  i. ReadToken() --> tokenKey
//  ii. If tokenKey != expectedKey --> Error
// 
// 6. ReadValue(type)
//  i.ReadToken() --> rawToken
//  ii. Convert rawToken to requested type
//  iii. If conversion fails --> Error
//  iv  Return value
// 
// 7. ReadValues(type , count)
//  i.values = empty list
//  ii. Repeat count times: --> values.add(ReadValue(type))
//  iii . Return values
// 
// 
// 
//
#pragma once

#include <string>
#include <sstream>
#include <fstream>
#include <stack>
#include <vector>
#include <glm/glm.hpp>
#include <stdexcept>

class TextReader
{
private:
    std::ifstream m_input;
    int m_indentLevel = 0;
    bool m_isAtLineStart = true;
    std::stack<std::string> m_scopeStack;
    std::string m_currentToken;
    std::istringstream m_tokenStream;
    static const int INDENT_SIZE = 4;

    // Helper methods
    void SkipWhitespace(bool skipNewlines = false);
    void SkipComments();
    bool ReadToken(std::string& token);
    void ValidateToken(const std::string& actual, const std::string& expected, const std::string& context);
    void ThrowError(const std::string& message);

public:
    TextReader(const std::string& filename);
    ~TextReader();

    // Main API methods
    void BeginDocument();
    void EndDocument();
    void BeginScope(const std::string& expectedName);
    void EndScope();
    void ReadKey(const std::string& expectedKey);

    // ReadValue for different types
    int ReadValue_Int();
    float ReadValue_Float();
    double ReadValue_Double();
    bool ReadValue_Bool();
    std::string ReadValue_String();
    glm::vec3 ReadValue_Vec3();

    // ReadValues for arrays
    std::vector<int> ReadValues_Int(size_t count);
    std::vector<float> ReadValues_Float(size_t count);
    std::vector<double> ReadValues_Double(size_t count);
    std::vector<bool> ReadValues_Bool(size_t count);
    std::vector<std::string> ReadValues_String(size_t count);

    // Generic type template
    template<typename T>
    T ReadValue();

    template<typename T>
    std::vector<T> ReadValues(size_t count);
};