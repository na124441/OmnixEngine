#include "Core/UUID/UUID.h"
#include <random>
#include <sstream>
#include <iomanip>

namespace eng::core {

    std::string UUID::ToString() const {
        std::stringstream ss;
        ss << std::hex << std::setfill('0');
        for (int i = 0; i < 16; ++i) {
            ss << std::setw(2) << static_cast<int>(bytes[i]);
            if (i == 3 || i == 5 || i == 7 || i == 9) {
                ss << "-";
            }
        }
        return ss.str();
    }

    UUID UUID::FromString(const std::string& str) {
        UUID uuid{};
        std::string hexStr = "";
        for (char c : str) {
            if (c != '-') {
                hexStr += c;
            }
        }

        if (hexStr.size() != 32) {
            return uuid;
        }

        for (size_t i = 0; i < 16; ++i) {
            std::string byteStr = hexStr.substr(i * 2, 2);
            try {
                uuid.bytes[i] = static_cast<uint8_t>(std::stoul(byteStr, nullptr, 16));
            } catch (...) {
                return UUID{};
            }
        }

        return uuid;
    }

    UUID UUID::GenerateV4() {
        UUID uuid{};
        
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<uint32_t> dis(0, 255);

        for (int i = 0; i < 16; ++i) {
            uuid.bytes[i] = static_cast<uint8_t>(dis(gen));
        }

        // Set version to 4 (0100)
        uuid.bytes[6] = (uuid.bytes[6] & 0x0F) | 0x40;
        // Set variant to RFC 4122 (10xx)
        uuid.bytes[8] = (uuid.bytes[8] & 0x3F) | 0x80;

        return uuid;
    }

} // namespace eng::core
