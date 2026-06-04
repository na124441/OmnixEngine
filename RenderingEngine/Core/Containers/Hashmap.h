#include <functional>
template <
    typename Key,
    typename Value,
    typename Hasher = std::hash<Key>,
    typename KeyEqual = std::equal_to<Key>,
    typename Alloc = std::allocator<std::pair<const Key, Value>>>
class HashMap {
public:
    explicit HashMap(size_t bucketCount = 0, const Alloc& alloc = Alloc());

    // Insert returns true on success, false if key existed.
    bool Insert(const Key& key, const Value& value);
    bool InsertOrAssign(const Key& key, const Value& value);

    Value* Find(const Key& key) noexcept;
    const Value* Find(const Key& key) const noexcept;

    bool Erase(const Key& key);
    void Clear() noexcept;

    size_t Size() const noexcept;
    bool   Empty() const noexcept;

    // Simple forward iterator for range‑for
    struct Iterator { /* ... */ };
    Iterator begin() const noexcept;
    Iterator end()   const noexcept;

private:
    // Internals: bucket array (Key + Value + state flag)
    // Robin‑hood displacement handling
};
