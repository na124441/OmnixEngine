#pragma once

#include <string>
#include <unordered_map>
#include <variant>
#include <mutex>

namespace eng::runtime {

    enum class CVarFlags : uint32_t {
        None = 0,
        ReadOnly = 1 << 0,
        Cheat = 1 << 1,
        SaveToConfig = 1 << 2
    };

    inline CVarFlags operator|(CVarFlags a, CVarFlags b) noexcept {
        return static_cast<CVarFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }

    struct CVar {
        std::string name;
        std::variant<int, float, bool, std::string> value;
        CVarFlags flags = CVarFlags::None;
        std::string description;
    };

    /**
     * @class CVarSystem
     * @brief Manages engine console variables (CVars) with read-only/cheat flags and string mappings (T1.1.15).
     */
    class CVarSystem {
    public:
        CVarSystem() = default;
        ~CVarSystem() = default;

        CVarSystem(const CVarSystem&) = delete;
        CVarSystem& operator=(const CVarSystem&) = delete;

        void RegisterInt(const std::string& name, int defaultValue, CVarFlags flags, const std::string& desc = "");
        void RegisterFloat(const std::string& name, float defaultValue, CVarFlags flags, const std::string& desc = "");
        void RegisterBool(const std::string& name, bool defaultValue, CVarFlags flags, const std::string& desc = "");
        void RegisterString(const std::string& name, const std::string& defaultValue, CVarFlags flags, const std::string& desc = "");

        [[nodiscard]] const CVar* GetCVar(const std::string& name) const;

        [[nodiscard]] int GetInt(const std::string& name, int defaultValue = 0) const;
        [[nodiscard]] float GetFloat(const std::string& name, float defaultValue = 0.0f) const;
        [[nodiscard]] bool GetBool(const std::string& name, bool defaultValue = false) const;
        [[nodiscard]] std::string GetString(const std::string& name, const std::string& defaultValue = "") const;

        bool SetInt(const std::string& name, int value);
        bool SetFloat(const std::string& name, float value);
        bool SetBool(const std::string& name, bool value);
        bool SetString(const std::string& name, const std::string& value);

        [[nodiscard]] std::string GetValueAsString(const std::string& name) const;
        bool SetValueFromString(const std::string& name, const std::string& valueStr);

        [[nodiscard]] const std::unordered_map<std::string, CVar>& GetCVars() const { return m_CVars; }

    private:
        mutable std::mutex m_Mutex;
        std::unordered_map<std::string, CVar> m_CVars;
    };

} // namespace eng::runtime
