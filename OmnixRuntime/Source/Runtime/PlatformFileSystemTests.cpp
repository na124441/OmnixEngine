#include "Runtime/PlatformFileSystemTests.h"
#include "Core/Platform/PlatformFileSystem.h"
#include "Core/Logger.h"
#include <cassert>
#include <filesystem>
#include <cstring>

namespace eng::runtime {

    void RunPlatformFileSystemTests() {
        CORE_LOG_INFO("=== Running Kernel Platform File System Tests ===");

        // Test 1: System Directory Queries
        CORE_LOG_INFO("[Test] Querying system paths...");
        std::string execDir = eng::platform::PlatformFileSystem::GetExecutableDirectory();
        std::string appDir = eng::platform::PlatformFileSystem::GetAppDirectory();
        std::string tempDir = eng::platform::PlatformFileSystem::GetTempDirectory();

        assert(!execDir.empty() && "Executable directory query failed!");
        assert(!tempDir.empty() && "Temp directory query failed!");
        CORE_LOG_INFO("  Exec Dir: '%s'", execDir.c_str());
        CORE_LOG_INFO("  Temp Dir: '%s'", tempDir.c_str());

        // Test 2: Normalization & Protocol Resolution
        CORE_LOG_INFO("[Test] Testing path translations and virtual protocols...");
        std::string rawPath = "test:\\\\subfolder\\\\file.bin";
        std::string normPath = eng::platform::PlatformFileSystem::NormalizePath(rawPath);
        assert(normPath == "test:/subfolder/file.bin" && "NormalizePath failed!");

        // Register protocol "test://" pointing to temp folder
        eng::platform::PlatformFileSystem::RegisterProtocol("test", tempDir);
        std::string resolved = eng::platform::PlatformFileSystem::ResolveVirtualPath("test://subfolder/file.bin");
        std::string expected = tempDir + "/subfolder/file.bin";
        assert(resolved == expected && "ResolveVirtualPath protocol translation mismatch!");
        CORE_LOG_INFO("  Normalized: '%s'", normPath.c_str());
        CORE_LOG_INFO("  Resolved:   '%s'", resolved.c_str());

        // Test 3: Platform File Creation and Stream IO
        CORE_LOG_INFO("[Test] Testing PlatformFile native handle IO...");
        std::string testFilePath = "test://temp_platform_test.bin";
        std::string resolvedTestFile = eng::platform::PlatformFileSystem::ResolveVirtualPath(testFilePath);

        // Delete test file if it exists prior to test
        std::error_code ec;
        std::filesystem::remove(resolvedTestFile, ec);

        {
            eng::platform::PlatformFile file;
            auto openRes = eng::platform::PlatformFile::Open(testFilePath, eng::platform::FileMode::Write, eng::platform::FileShare::None, file);
            assert(openRes == eng::core::ResultCode::Success && "Failed to open PlatformFile for writing!");
            assert(file.IsOpen() && "File IsOpen returned false!");

            const char* payload = "Omnix Engine Platform File Stream Test Data Payload";
            uint64_t bytesToWrite = std::strlen(payload);
            uint64_t bytesWritten = 0;
            auto writeRes = file.Write(payload, bytesToWrite, bytesWritten);
            
            assert(writeRes == eng::core::ResultCode::Success && "PlatformFile Write failed!");
            assert(bytesWritten == bytesToWrite && "Write byte count mismatch!");

            file.Close();
            assert(!file.IsOpen() && "File IsOpen returned true after Close!");
        }

        // Test 4: Reading and Seeking
        {
            eng::platform::PlatformFile file;
            auto openRes = eng::platform::PlatformFile::Open(testFilePath, eng::platform::FileMode::Read, eng::platform::FileShare::Read, file);
            assert(openRes == eng::core::ResultCode::Success && "Failed to open PlatformFile for reading!");

            const char* expectedPayload = "Omnix Engine Platform File Stream Test Data Payload";
            uint64_t expectedSize = std::strlen(expectedPayload);
            uint64_t fileSize = file.GetSize();
            assert(fileSize == expectedSize && "File size mismatch!");

            // Test Seek
            uint64_t newPos = 0;
            auto seekRes = file.Seek(13, newPos); // Skip "Omnix Engine "
            assert(seekRes == eng::core::ResultCode::Success && "PlatformFile Seek failed!");
            assert(newPos == 13 && "Seek reported incorrect new position!");

            // Read the rest
            char buffer[128] = {};
            uint64_t bytesToRead = fileSize - 13;
            uint64_t bytesRead = 0;
            auto readRes = file.Read(buffer, bytesToRead, bytesRead);

            assert(readRes == eng::core::ResultCode::Success && "PlatformFile Read failed!");
            assert(bytesRead == bytesToRead && "Read byte count mismatch!");
            assert(std::strcmp(buffer, "Platform File Stream Test Data Payload") == 0 && "Read content mismatch!");

            file.Close();
        }

        // Cleanup test file
        std::filesystem::remove(resolvedTestFile, ec);

        CORE_LOG_INFO("[Test] Platform filesystem read, write, seek, and size verified.");
        CORE_LOG_INFO("=== All Platform File System Tests Passed Successfully ===");
    }
}
