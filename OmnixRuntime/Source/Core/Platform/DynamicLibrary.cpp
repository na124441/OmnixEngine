#include "Core/Platform/DynamicLibrary.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace eng::platform {

    DynamicLibrary::~DynamicLibrary() noexcept {
        Unload();
    }

    DynamicLibrary::DynamicLibrary(DynamicLibrary&& other) noexcept
        : m_Handle(other.m_Handle) {
        other.m_Handle = nullptr;
    }

    DynamicLibrary& DynamicLibrary::operator=(DynamicLibrary&& other) noexcept {
        if (this != &other) {
            Unload();
            m_Handle = other.m_Handle;
            other.m_Handle = nullptr;
        }
        return *this;
    }

    eng::core::ResultCode DynamicLibrary::Load(const std::string& path, DynamicLibrary& outLib) noexcept {
        outLib.Unload();

        HMODULE handle = LoadLibraryA(path.c_str());
        if (!handle) {
            return eng::core::ResultCode::Failure;
        }

        outLib.m_Handle = handle;
        return eng::core::ResultCode::Success;
    }

    void DynamicLibrary::Unload() noexcept {
        if (m_Handle) {
            FreeLibrary(static_cast<HMODULE>(m_Handle));
            m_Handle = nullptr;
        }
    }

    bool DynamicLibrary::IsLoaded() const noexcept {
        return m_Handle != nullptr;
    }

    void* DynamicLibrary::GetSymbol(const std::string& symbolName) const noexcept {
        if (!m_Handle) return nullptr;
        return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(m_Handle), symbolName.c_str()));
    }

} // namespace eng::platform

#else

#include <dlfcn.h>

namespace eng::platform {

    DynamicLibrary::~DynamicLibrary() noexcept {
        Unload();
    }

    DynamicLibrary::DynamicLibrary(DynamicLibrary&& other) noexcept
        : m_Handle(other.m_Handle) {
        other.m_Handle = nullptr;
    }

    DynamicLibrary& DynamicLibrary::operator=(DynamicLibrary&& other) noexcept {
        if (this != &other) {
            Unload();
            m_Handle = other.m_Handle;
            other.m_Handle = nullptr;
        }
        return *this;
    }

    eng::core::ResultCode DynamicLibrary::Load(const std::string& path, DynamicLibrary& outLib) noexcept {
        outLib.Unload();

        void* handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!handle) {
            return eng::core::ResultCode::Failure;
        }

        outLib.m_Handle = handle;
        return eng::core::ResultCode::Success;
    }

    void DynamicLibrary::Unload() noexcept {
        if (m_Handle) {
            dlclose(m_Handle);
            m_Handle = nullptr;
        }
    }

    bool DynamicLibrary::IsLoaded() const noexcept {
        return m_Handle != nullptr;
    }

    void* DynamicLibrary::GetSymbol(const std::string& symbolName) const noexcept {
        if (!m_Handle) return nullptr;
        return dlsym(m_Handle, symbolName.c_str());
    }

} // namespace eng::platform

#endif
