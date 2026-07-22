#pragma once
#include "Core/Memory/IAllocator.h"
#include "Core/Containers/StringView.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ostream>
#include <utility>
#include <algorithm>
#include <cassert>
#include <stdexcept>

namespace eng::containers {

    class String {
    public:
        using iterator = char*;
        using const_iterator = const char*;

        String() : String(eng::memory::GetDefaultAllocator()) {}

        explicit String(eng::memory::IAllocator* allocator)
            : m_Allocator(allocator ? allocator : eng::memory::GetDefaultAllocator())
            , m_Data(nullptr)
            , m_Size(0)
            , m_Capacity(0)
        {
            // Always allocate space for null-terminator
            m_Capacity = 1;
            m_Data = static_cast<char*>(m_Allocator->Allocate(m_Capacity * sizeof(char), alignof(char)));
            assert(m_Data && "String allocation failed");
            m_Data[0] = '\0';
        }

        String(const char* str, eng::memory::IAllocator* allocator = nullptr)
            : m_Allocator(allocator ? allocator : eng::memory::GetDefaultAllocator())
            , m_Data(nullptr)
            , m_Size(0)
            , m_Capacity(0)
        {
            size_t len = str ? std::strlen(str) : 0;
            m_Size = len;
            m_Capacity = len + 1;
            m_Data = static_cast<char*>(m_Allocator->Allocate(m_Capacity * sizeof(char), alignof(char)));
            assert(m_Data && "String allocation failed");
            if (len > 0) {
                std::memcpy(m_Data, str, len);
            }
            m_Data[len] = '\0';
        }

        String(const char* str, size_t length, eng::memory::IAllocator* allocator = nullptr)
            : m_Allocator(allocator ? allocator : eng::memory::GetDefaultAllocator())
            , m_Data(nullptr)
            , m_Size(length)
            , m_Capacity(length + 1)
        {
            m_Data = static_cast<char*>(m_Allocator->Allocate(m_Capacity * sizeof(char), alignof(char)));
            assert(m_Data && "String allocation failed");
            if (length > 0 && str) {
                std::memcpy(m_Data, str, length);
            }
            m_Data[length] = '\0';
        }

        String(StringView sv, eng::memory::IAllocator* allocator = nullptr)
            : String(sv.Data(), sv.Size(), allocator) {}

        ~String() {
            if (m_Data) {
                m_Allocator->Free(m_Data);
            }
        }

        // Copy constructor
        String(const String& other)
            : m_Allocator(other.m_Allocator)
            , m_Data(nullptr)
            , m_Size(other.m_Size)
            , m_Capacity(other.m_Size + 1)
        {
            m_Data = static_cast<char*>(m_Allocator->Allocate(m_Capacity * sizeof(char), alignof(char)));
            assert(m_Data && "String allocation failed");
            if (m_Size > 0) {
                std::memcpy(m_Data, other.m_Data, m_Size);
            }
            m_Data[m_Size] = '\0';
        }

        // Move constructor
        String(String&& other) noexcept
            : m_Allocator(other.m_Allocator)
            , m_Data(other.m_Data)
            , m_Size(other.m_Size)
            , m_Capacity(other.m_Capacity)
        {
            other.m_Data = nullptr;
            other.m_Size = 0;
            other.m_Capacity = 0;
        }

        // Copy assignment
        String& operator=(const String& other) {
            if (this != &other) {
                if (m_Capacity < other.m_Size + 1) {
                    if (m_Data) {
                        m_Allocator->Free(m_Data);
                    }
                    m_Capacity = other.m_Size + 1;
                    m_Data = static_cast<char*>(m_Allocator->Allocate(m_Capacity * sizeof(char), alignof(char)));
                    assert(m_Data && "String allocation failed");
                }
                m_Size = other.m_Size;
                if (m_Size > 0) {
                    std::memcpy(m_Data, other.m_Data, m_Size);
                }
                m_Data[m_Size] = '\0';
            }
            return *this;
        }

        // Move assignment
        String& operator=(String&& other) noexcept {
            if (this != &other) {
                if (m_Data) {
                    m_Allocator->Free(m_Data);
                }
                m_Allocator = other.m_Allocator;
                m_Data = other.m_Data;
                m_Size = other.m_Size;
                m_Capacity = other.m_Capacity;

                other.m_Data = nullptr;
                other.m_Size = 0;
                other.m_Capacity = 0;
            }
            return *this;
        }

        String& operator=(const char* str) {
            size_t len = str ? std::strlen(str) : 0;
            if (m_Capacity < len + 1) {
                if (m_Data) {
                    m_Allocator->Free(m_Data);
                }
                m_Capacity = len + 1;
                m_Data = static_cast<char*>(m_Allocator->Allocate(m_Capacity * sizeof(char), alignof(char)));
                assert(m_Data && "String allocation failed");
            }
            m_Size = len;
            if (m_Size > 0) {
                std::memcpy(m_Data, str, m_Size);
            }
            m_Data[m_Size] = '\0';
            return *this;
        }

