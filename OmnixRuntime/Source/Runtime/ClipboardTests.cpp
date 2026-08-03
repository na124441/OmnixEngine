#include "Runtime/ClipboardTests.h"
#include "Core/Platform/Clipboard.h"
#include "Core/Logger.h"
#include <cassert>

namespace eng::runtime {

    void RunClipboardTests() {
        CORE_LOG_INFO("=== Running Kernel Clipboard Subsystem Tests ===");

        // Back up existing clipboard content so we do not overwrite user data permanently
        std::string backupText = platform::Clipboard::GetText();
        bool hadBackup = !backupText.empty();

        CORE_LOG_INFO("[Test] Querying initial Clipboard text state...");
        // Test 1: Write and read text
        std::string testPayload = "OmnixEngine Clipboard Test Payload #42!";
        CORE_LOG_INFO("[Test] Setting clipboard text to: '%s'", testPayload.c_str());
        
        bool setRes = platform::Clipboard::SetText(testPayload);
        assert(setRes && "Failed to set text to Clipboard!");

        // Test 2: HasText query
        bool hasText = platform::Clipboard::HasText();
        assert(hasText && "Clipboard should report having text!");

        // Test 3: GetText retrieve
        std::string retrieved = platform::Clipboard::GetText();
        CORE_LOG_INFO("[Test] Retrieved clipboard text: '%s'", retrieved.c_str());
        assert(retrieved == testPayload && "Retrieved clipboard text does not match payload!");

        // Test 4: Write empty / verify
        std::string emptyPayload = "";
        bool setEmptyRes = platform::Clipboard::SetText(emptyPayload);
        assert(setEmptyRes && "Failed to set empty text to Clipboard!");
        
        std::string retrievedEmpty = platform::Clipboard::GetText();
        assert(retrievedEmpty.empty() && "Retrieved text from empty clipboard should be empty!");

        // Restore backup if there was any
        if (hadBackup) {
            CORE_LOG_INFO("[Test] Restoring user clipboard backup...");
            platform::Clipboard::SetText(backupText);
        } else {
            // Leave it clean
            platform::Clipboard::SetText("");
        }

        CORE_LOG_INFO("=== All Clipboard Tests Passed Successfully ===");
    }

} // namespace eng::runtime
