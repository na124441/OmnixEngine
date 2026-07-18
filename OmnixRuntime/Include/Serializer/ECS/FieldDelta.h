#pragma once

#include <string>
#include <cstdint>

enum class FieldType {
    INT,
    FLOAT,
    DOUBLE,
    BOOL,
    VEC3,
    VEC4,
    QUAT,
    STRING,
    BLOB,
    UNKNOWN
};

struct FieldData {
    uint32_t id;
    FieldType type;
    std::string typeString;
    std::string value;
};