#pragma once
#include <memory>

namespace eng::core {

    template <typename T, typename Allocator = std::allocator<T>>
    class Array {
    public:
        explicit Array(size_t initialCapacity = 0, const Allocator& alloc = Allocator())
            : data_(nullptr), size_(0), capacity_(0), allocator_(alloc) {}

        // Non‑copyable (ownership semantics)
        Array(const Array&) = delete;
        Array& operator=(const Array&) = delete;

        // Movable
        Array(Array&&) noexcept = default;
        Array& operator=(Array&&) noexcept = default;

        // Element access
        T& operator[](size_t idx) noexcept { return data_[idx]; }
        const T& operator[](size_t idx) const noexcept { return data_[idx]; }

        // Modification
        void   PushBack(const T& value) {}
        
        template<typename... Args>
        void   EmplaceBack(Args&&... args) {}
        
        void   Clear() noexcept { size_ = 0; }

        // Capacity
        size_t Size() const noexcept { return size_; }
        size_t Capacity() const noexcept { return capacity_; }
        bool   Empty() const noexcept { return size_ == 0; }

    private:
        T* data_;
        size_t  size_;
        size_t  capacity_;
        Allocator allocator_;
    };

} // namespace eng::core
