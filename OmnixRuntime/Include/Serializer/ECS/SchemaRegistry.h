// Maps component types to their schema.
// 1. On engine init, each component calls RegisterSchema.
// 2. Registry stores: ComponentTypeID -> ComponentSchema.
// 3. On lookup, return a const reference to prevent mutation.

#pragma once

#include "Serializer/ECS/ComponentSchema.h"
#include "Serializer/ECS/ComponentTypes.h"
#include "../../Core/Logger.h"
#include <map>
#include <string>
#include <cstring>
#include <vector>

class ComponentSchemaRegistry {
private:
    std::map<uint32_t, ComponentSchema> m_Schemas;

    static bool ValidateSchema(const ComponentSchema& schema) {
        if (!schema.componentName || strlen(schema.componentName) == 0) return false;
        if (schema.componentSize == 0) return false;
        if (!schema.fields || schema.fieldCount == 0) return false;

        for (int i = 0; i < schema.fieldCount; i++) {
            const FieldSchema& field = schema.fields[i];
            if (field.offset + field.size > schema.componentSize) {
                LOG_ERROR("Field offset out of bounds: %s::%s", schema.componentName, field.name);
                return false;
            }
            if (!field.name || strlen(field.name) == 0) return false;
        }
        return true;
    }

public:
    ComponentSchemaRegistry() = default;

    bool RegisterSchema(uint32_t componentTypeID, const ComponentSchema& schema) {
        if (m_Schemas.count(componentTypeID)) {
            LOG_ERROR("Component type %u already registered!", componentTypeID);
            return false;
        }
        if (!ValidateSchema(schema)) {
            LOG_ERROR("Invalid schema for component type %u", componentTypeID);
            return false;
        }
        m_Schemas[componentTypeID] = schema;
        LOG_INFO("Registered schema for component type %u", componentTypeID);
        return true;
    }

    [[nodiscard]] const ComponentSchema* GetSchema(uint32_t componentTypeID) const {
        auto it = m_Schemas.find(componentTypeID);
        if (it == m_Schemas.end()) {
            LOG_WARN("Schema not found for component type %u", componentTypeID);
            return nullptr;
        }
        return &it->second;
    }

    [[nodiscard]] bool HasSchema(uint32_t componentTypeID) const {
        return m_Schemas.count(componentTypeID);
    }

    void PrintAllSchemas() const {
        LOG_INFO("=== Registered Component Schemas ===");
        for (const auto& [typeID, schema] : m_Schemas) {
            LOG_INFO("Type ID: %u, Name: %s, Size: %zu bytes, Fields: %d",
                     typeID, schema.componentName, schema.componentSize, schema.fieldCount);
        }
        LOG_INFO("Total schemas: %zu", m_Schemas.size());
    }
};
