#pragma once
#include <string>

namespace eng::platform {

    class Clipboard {
    public:
        /**
         * @brief Sets text to the system clipboard.
         * @param text The string payload to write.
         * @return true if successful, false otherwise.
         */
        static bool SetText(const std::string& text) noexcept;

        /**
         * @brief Gets text from the system clipboard.
         * @return The retrieved text, or empty string on failure / if no text is present.
         */
        static std::string GetText() noexcept;

        /**
         * @brief Queries if the clipboard contains text.
         * @return true if text format is available on clipboard, false otherwise.
         */
        static bool HasText() noexcept;
    };

} // namespace eng::platform
