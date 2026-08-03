#include "Runtime/FileTests.h"
#include "Core/IO/BinaryReader.h"
#include "Core/IO/BinaryWriter.h"
#include "Core/IO/FileWatcher.h"
#include "Core/IO/FileSystem.h"
#include "Core/Logger.h"
#include <cassert>
#include <thread>
#include <chrono>

namespace eng::runtime {

    void RunFileTests() {
        CORE_LOG_INFO("=== Running Kernel File Utilities & IO Tests ===");

        const std::string testBinFile = "test_temp_file.bin";
        const std::string testTxtFile = "test_temp_file.txt";

        // Cleanup before starting
        eng::core::fs::RemoveFile(testBinFile);
        eng::core::fs::RemoveFile(testTxtFile);

        // Test 1: BinaryWriter & BinaryReader Serialization / Deserialization
        {
            CORE_LOG_INFO("[Test] Starting BinaryReader/Writer serialization validation...");
            eng::core::BinaryWriter writer;
            const char magic[8] = {'O', 'M', 'N', 'I', 'X', 'B', 'I', 'N'};
            writer.BeginFile(magic, 1, 2);

            writer.WriteU8(123);
            writer.WriteU16(4567);
            writer.WriteU32(987654);
            writer.WriteU64(1234567890123ULL);
            writer.WriteF32(3.14159f);
            writer.WriteBool(true);
            writer.WriteString("Hello Omnix Engine IO!");

            bool writeSuccess = writer.SaveToFile(testBinFile);
            assert(writeSuccess && "Failed to save binary file!");

            eng::core::BinaryReader reader;
            bool readSuccess = reader.LoadFromFile(testBinFile);
            assert(readSuccess && "Failed to load binary file!");

            bool headerValid = reader.ValidateHeaderAndChecksum(magic, 1, 2);
            assert(headerValid && "Header/Checksum validation failed!");

            assert(reader.ReadU8() == 123 && "ReadU8 mismatch!");
            assert(reader.ReadU16() == 4567 && "ReadU16 mismatch!");
            assert(reader.ReadU32() == 987654 && "ReadU32 mismatch!");
            assert(reader.ReadU64() == 1234567890123ULL && "ReadU64 mismatch!");
            assert(std::abs(reader.ReadF32() - 3.14159f) < 0.0001f && "ReadF32 mismatch!");
            assert(reader.ReadBool() == true && "ReadBool mismatch!");
            assert(reader.ReadString() == "Hello Omnix Engine IO!" && "ReadString mismatch!");

            CORE_LOG_INFO("[Test] BinaryReader/Writer serialization validation passed.");
        }

        // Test 2: Safe Exception-Free fs:: Helpers & Monadic Results
        {
            CORE_LOG_INFO("[Test] Starting safe filesystem helpers validation...");
            
            // Text file operations
            auto writeTextRes = eng::core::fs::WriteText(testTxtFile, "Safe Monadic IO content!");
            assert(writeTextRes.has_value() && "Safe WriteText failed!");

            bool exists = eng::core::fs::Exists(testTxtFile);
            assert(exists && "File should exist!");

            auto sizeRes = eng::core::fs::GetSize(testTxtFile);
            assert(sizeRes.has_value() && "Safe GetSize failed!");
            assert(sizeRes.value() == 24 && "Text size mismatch!");

            auto readTextRes = eng::core::fs::ReadText(testTxtFile);
            assert(readTextRes.has_value() && "Safe ReadText failed!");
            assert(readTextRes.value() == "Safe Monadic IO content!" && "ReadText content mismatch!");

            // Missing file check
            auto missingTextRes = eng::core::fs::ReadText("non_existent_file_io.txt");
            assert(!missingTextRes.has_value() && "ReadText should fail for non-existent file!");
            assert(missingTextRes.error() == std::errc::no_such_file_or_directory && "Expected no such file error!");

            CORE_LOG_INFO("[Test] Safe filesystem helpers validation passed.");
        }

        // Test 3: FileWatcher Debounced Callback Testing
        {
            CORE_LOG_INFO("[Test] Starting FileWatcher callback validation...");
            
            const std::string watchDir = "./temp_watch_folder";
            eng::core::fs::CreateDir(watchDir);
            
            eng::core::FileWatcher watcher;
            watcher.WatchDirectory(watchDir);

            std::string triggeredPath = "";
            watcher.SetCallback([&triggeredPath](const std::filesystem::path& path) {
                triggeredPath = path.string();
            });

            // Write a new file inside the watched folder
            std::string targetFilePath = watchDir + "/test_change.txt";
            eng::core::fs::WriteText(targetFilePath, "Initial content");

            // Scan watched folder first to register existence
            watcher.PollChanges();

            // Modify the file to trigger write time change
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            eng::core::fs::WriteText(targetFilePath, "Modified content");

            // Poll multiple times with debouncing delay
            watcher.PollChanges();
            std::this_thread::sleep_for(std::chrono::milliseconds(350));
            watcher.PollChanges();

            assert(!triggeredPath.empty() && "FileWatcher failed to trigger callback on change!");
            
            // Clean up watched folder
            eng::core::fs::RemoveFile(targetFilePath);
            eng::core::fs::RemoveFile(watchDir);

            CORE_LOG_INFO("[Test] FileWatcher callback validation passed.");
        }

        // Cleanup after tests
        eng::core::fs::RemoveFile(testBinFile);
        eng::core::fs::RemoveFile(testTxtFile);

        CORE_LOG_INFO("=== All File Utilities & IO Tests Passed Successfully ===");
    }
}
