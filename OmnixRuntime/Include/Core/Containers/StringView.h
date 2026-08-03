#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <ostream>
#include <algorithm>
#include <cassert>
#include <stdexcept>

namespace eng::containers {

    class StringView {
    public:
        using const_iterator = const char*;

        constexpr StringView() noexcept : m_Data(nullptr), m_Length(0) {}

        constexpr StringView(const char* str) noexcept
            : m_Data(str), m_Length(str ? constexpr_strlen(str) : 0) {}

        constexpr StringView(const char* str, size_t len) noexcept
            : m_Data(str), m_Length(len) {}

        StringView(const std::string& str) noexcept
            : m_Data(str.data()), m_Length(str.length()) {}

        constexpr StringView(std::string_view sv) noexcept
            : m_Data(sv.data()), m_Length(sv.length()) {}

        [[nodiscard]] constexpr const char* Data() const noexcept { return m_Data; }
        [[nodiscard]] constexpr size_t Size() const noexcept { return m_Length; }
        [[nodiscard]] constexpr size_t Length() const noexcept { return m_Length; }
        [[nodiscard]] constexpr bool Empty() const noexcept { return m_Length == 0; }

        constexpr char operator[](size_t index) const noexcept {
            assert(index < m_Length && "StringView index out of bounds");
            return m_Data[index];
        }

        constexpr char At(size_t index) const {
            if (index >= m_Length) {
                throw std::out_of_range("StringView index out of range");
            }
            return m_Data[index];
        }

        [[nodiscard]] constexpr char Front() const noexcept {
            assert(m_Length > 0 && "Front called on empty StringView");
            return m_Data[0];
        }

        [[nodiscard]] constexpr char Back() const noexcept {
            assert(m_Length > 0 && "Back called on empty StringView");
            return m_Data[m_Length - 1];
        }

        constexpr const_iterator begin() const noexcept { return m_Data; }
        constexpr const_iterator cbegin() const noexcept { return m_Data; }
        constexpr const_iterator end() const noexcept { return m_Data + m_Length; }
        constexpr const_iterator cend() const noexcept { return m_Data + m_Length; }

        constexpr void RemovePrefix(size_t n) noexcept {
            assert(n <= m_Length);
            m_Data += n;
            m_Length -= n;
        }

        constexpr void RemoveSuffix(size_t n) noexcept {
            assert(n <= m_Length);
            m_Length -= n;
        }

        [[nodiscard]] constexpr StringView Substring(size_t pos, size_t count = npos) const {
            if (pos > m_Length) {
                throw std::out_of_range("Substring pos out of range");
            }
            size_t actualCount = std::min(count, m_Length - pos);
            return StringView(m_Data + pos, actualCount);
        }

        constexpr int Compare(StringView other) const noexcept {
            size_t minLen = std::min(m_Length, other.m_Length);
            for (size_t i = 0; i < minLen; ++i) {
                if (m_Data[i] < other.m_Data[i]) return -1;
                if (m_Data[i] > other.m_Data[i]) return 1;
            }
            if (m_Length < other.m_Length) return -1;
            if (m_Length > other.m_Length) return 1;
            return 0;
        }

        constexpr bool operator==(StringView o) const noexcept { return Compare(o) == 0; }
        constexpr bool operator!=(StringView o) const noexcept { return Compare(o) != 0; }
        constexpr bool operator<(StringView o) const noexcept { return Compare(o) < 0; }
        constexpr bool operator<=(StringView o) const noexcept { return Compare(o) <= 0; }
        constexpr bool operator>(StringView o) const noexcept { return Compare(o) > 0; }
        constexpr bool operator>=(StringView o) const noexcept { return Compare(o) >= 0; }

        static constexpr size_t npos = static_cast<size_t>(-1);

    private:
        static constexpr size_t constexpr_strlen(const char* str) noexcept {
            size_t len = 0;
            while (str[len] != '\0') {
                ++len;
            }
            return len;
        }

        const char* m_Data;
        size_t m_Length;
    };

    inline std::ostream& operator<<(std::ostream& os, StringView sv) {
        if (!sv.Empty()) {
            os.write(sv.Data(), static_cast<std::streamsize>(sv.Size()));
        }
        return os;
    }

} // namespace eng::containers
