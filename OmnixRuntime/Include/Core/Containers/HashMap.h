#pragma once
#include "Core/Memory/IAllocator.h"
#include <cstddef>
#include <cstdint>
#include <new>
#include <utility>
#include <functional>
#include <algorithm>
#include <cassert>
#include <stdexcept>

namespace eng::containers {

    template<typename Key, typename Value, typename Hash = std::hash<Key>, typename KeyEqual = std::equal_to<Key>>
    class HashMap {
    private:
        struct Bucket {
            alignas(Key) uint8_t keyBuffer[sizeof(Key)];
            alignas(Value) uint8_t valueBuffer[sizeof(Value)];
            bool occupied = false;
            bool deleted = false;

            Key& GetKey() { return *reinterpret_cast<Key*>(keyBuffer); }
            const Key& GetKey() const { return *reinterpret_cast<const Key*>(keyBuffer); }
            Value& GetValue() { return *reinterpret_cast<Value*>(valueBuffer); }
            const Value& GetValue() const { return *reinterpret_cast<const Value*>(valueBuffer); }
        };

    public:
        struct Entry {
            const Key& first;
            Value& second;
        };

        class iterator {
        public:
            iterator(Bucket* buckets, size_t capacity, size_t index)
                : m_Buckets(buckets), m_Capacity(capacity), m_Index(index) {
                MoveToNextOccupied();
            }

            Entry operator*() const {
                return Entry{ m_Buckets[m_Index].GetKey(), m_Buckets[m_Index].GetValue() };
            }

            iterator& operator++() {
                ++m_Index;
                MoveToNextOccupied();
                return *this;
            }

            bool operator==(const iterator& o) const {
                return m_Index == o.m_Index;
            }

            bool operator!=(const iterator& o) const {
                return m_Index != o.m_Index;
            }

        private:
            void MoveToNextOccupied() {
                while (m_Index < m_Capacity && !m_Buckets[m_Index].occupied) {
                    ++m_Index;
                }
            }

            Bucket* m_Buckets;
            size_t m_Capacity;
            size_t m_Index;
        };

        class const_iterator {
        public:
            struct ConstEntry {
                const Key& first;
                const Value& second;
            };

            const_iterator(const Bucket* buckets, size_t capacity, size_t index)
                : m_Buckets(buckets), m_Capacity(capacity), m_Index(index) {
                MoveToNextOccupied();
            }

            ConstEntry operator*() const {
                return ConstEntry{ m_Buckets[m_Index].GetKey(), m_Buckets[m_Index].GetValue() };
            }

            const_iterator& operator++() {
                ++m_Index;
                MoveToNextOccupied();
                return *this;
            }

            bool operator==(const const_iterator& o) const {
                return m_Index == o.m_Index;
            }

            bool operator!=(const const_iterator& o) const {
                return m_Index != o.m_Index;
            }

        private:
            void MoveToNextOccupied() {
                while (m_Index < m_Capacity && !m_Buckets[m_Index].occupied) {
                    ++m_Index;
                }
            }

            const Bucket* m_Buckets;
            size_t m_Capacity;
            size_t m_Index;
        };

        HashMap() : HashMap(eng::memory::GetDefaultAllocator()) {}

        explicit HashMap(eng::memory::IAllocator* allocator)
            : m_Allocator(allocator ? allocator : eng::memory::GetDefaultAllocator())
            , m_Buckets(nullptr)
            , m_Size(0)
            , m_Capacity(0)
        {}

        explicit HashMap(size_t initialCapacity, eng::memory::IAllocator* allocator = nullptr)
            : m_Allocator(allocator ? allocator : eng::memory::GetDefaultAllocator())
            , m_Buckets(nullptr)
            , m_Size(0)
            , m_Capacity(0)
        {
            if (initialCapacity > 0) {
                size_t cap = 4;
                while (cap < initialCapacity) cap *= 2;
                Rehash(cap);
            }
        }

        ~HashMap() {
            Clear();
            if (m_Buckets) {
                m_Allocator->Free(m_Buckets);
            }
        }