        [[nodiscard]] const char* CStr() const noexcept { return m_Data ? m_Data : ""; }
        [[nodiscard]] const char* Data() const noexcept { return m_Data; }
        [[nodiscard]] char* Data() noexcept { return m_Data; }
        [[nodiscard]] size_t Size() const noexcept { return m_Size; }
        [[nodiscard]] size_t Length() const noexcept { return m_Size; }
        [[nodiscard]] size_t Capacity() const noexcept { return m_Capacity; }
        [[nodiscard]] bool Empty() const noexcept { return m_Size == 0; }

        char operator[](size_t index) const noexcept {
            assert(index < m_Size && "String index out of bounds");
            return m_Data[index];
        }

        char& operator[](size_t index) noexcept {
            assert(index < m_Size && "String index out of bounds");
            return m_Data[index];
        }

        char At(size_t index) const {
            if (index >= m_Size) {
                throw std::out_of_range("String index out of range");
            }
            return m_Data[index];
        }

        char& At(size_t index) {
            if (index >= m_Size) {
                throw std::out_of_range("String index out of range");
            }
            return m_Data[index];
        }

        void Clear() noexcept {
            m_Size = 0;
            if (m_Data) {
                m_Data[0] = '\0';
            }
        }

        void Reserve(size_t newCapacity) {
            if (newCapacity <= m_Capacity) return;

            char* newData = static_cast<char*>(m_Allocator->Allocate(newCapacity * sizeof(char), alignof(char)));
            assert(newData && "String allocation failed");
            if (m_Size > 0 && m_Data) {
                std::memcpy(newData, m_Data, m_Size);
            }
            newData[m_Size] = '\0';

            if (m_Data) {
                m_Allocator->Free(m_Data);
            }
            m_Data = newData;
            m_Capacity = newCapacity;
        }

        void Append(const char* str, size_t len) {
            if (len == 0) return;
            size_t reqCapacity = m_Size + len + 1;
            if (reqCapacity > m_Capacity) {
                size_t newCapacity = m_Capacity * 2;
                if (newCapacity < reqCapacity) newCapacity = reqCapacity;
                Reserve(newCapacity);
            }
            std::memcpy(m_Data + m_Size, str, len);
            m_Size += len;
            m_Data[m_Size] = '\0';
        }

        void Append(const char* str) {
            Append(str, str ? std::strlen(str) : 0);
        }

        void Append(const String& other) {
            Append(other.Data(), other.Size());
        }

        void Append(StringView sv) {
            Append(sv.Data(), sv.Size());
        }

        String& operator+=(const char* str) {
            Append(str);
            return *this;
        }

        String& operator+=(const String& other) {
            Append(other);
            return *this;
        }

        String& operator+=(StringView sv) {
            Append(sv);
            return *this;
        }

        operator StringView() const noexcept { return StringView(m_Data, m_Size); }

        iterator begin() noexcept { return m_Data; }
        const_iterator begin() const noexcept { return m_Data; }
        const_iterator cbegin() const noexcept { return m_Data; }

        iterator end() noexcept { return m_Data + m_Size; }
        const_iterator end() const noexcept { return m_Data + m_Size; }
        const_iterator cend() const noexcept { return m_Data + m_Size; }

        [[nodiscard]] eng::memory::IAllocator* GetAllocator() const noexcept { return m_Allocator; }

    private:
        eng::memory::IAllocator* m_Allocator;
        char* m_Data;
        size_t m_Size;
        size_t m_Capacity;
    };

    inline bool operator==(const String& lhs, const String& rhs) noexcept {
        return StringView(lhs) == StringView(rhs);
    }
    inline bool operator==(const String& lhs, const char* rhs) noexcept {
        return StringView(lhs) == StringView(rhs);
    }
    inline bool operator==(const char* lhs, const String& rhs) noexcept {
        return StringView(lhs) == StringView(rhs);
    }
    inline bool operator!=(const String& lhs, const String& rhs) noexcept {
        return StringView(lhs) != StringView(rhs);
    }
    inline bool operator!=(const String& lhs, const char* rhs) noexcept {
        return StringView(lhs) != StringView(rhs);
    }
    inline bool operator!=(const char* lhs, const String& rhs) noexcept {
        return StringView(lhs) != StringView(rhs);
    }

    inline String operator+(String lhs, const char* rhs) {
        lhs.Append(rhs);
        return lhs;
    }

    inline String operator+(String lhs, const String& rhs) {
        lhs.Append(rhs);
        return lhs;
    }

    inline String operator+(const char* lhs, const String& rhs) {
        String result(lhs, rhs.GetAllocator());
        result.Append(rhs);
        return result;
    }

    inline std::ostream& operator<<(std::ostream& os, const String& str) {
        os << StringView(str);
        return os;
    }

} // namespace eng::containers
