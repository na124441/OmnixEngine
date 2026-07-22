#pragma once
#include <cstdint>
#include "Core/Error/ResultCode.h"

namespace eng::core {

    /* --------------------------------------------------------------------
     * 2️⃣  The `Result` wrapper – provides a convenient, expressive API.
     * -------------------------------------------------------------------- */
    class Result {
    public:
        // ----------- Constructors ------------------------------------------------
        constexpr Result() noexcept                     // default → Success
            : m_Code(ResultCode::Success) {
        }

        constexpr explicit Result(ResultCode c) noexcept
            : m_Code(c) {
        }

        // ----------- Query API --------------------------------------------------
        constexpr bool IsSuccess() const noexcept { return m_Code == ResultCode::Success; }
        constexpr bool IsFailure() const noexcept { return !IsSuccess(); }

        constexpr ResultCode Code() const noexcept { return m_Code; }

        // ----------- Implicit conversion to bool – enables `if (res) …` ---------
        explicit constexpr operator bool() const noexcept { return IsSuccess(); }

        // ----------- Equality helpers (useful for `ENG_RETURN_IF_FAILED`) ------
        constexpr bool operator==(ResultCode rhs) const noexcept { return m_Code == rhs; }
        constexpr bool operator!=(ResultCode rhs) const noexcept { return !(*this == rhs); }

    private:
        ResultCode m_Code;        // stored as a 16‑bit value – fits in a register.
    };

    /* --------------------------------------------------------------------
     * 3️⃣  Helper macro for “early‑exit on failure” – shortens boiler‑plate.
     * -------------------------------------------------------------------- */
#define ENG_RETURN_IF_FAILED(expr)                                 \
    do {                                                           \
        ::eng::core::Result _r = (expr);                           \
        if (_r.IsFailure()) return _r;                            \
    } while (0)

} // namespace eng::core
