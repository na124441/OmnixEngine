#pragma once

#include <variant>
#include <type_traits>
#include <utility>
#include <stdexcept>
#include "Core/Error/ResultCode.h"

namespace eng::core {

    /**
     * @brief A wrapper for unexpected/error values.
     * Equivalent to std::unexpected in C++23.
     */
    template <typename E>
    class Unexpected {
    public:
        Unexpected() = default;
        explicit Unexpected(const E& err) : m_Error(err) {}
        explicit Unexpected(E&& err) : m_Error(std::move(err)) {}

        const E& value() const { return m_Error; }
        E& value() { return m_Error; }

    private:
        E m_Error;
    };

    /**
     * @brief A type-safe value-or-error container.
     * Similar to std::expected in C++23.
     */
    template <typename T, typename E>
    class Expected {
    public:
        using value_type = T;
        using error_type = E;

        // Constructors for value
        Expected(const T& val) : m_Data(val) {}
        Expected(T&& val) : m_Data(std::move(val)) {}

        // Constructors for unexpected error
        Expected(const Unexpected<E>& err) : m_Data(err) {}
        Expected(Unexpected<E>&& err) : m_Data(std::move(err)) {}

        // Direct conversion from error code (only if T and E are different)
        template <typename U = E, typename = std::enable_if_t<!std::is_same_v<T, U>>>
        Expected(const U& err) : m_Data(Unexpected<E>(err)) {}

        template <typename U = E, typename = std::enable_if_t<!std::is_same_v<T, U>>>
        Expected(U&& err) : m_Data(Unexpected<E>(std::move(err))) {}

        bool has_value() const {
            return std::holds_alternative<T>(m_Data);
        }

        explicit operator bool() const {
            return has_value();
        }

        const T& value() const {
            if (!has_value()) {
                throw std::runtime_error("Expected does not contain a value");
            }
            return std::get<T>(m_Data);
        }

        T& value() {
            if (!has_value()) {
                throw std::runtime_error("Expected does not contain a value");
            }
            return std::get<T>(m_Data);
        }

        const E& error() const {
            if (has_value()) {
                throw std::runtime_error("Expected does not contain an error");
            }
            return std::get<Unexpected<E>>(m_Data).value();
        }

        E& error() {
            if (has_value()) {
                throw std::runtime_error("Expected does not contain an error");
            }
            return std::get<Unexpected<E>>(m_Data).value();
        }

        const T* operator->() const { return &value(); }
        T* operator->() { return &value(); }

        const T& operator*() const & { return value(); }
        T& operator*() & { return value(); }

        // Monadic chaining operator: and_then
        template <typename F>
        auto and_then(F&& f) const {
            using ReturnType = decltype(f(std::declval<const T&>()));
            if (has_value()) {
                return f(value());
            } else {
                return ReturnType(Unexpected<E>(error()));
            }
        }

        // Monadic chaining operator: transform
        template <typename F>
        auto transform(F&& f) const {
            using NewValueType = decltype(f(std::declval<const T&>()));
            using NewExpectedType = Expected<NewValueType, E>;
            if (has_value()) {
                return NewExpectedType(f(value()));
            } else {
                return NewExpectedType(Unexpected<E>(error()));
            }
        }

    private:
        std::variant<T, Unexpected<E>> m_Data;
    };

    /**
     * @brief Specialization of Expected for void success state.
     */
    template <typename E>
    class Expected<void, E> {
    public:
        using value_type = void;
        using error_type = E;

        Expected() : m_HasValue(true), m_Error() {}
        Expected(const Unexpected<E>& err) : m_HasValue(false), m_Error(err.value()) {}
        Expected(Unexpected<E>&& err) : m_HasValue(false), m_Error(std::move(err.value())) {}

        Expected(const E& err) : m_HasValue(false), m_Error(err) {}
        Expected(E&& err) : m_HasValue(false), m_Error(std::move(err)) {}

        bool has_value() const { return m_HasValue; }
        explicit operator bool() const { return m_HasValue; }

        void value() const {
            if (!m_HasValue) {
                throw std::runtime_error("Expected does not contain a value");
            }
        }

        const E& error() const {
            if (m_HasValue) {
                throw std::runtime_error("Expected does not contain an error");
            }
            return m_Error;
        }

        E& error() {
            if (m_HasValue) {
                throw std::runtime_error("Expected does not contain an error");
            }
            return m_Error;
        }

    private:
        bool m_HasValue;
        E m_Error;
    };

} // namespace eng::core
