#include "Runtime/DynamicLibraryTests.h"
#include "Core/Platform/DynamicLibrary.h"
#include "Core/Logger.h"
#include <cassert>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace eng::runtime {

    void RunDynamicLibraryTests() {
        CORE_LOG_INFO("=== Running Kernel Dynamic Library Loader Tests ===");

        // Test 1: Invalid Library Load Failure
        {
            CORE_LOG_INFO("[Test] Loading non-existent library...");
            eng::platform::DynamicLibrary lib;
            auto res = eng::platform::DynamicLibrary::Load("non_existent_library_xyz.dll", lib);
            assert(res != eng::core::ResultCode::Success && "Load unexpectedly succeeded for missing file!");
            assert(!lib.IsLoaded() && "IsLoaded returned true for missing file!");
        }

        // Test 2: Valid Library Load & Symbol Resolution
        {
#ifdef _WIN32
            CORE_LOG_INFO("[Test] Loading Win32 kernel32.dll and resolving symbols...");
            eng::platform::DynamicLibrary lib;
            auto res = eng::platform::DynamicLibrary::Load("kernel32.dll", lib);
            assert(res == eng::core::ResultCode::Success && "Failed to load kernel32.dll!");
            assert(lib.IsLoaded() && "IsLoaded returned false after successful load!");

            // Resolve GetTickCount
            void* sym = lib.GetSymbol("GetTickCount");
            assert(sym != nullptr && "Failed to resolve GetTickCount!");

            // Call GetTickCount
            using GetTickCountFn = DWORD(WINAPI*)();
            auto tickFn = reinterpret_cast<GetTickCountFn>(sym);
            DWORD ticks = tickFn();
            CORE_LOG_INFO("  GetTickCount returned: %lu", ticks);
            assert(ticks > 0 && "GetTickCount returned 0!");

            // Test 3: Move semantics
            CORE_LOG_INFO("[Test] Testing move constructor...");
            eng::platform::DynamicLibrary libMoved(std::move(lib));
            assert(!lib.IsLoaded() && "Original library wrapper still reports loaded after move!");
            assert(libMoved.IsLoaded() && "Moved library wrapper fails to report loaded!");

            // Verify symbol is still callable on the moved instance
            auto tickFnMoved = reinterpret_cast<GetTickCountFn>(libMoved.GetSymbol("GetTickCount"));
            assert(tickFnMoved != nullptr && "Failed to query symbol from moved library instance!");
            DWORD ticks2 = tickFnMoved();
            assert(ticks2 > 0 && "Symbol lookup from moved instance returned invalid value!");

            // Unload explicitly
            libMoved.Unload();
            assert(!libMoved.IsLoaded() && "Failed to unload!");
#else
            CORE_LOG_INFO("[Test] Running on non-Windows system, skipping native Win32 library load test.");
#endif
        }

        CORE_LOG_INFO("=== All Dynamic Library Loader Tests Passed Successfully ===");
    }
}
