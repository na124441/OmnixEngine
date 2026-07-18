#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <typeindex>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <cstddef>

namespace eng::runtime {

    enum class PropertyFlags : uint32_t {
        None = 0,
        Edit = 1 << 0,
        Save = 1 << 1,
        Network = 1 << 2
    };

    inline PropertyFlags operator|(PropertyFlags a, PropertyFlags b) noexcept {
        return static_cast<PropertyFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }

    struct FieldMetadata {
        std::string name;
        std::string typeName;
        size_t offset = 0;
        size_t size = 0;
        PropertyFlags flags = PropertyFlags::None;

        std::function<void(void* instance, const void* value)> setter;
        std::function<void*(void* instance)> getter;
    };

    struct TypeMetadata {
        std::string name;
        size_t size = 0;
        std::unordered_map<std::string, FieldMetadata> fields;
    };

    /**
     * @class ReflectionRegistry
     * @brief A registry for struct and field metadata mapping types and field offsets dynamically (T1.1.9, T1.1.10).
     */
    class ReflectionRegistry {
    public:
        static ReflectionRegistry& Get() {
            static ReflectionRegistry instance;
            return instance;
        }

        template<typename StructType>
        void RegisterType(const std::string& name) {
            std::lock_guard<std::mutex> lock(m_Mutex);
            auto& meta = m_Types[typeid(StructType)];
            meta.name = name;
            meta.size = sizeof(StructType);
        }

        template<typename StructType, typename FieldType>
        void RegisterField(const std::string& fieldName,
                           const std::string& fieldTypeName,
                           size_t offset,
                           PropertyFlags flags) {
            std::lock_guard<std::mutex> lock(m_Mutex);
            auto& typeMeta = m_Types[typeid(StructType)];
            
            FieldMetadata field{};
            field.name = fieldName;
            field.typeName = fieldTypeName;
            field.offset = offset;
            field.size = sizeof(FieldType);
            field.flags = flags;

            field.setter = [offset](void* instance, const void* value) {
                *reinterpret_cast<FieldType*>(reinterpret_cast<char*>(instance) + offset) = *reinterpret_cast<const FieldType*>(value);
            };

            field.getter = [offset](void* instance) -> void* {
                return reinterpret_cast<void*>(reinterpret_cast<char*>(instance) + offset);
            };

            typeMeta.fields[fieldName] = field;
        }

        [[nodiscard]] const TypeMetadata* GetTypeMetadata(std::type_index typeIdx) const {
            std::lock_guard<std::mutex> lock(m_Mutex);
            auto it = m_Types.find(typeIdx);
            if (it != m_Types.end()) {
                return &it->second;
            }
            return nullptr;
        }

        template<typename StructType>
        [[nodiscard]] const TypeMetadata* GetTypeMetadata() const {
            return GetTypeMetadata(typeid(StructType));
        }

    private:
        ReflectionRegistry() = default;
        mutable std::mutex m_Mutex;
        std::unordered_map<std::type_index, TypeMetadata> m_Types;
    };

    // Reflection macros (T1.1.10)
    #define REFLECT_STRUCT_BEGIN(Type) \
        struct Type##Reflector { \
            Type##Reflector() { \
                eng::runtime::ReflectionRegistry::Get().RegisterType<Type>(#Type);

    #define REFLECT_FIELD(StructType, FieldType, FieldName, Flags) \
                eng::runtime::ReflectionRegistry::Get().RegisterField<StructType, FieldType>( \
                    #FieldName, #FieldType, offsetof(StructType, FieldName), Flags);

    #define REFLECT_STRUCT_END(Type) \
            } \
        }; \
        static Type##Reflector s_##Type##ReflectorInstance;

    // Get/Set named property functions (T1.1.11)
    template<typename StructType, typename FieldType>
    void SetProperty(StructType& instance, const std::string& name, const FieldType& value) {
        const auto* meta = ReflectionRegistry::Get().GetTypeMetadata<StructType>();
        if (!meta) {
            throw std::runtime_error("Type is not reflected!");
        }
        auto it = meta->fields.find(name);
        if (it == meta->fields.end()) {
            throw std::runtime_error("Field not found: " + name);
        }
        it->second.setter(&instance, &value);
    }

    template<typename StructType, typename FieldType>
    FieldType GetProperty(StructType& instance, const std::string& name) {
        const auto* meta = ReflectionRegistry::Get().GetTypeMetadata<StructType>();
        if (!meta) {
            throw std::runtime_error("Type is not reflected!");
        }
        auto it = meta->fields.find(name);
        if (it == meta->fields.end()) {
            throw std::runtime_error("Field not found: " + name);
        }
        return *reinterpret_cast<FieldType*>(it->second.getter(&instance));
    }

} // namespace eng::runtime
