#pragma once

#include <string>
#include <sstream>
#include <vector>
#include <iomanip>
#include <fstream>
#include <cmath>
#include <glm/glm.hpp>

class TextWriter
{
private:
    std::ofstream m_output;
    int m_indentLevel = 0;
    static const int INDENT_SIZE = 4;
    static const int FLOAT_PRECISION = 6;

    // Helper methods
    void WriteIndent();
    void WriteToken(const std::string& token);
    void WriteSpace();
    void WriteNewline();
    void IncreaseIndentLevel();
    void DecreaseIndentLevel();

public:
    TextWriter(const std::string& filename);
    ~TextWriter();

    // Main API methods
    void BeginScope(const std::string& name);
    void EndScope();
    void WriteKey(const std::string& key);
    
    // WriteValue overloads for different types
    void WriteValue(int value);
    void WriteValue(float value);
    void WriteValue(double value);
    void WriteValue(bool value);
    void WriteValue(const std::string& value);
    void WriteValue(const glm::vec3& vec);
    void WriteValue(const std::vector<float>& array);
    void WriteValue(const std::vector<int>& array);
    void WriteValue(const std::vector<bool>& array);
    void WriteValue(const std::vector<std::string>& array);
};
