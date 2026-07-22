#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <new>

namespace eng::memory {

    class IAllocator {
    public:
        virtual ~IAllocator() = default;
        virtual void* Allocate(size_t size, size_t alignment = alignof(std::max_align_t)) = 0;
        virtual void Free(void* ptr) = 0;
    };

    class SystemAllocator : public IAllocator {
    public:
        void* Allocate(size_t size, size_t alignment = alignof(std::max_align_t)) override {
#if defined(_MSC_VER) || defined(__MINGW32__)
            return _aligned_malloc(size, alignment);
#else
            void* ptr = nullptr;
            if (posix_memalign(&ptr, alignment, size) != 0) return nullptr;
            return ptr;
#endif
        }

        void Free(void* ptr) override {
            if (!ptr) return;
#if defined(_MSC_VER) || defined(__MINGW32__)
            _aligned_free(ptr);
#else
            free(ptr);
#endif
        }
    };

    inline IAllocator* GetDefaultAllocator() {
        static SystemAllocator s_SystemAllocator;
        return &s_SystemAllocator;
    }

} // namespace eng::memory
