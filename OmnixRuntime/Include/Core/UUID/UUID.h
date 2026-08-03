#pragma once

#include <string>
#include <array>
#include <cstdint>

namespace eng::core {

    /**
     * @struct UUID
     * @brief Represents a standard 128-bit RFC 4122 v4 UUID with string conversion and comparison.
     */
    struct UUID {
        std::array<uint8_t, 16> bytes{};

        bool operator==(const UUID& other) const noexcept {
            return bytes == other.bytes;
        }

        bool operator!=(const UUID& other) const noexcept {
            return bytes != other.bytes;
        }

        bool operator<(const UUID& other) const noexcept {
            return bytes < other.bytes;
        }

        /**
         * @brief Converts the UUID to a string format: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
         */
        [[nodiscard]] std::string ToString() const;

        /**
         * @brief Parses a UUID from a string. Returns an empty/null UUID on parse failure.
         */
        [[nodiscard]] static UUID FromString(const std::string& str);

        /**
         * @brief Generates a random v4 UUID using cryptographically secure-ish random source.
         */
        [[nodiscard]] static UUID GenerateV4();
    };

} // namespace eng::core
