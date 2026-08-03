#pragma once
#include "Core/Memory/IAllocator.h"
#include <cstddef>
#include <cstdint>
#include <new>
#include <utility>
#include <algorithm>
#include <stdexcept>
#include <cassert>

namespace eng::containers {

    template<typename T>
    class DynamicArray {
    public:
        using iterator = T*;
        using const_iterator = const T*;

        DynamicArray() : DynamicArray(eng::memory::GetDefaultAllocator()) {}

        explicit DynamicArray(eng::memory::IAllocator* allocator)
            : m_Allocator(allocator ? allocator : eng::memory::GetDefaultAllocator())
            , m_Data(nullptr)
            , m_Size(0)
            , m_Capacity(0)
        {}

        explicit DynamicArray(size_t initialCapacity, eng::memory::IAllocator* allocator = nullptr)
            : m_Allocator(allocator ? allocator : eng::memory::GetDefaultAllocator())
            , m_Data(nullptr)
            , m_Size(0)
            , m_Capacity(0)
        {
            if (initialCapacity > 0) {
                Reserve(initialCapacity);
            }
        }

        ~DynamicArray() {
            Clear();
            if (m_Data) {
                m_Allocator->Free(m_Data);
            }
        }

        // Copy constructor
        DynamicArray(const DynamicArray& other)
            : m_Allocator(other.m_Allocator)
            , m_Data(nullptr)
            , m_Size(0)
            , m_Capacity(0)
        {
            Reserve(other.m_Size);
            for (size_t i = 0; i < other.m_Size; ++i) {
                new (&m_Data[i]) T(other.m_Data[i]);
            }
            m_Size = other.m_Size;
        }

        // Move constructor
        DynamicArray(DynamicArray&& other) noexcept
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
        DynamicArray& operator=(const DynamicArray& other) {
            if (this != &other) {
                Clear();
                if (m_Capacity < other.m_Size) {
                    if (m_Data) {
                        m_Allocator->Free(m_Data);
                        m_Data = nullptr;
                        m_Capacity = 0;
                    }
                    Reserve(other.m_Size);
                }
                for (size_t i = 0; i < other.m_Size; ++i) {
                    new (&m_Data[i]) T(other.m_Data[i]);
                }
                m_Size = other.m_Size;
            }
            return *this;
        }

