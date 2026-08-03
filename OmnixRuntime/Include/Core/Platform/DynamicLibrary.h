#pragma once
#include <string>
#include "Core/Error/ResultCode.h"

namespace eng::platform {

    class DynamicLibrary {
    public:
        DynamicLibrary() noexcept = default;
        ~DynamicLibrary() noexcept;

        DynamicLibrary(const DynamicLibrary&) = delete;
        DynamicLibrary& operator=(const DynamicLibrary&) = delete;

        DynamicLibrary(DynamicLibrary&& other) noexcept;
        DynamicLibrary& operator=(DynamicLibrary&& other) noexcept;

        static eng::core::ResultCode Load(const std::string& path, DynamicLibrary& outLib) noexcept;
        void Unload() noexcept;

        [[nodiscard]] bool IsLoaded() const noexcept;
        [[nodiscard]] void* GetSymbol(const std::string& symbolName) const noexcept;

    private:
        void* m_Handle = nullptr;
    };

} // namespace eng::platform
