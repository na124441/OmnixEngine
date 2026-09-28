#include "Core/Logging/LoggerTests.h"
#include "Core/Logging/Logger.h"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>

namespace eng::logging {

    bool RunLoggerTests() noexcept {
        std::cout << "================================================================================\n";
        std::cout << "                              RUNNING LOGGER TESTS                              \n";
        std::cout << "================================================================================\n";

        const std::string testLogPath = "test_logger_output.log";

        // Cleanup before starting
        if (std::filesystem::exists(testLogPath)) {
            std::filesystem::remove(testLogPath);
        }

        // Test 1: Init and File Creation
        std::cout << "[LoggerTest] Running Test 1: Init and File Creation...\n";
        {
            Logger::Init(testLogPath, LogSeverity::Info);
            if (!std::filesystem::exists(testLogPath)) {
                std::cerr << "[LoggerTest] Test 1 FAILED: Log file was not created.\n";
                return false;
            }
            std::cout << "[LoggerTest] Test 1 Passed: Log file successfully created.\n";
        }

        // Test 2: Severity Get/Set
        std::cout << "[LoggerTest] Running Test 2: Severity Get/Set...\n";
        {
            Logger::SetSeverity(LogSeverity::Warning);
            if (Logger::GetSeverity() != LogSeverity::Warning) {
                std::cerr << "[LoggerTest] Test 2 FAILED: Severity not set/retrieved correctly.\n";
                return false;
            }
            Logger::SetSeverity(LogSeverity::Trace);
            if (Logger::GetSeverity() != LogSeverity::Trace) {
                std::cerr << "[LoggerTest] Test 2 FAILED: Severity not set/retrieved correctly to Trace.\n";
                return false;
            }
            std::cout << "[LoggerTest] Test 2 Passed: Severity getter/setter works.\n";
        }

        // Test 3: Log Writing and Severity Filtering
        std::cout << "[LoggerTest] Running Test 3: Log Writing and Filtering...\n";
        {
            // Clear file content by re-initing or we just append and check the last few lines
            // Let's shutdown and delete to start fresh
            Logger::Shutdown();
            if (std::filesystem::exists(testLogPath)) {
                std::filesystem::remove(testLogPath);
            }

            Logger::Init(testLogPath, LogSeverity::Warning); // Only Warning, Error, Fatal

            // Should be ignored
            Logger::LogWrite(LogSeverity::Info, LogCategory::Runtime, "This is an info message.");
            Logger::LogWrite(LogSeverity::Trace, LogCategory::ECS, "This is a trace message.");

            // Should be logged
            Logger::LogWrite(LogSeverity::Warning, LogCategory::Renderer, "This is a warning message.");
            Logger::LogWrite(LogSeverity::Error, LogCategory::Physics, "This is an error message.");

            Logger::Shutdown(); // Flush and close

            // Read the file and check contents
            std::ifstream file(testLogPath);
            std::vector<std::string> lines;
            std::string line;
            while (std::getline(file, line)) {
                if (!line.empty()) {
                    lines.push_back(line);
                }
            }

            // Expected lines:
            // 1. "Logger initialised" from Init()
            // 2. "This is a warning message."
            // 3. "This is an error message."
            // 4. "Logger shutting down" from Shutdown()

            bool foundWarning = false;
            bool foundError = false;
            bool foundInfoOrTrace = false;

            for (const auto& l : lines) {
                if (l.find("This is a warning message.") != std::string::npos) foundWarning = true;
                if (l.find("This is an error message.") != std::string::npos) foundError = true;
                if (l.find("This is an info message.") != std::string::npos) foundInfoOrTrace = true;
                if (l.find("This is a trace message.") != std::string::npos) foundInfoOrTrace = true;
            }

            if (!foundWarning || !foundError || foundInfoOrTrace) {
                std::cerr << "[LoggerTest] Test 3 FAILED: Severity filtering did not work as expected.\n";
                std::cerr << "Found Warning: " << foundWarning << ", Found Error: " << foundError << ", Found Info/Trace (should be false): " << foundInfoOrTrace << "\n";
                return false;
            }

            std::cout << "[LoggerTest] Test 3 Passed: Logs filtered correctly.\n";
        }

        // Cleanup
        if (std::filesystem::exists(testLogPath)) {
            std::filesystem::remove(testLogPath);
        }

        // Re-initialize for the rest of the application
        Logger::Init("Omnix.log", LogSeverity::Trace);

        std::cout << "================================================================================\n";
        std::cout << "                              ALL LOGGER TESTS PASSED                           \n";
        std::cout << "================================================================================\n";
        return true;
    }

} // namespace eng::logging