        // Move assignment
        DynamicArray& operator=(DynamicArray&& other) noexcept {
            if (this != &other) {
                Clear();
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

        void PushBack(const T& value) {
            if (m_Size >= m_Capacity) {
                Grow();
            }
            new (&m_Data[m_Size]) T(value);
            ++m_Size;
        }

        void PushBack(T&& value) {
            if (m_Size >= m_Capacity) {
                Grow();
            }
            new (&m_Data[m_Size]) T(std::move(value));
            ++m_Size;
        }

        template<typename... Args>
        T& EmplaceBack(Args&&... args) {
            if (m_Size >= m_Capacity) {
                Grow();
            }
            T* ptr = new (&m_Data[m_Size]) T(std::forward<Args>(args)...);
            ++m_Size;
            return *ptr;
        }

        void PopBack() {
            assert(m_Size > 0 && "PopBack called on empty DynamicArray");
            if (m_Size > 0) {
                --m_Size;
                m_Data[m_Size].~T();
            }
        }

        void Reserve(size_t newCapacity) {
            if (newCapacity <= m_Capacity) return;

            T* newData = static_cast<T*>(m_Allocator->Allocate(newCapacity * sizeof(T), alignof(T)));
            assert(newData && "DynamicArray Allocation failed");

            for (size_t i = 0; i < m_Size; ++i) {
                new (&newData[i]) T(std::move(m_Data[i]));
                m_Data[i].~T();
            }

            if (m_Data) {
                m_Allocator->Free(m_Data);
            }

            m_Data = newData;
            m_Capacity = newCapacity;
        }

        void Resize(size_t newSize) {
            if (newSize < m_Size) {
                for (size_t i = newSize; i < m_Size; ++i) {
                    m_Data[i].~T();
                }
                m_Size = newSize;
            } else if (newSize > m_Size) {
                if (newSize > m_Capacity) {
                    Reserve(newSize);
                }
                for (size_t i = m_Size; i < newSize; ++i) {
                    new (&m_Data[i]) T();
                }
                m_Size = newSize;
            }
        }

        void Resize(size_t newSize, const T& value) {
            if (newSize < m_Size) {
                for (size_t i = newSize; i < m_Size; ++i) {
                    m_Data[i].~T();
                }
                m_Size = newSize;
            } else if (newSize > m_Size) {
                if (newSize > m_Capacity) {
                    Reserve(newSize);
                }
                for (size_t i = m_Size; i < newSize; ++i) {
                    new (&m_Data[i]) T(value);
                }
                m_Size = newSize;
            }
        }

        void Clear() noexcept {
            for (size_t i = 0; i < m_Size; ++i) {
                m_Data[i].~T();
            }
            m_Size = 0;
        }

        iterator Erase(const_iterator pos) {
            size_t index = pos - begin();
            assert(index < m_Size && "Erase out of bounds");
            if (index < m_Size) {
                m_Data[index].~T();
                for (size_t i = index; i < m_Size - 1; ++i) {
                    new (&m_Data[i]) T(std::move(m_Data[i + 1]));
                    m_Data[i + 1].~T();
                }
                --m_Size;
            }
            return begin() + index;
        }

        T& operator[](size_t index) noexcept {
            assert(index < m_Size && "Index out of bounds");
            return m_Data[index];
        }

        const T& operator[](size_t index) const noexcept {
            assert(index < m_Size && "Index out of bounds");
            return m_Data[index];
        }

        T& At(size_t index) {
            if (index >= m_Size) {
                throw std::out_of_range("DynamicArray index out of range");
            }
            return m_Data[index];
        }

        const T& At(size_t index) const {
            if (index >= m_Size) {
                throw std::out_of_range("DynamicArray index out of range");
            }
            return m_Data[index];
        }

        [[nodiscard]] T& Front() noexcept {
            assert(m_Size > 0 && "Front called on empty DynamicArray");
            return m_Data[0];
        }

        [[nodiscard]] const T& Front() const noexcept {
            assert(m_Size > 0 && "Front called on empty DynamicArray");
            return m_Data[0];
        }

        [[nodiscard]] T& Back() noexcept {
            assert(m_Size > 0 && "Back called on empty DynamicArray");
            return m_Data[m_Size - 1];
        }

        [[nodiscard]] const T& Back() const noexcept {
            assert(m_Size > 0 && "Back called on empty DynamicArray");
            return m_Data[m_Size - 1];
        }

        [[nodiscard]] T* Data() noexcept { return m_Data; }
        [[nodiscard]] const T* Data() const noexcept { return m_Data; }

        [[nodiscard]] size_t Size() const noexcept { return m_Size; }
        [[nodiscard]] size_t Capacity() const noexcept { return m_Capacity; }
        [[nodiscard]] bool Empty() const noexcept { return m_Size == 0; }

        iterator begin() noexcept { return m_Data; }
        const_iterator begin() const noexcept { return m_Data; }
        const_iterator cbegin() const noexcept { return m_Data; }

        iterator end() noexcept { return m_Data + m_Size; }
        const_iterator end() const noexcept { return m_Data + m_Size; }
        const_iterator cend() const noexcept { return m_Data + m_Size; }

        eng::memory::IAllocator* GetAllocator() const noexcept { return m_Allocator; }

    private:
        void Grow() {
            size_t newCapacity = m_Capacity == 0 ? 4 : m_Capacity * 2;
            Reserve(newCapacity);
        }

        eng::memory::IAllocator* m_Allocator;
        T* m_Data;
        size_t m_Size;
        size_t m_Capacity;
    };

} // namespace eng::containers