        HashMap(const HashMap& other)
            : m_Allocator(other.m_Allocator)
            , m_Buckets(nullptr)
            , m_Size(0)
            , m_Capacity(0)
        {
            if (other.m_Capacity > 0) {
                Rehash(other.m_Capacity);
                for (size_t i = 0; i < other.m_Capacity; ++i) {
                    if (other.m_Buckets[i].occupied) {
                        InsertInternal(other.m_Buckets[i].GetKey(), other.m_Buckets[i].GetValue());
                    }
                }
            }
        }

        HashMap(HashMap&& other) noexcept
            : m_Allocator(other.m_Allocator)
            , m_Buckets(other.m_Buckets)
            , m_Size(other.m_Size)
            , m_Capacity(other.m_Capacity)
        {
            other.m_Buckets = nullptr;
            other.m_Size = 0;
            other.m_Capacity = 0;
        }

        HashMap& operator=(const HashMap& other) {
            if (this != &other) {
                Clear();
                if (m_Buckets) {
                    m_Allocator->Free(m_Buckets);
                    m_Buckets = nullptr;
                    m_Capacity = 0;
                }
                if (other.m_Capacity > 0) {
                    Rehash(other.m_Capacity);
                    for (size_t i = 0; i < other.m_Capacity; ++i) {
                        if (other.m_Buckets[i].occupied) {
                            InsertInternal(other.m_Buckets[i].GetKey(), other.m_Buckets[i].GetValue());
                        }
                    }
                }
            }
            return *this;
        }

        HashMap& operator=(HashMap&& other) noexcept {
            if (this != &other) {
                Clear();
                if (m_Buckets) {
                    m_Allocator->Free(m_Buckets);
                }
                m_Allocator = other.m_Allocator;
                m_Buckets = other.m_Buckets;
                m_Size = other.m_Size;
                m_Capacity = other.m_Capacity;

                other.m_Buckets = nullptr;
                other.m_Size = 0;
                other.m_Capacity = 0;
            }
            return *this;
        }

        bool Insert(const Key& key, const Value& value) {
            if (m_Capacity == 0 || (static_cast<float>(m_Size + 1) / m_Capacity) > 0.7f) {
                Grow();
            }
            return InsertInternal(key, value);
        }

        bool Insert(Key&& key, Value&& value) {
            if (m_Capacity == 0 || (static_cast<float>(m_Size + 1) / m_Capacity) > 0.7f) {
                Grow();
            }
            return InsertInternal(std::move(key), std::move(value));
        }

        iterator Find(const Key& key) noexcept {
            if (m_Capacity == 0) return end();
            size_t index = FindBucketIndex(key);
            if (index != m_Capacity) {
                return iterator(m_Buckets, m_Capacity, index);
            }
            return end();
        }

        const_iterator Find(const Key& key) const noexcept {
            if (m_Capacity == 0) return cend();
            size_t index = FindBucketIndex(key);
            if (index != m_Capacity) {
                return const_iterator(m_Buckets, m_Capacity, index);
            }
            return cend();
        }

        bool Erase(const Key& key) {
            if (m_Capacity == 0) return false;
            size_t index = FindBucketIndex(key);
            if (index != m_Capacity) {
                m_Buckets[index].GetKey().~Key();
                m_Buckets[index].GetValue().~Value();
                m_Buckets[index].occupied = false;
                m_Buckets[index].deleted = true;
                --m_Size;
                return true;
            }
            return false;
        }

        Value& operator[](const Key& key) {
            if (m_Capacity == 0 || (static_cast<float>(m_Size + 1) / m_Capacity) > 0.7f) {
                Grow();
            }
            size_t index = FindBucketIndex(key);
            if (index != m_Capacity) {
                return m_Buckets[index].GetValue();
            }
            size_t h = Hash{}(key);
            size_t mask = m_Capacity - 1;
            for (size_t i = 0; i < m_Capacity; ++i) {
                size_t idx = (h + i * i) & mask;
                if (!m_Buckets[idx].occupied) {
                    new (m_Buckets[idx].keyBuffer) Key(key);
                    new (m_Buckets[idx].valueBuffer) Value();
                    m_Buckets[idx].occupied = true;
                    m_Buckets[idx].deleted = false;
                    ++m_Size;
                    return m_Buckets[idx].GetValue();
                }
            }
            throw std::runtime_error("HashMap is full during operator[] insert");
        }

        void Clear() noexcept {
            if (m_Buckets) {
                for (size_t i = 0; i < m_Capacity; ++i) {
                    if (m_Buckets[i].occupied) {
                        m_Buckets[i].GetKey().~Key();
                        m_Buckets[i].GetValue().~Value();
                        m_Buckets[i].occupied = false;
                    }
                    m_Buckets[i].deleted = false;
                }
            }
            m_Size = 0;
        }

        [[nodiscard]] size_t Size() const noexcept { return m_Size; }
        [[nodiscard]] size_t Capacity() const noexcept { return m_Capacity; }
        [[nodiscard]] bool Empty() const noexcept { return m_Size == 0; }

        iterator begin() noexcept { return iterator(m_Buckets, m_Capacity, 0); }
        const_iterator begin() const noexcept { return const_iterator(m_Buckets, m_Capacity, 0); }
        const_iterator cbegin() const noexcept { return const_iterator(m_Buckets, m_Capacity, 0); }

        iterator end() noexcept { return iterator(m_Buckets, m_Capacity, m_Capacity); }
        const_iterator end() const noexcept { return const_iterator(m_Buckets, m_Capacity, m_Capacity); }
        const_iterator cend() const noexcept { return const_iterator(m_Buckets, m_Capacity, m_Capacity); }

        [[nodiscard]] eng::memory::IAllocator* GetAllocator() const noexcept { return m_Allocator; }

    private:
        void Grow() {
            size_t newCapacity = m_Capacity == 0 ? 8 : m_Capacity * 2;
            Rehash(newCapacity);
        }

        void Rehash(size_t newCapacity) {
            Bucket* oldBuckets = m_Buckets;
            size_t oldCapacity = m_Capacity;

            m_Buckets = static_cast<Bucket*>(m_Allocator->Allocate(newCapacity * sizeof(Bucket), alignof(Bucket)));
            assert(m_Buckets && "HashMap allocation failed");
            m_Capacity = newCapacity;
            m_Size = 0;

            for (size_t i = 0; i < newCapacity; ++i) {
                m_Buckets[i].occupied = false;
                m_Buckets[i].deleted = false;
            }

            if (oldBuckets) {
                for (size_t i = 0; i < oldCapacity; ++i) {
                    if (oldBuckets[i].occupied) {
                        InsertInternal(std::move(oldBuckets[i].GetKey()), std::move(oldBuckets[i].GetValue()));
                        oldBuckets[i].GetKey().~Key();
                        oldBuckets[i].GetValue().~Value();
                    }
                }
                m_Allocator->Free(oldBuckets);
            }
        }

        template<typename K, typename V>
        bool InsertInternal(K&& key, V&& value) {
            size_t h = Hash{}(key);
            size_t mask = m_Capacity - 1;
            for (size_t i = 0; i < m_Capacity; ++i) {
                size_t idx = (h + i * i) & mask;
                if (m_Buckets[idx].occupied) {
                    if (KeyEqual{}(m_Buckets[idx].GetKey(), key)) {
                        m_Buckets[idx].GetValue() = std::forward<V>(value);
                        return false;
                    }
                } else {
                    new (m_Buckets[idx].keyBuffer) Key(std::forward<K>(key));
                    new (m_Buckets[idx].valueBuffer) Value(std::forward<V>(value));
                    m_Buckets[idx].occupied = true;
                    m_Buckets[idx].deleted = false;
                    ++m_Size;
                    return true;
                }
            }
            return false;
        }

        size_t FindBucketIndex(const Key& key) const noexcept {
            size_t h = Hash{}(key);
            size_t mask = m_Capacity - 1;
            for (size_t i = 0; i < m_Capacity; ++i) {
                size_t idx = (h + i * i) & mask;
                if (m_Buckets[idx].occupied) {
                    if (KeyEqual{}(m_Buckets[idx].GetKey(), key)) {
                        return idx;
                    }
                } else if (!m_Buckets[idx].deleted) {
                    break;
                }
            }
            return m_Capacity;
        }

        eng::memory::IAllocator* m_Allocator;
        Bucket* m_Buckets;
        size_t m_Size;
        size_t m_Capacity;
    };

} // namespace eng::containers
