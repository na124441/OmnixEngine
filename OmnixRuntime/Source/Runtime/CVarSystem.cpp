#include "Runtime/CVarSystem.h"
#include "Core/Logging/Logger.h"
#include <algorithm>

namespace eng::runtime {

    void CVarSystem::RegisterInt(const std::string& name, int defaultValue, CVarFlags flags, const std::string& desc) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        if (m_CVars.find(name) != m_CVars.end()) return;
        m_CVars[name] = CVar{ name, defaultValue, flags, desc };
    }

    void CVarSystem::RegisterFloat(const std::string& name, float defaultValue, CVarFlags flags, const std::string& desc) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        if (m_CVars.find(name) != m_CVars.end()) return;
        m_CVars[name] = CVar{ name, defaultValue, flags, desc };
    }

    void CVarSystem::RegisterBool(const std::string& name, bool defaultValue, CVarFlags flags, const std::string& desc) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        if (m_CVars.find(name) != m_CVars.end()) return;
        m_CVars[name] = CVar{ name, defaultValue, flags, desc };
    }

    void CVarSystem::RegisterString(const std::string& name, const std::string& defaultValue, CVarFlags flags, const std::string& desc) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        if (m_CVars.find(name) != m_CVars.end()) return;
        m_CVars[name] = CVar{ name, defaultValue, flags, desc };
    }

    const CVar* CVarSystem::GetCVar(const std::string& name) const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        auto it = m_CVars.find(name);
        if (it != m_CVars.end()) {
            return &it->second;
        }
        return nullptr;
    }

    int CVarSystem::GetInt(const std::string& name, int defaultValue) const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        auto it = m_CVars.find(name);
        if (it != m_CVars.end() && std::holds_alternative<int>(it->second.value)) {
            return std::get<int>(it->second.value);
        }
        return defaultValue;
    }

    float CVarSystem::GetFloat(const std::string& name, float defaultValue) const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        auto it = m_CVars.find(name);
        if (it != m_CVars.end() && std::holds_alternative<float>(it->second.value)) {
            return std::get<float>(it->second.value);
        }
        return defaultValue;
    }

    bool CVarSystem::GetBool(const std::string& name, bool defaultValue) const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        auto it = m_CVars.find(name);
        if (it != m_CVars.end() && std::holds_alternative<bool>(it->second.value)) {
            return std::get<bool>(it->second.value);
        }
        return defaultValue;
    }

    std::string CVarSystem::GetString(const std::string& name, const std::string& defaultValue) const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        auto it = m_CVars.find(name);
        if (it != m_CVars.end() && std::holds_alternative<std::string>(it->second.value)) {
            return std::get<std::string>(it->second.value);
        }
        return defaultValue;
    }

    bool CVarSystem::SetInt(const std::string& name, int value) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        auto it = m_CVars.find(name);
        if (it != m_CVars.end() && std::holds_alternative<int>(it->second.value)) {
            if (static_cast<uint32_t>(it->second.flags) & static_cast<uint32_t>(CVarFlags::ReadOnly)) {
                CORE_LOG_WARN("[CVarSystem] Refused to modify ReadOnly CVar: %s", name.c_str());
                return false;
            }
            it->second.value = value;
            return true;
        }
        return false;
    }

    bool CVarSystem::SetFloat(const std::string& name, float value) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        auto it = m_CVars.find(name);
        if (it != m_CVars.end() && std::holds_alternative<float>(it->second.value)) {
            if (static_cast<uint32_t>(it->second.flags) & static_cast<uint32_t>(CVarFlags::ReadOnly)) {
                CORE_LOG_WARN("[CVarSystem] Refused to modify ReadOnly CVar: %s", name.c_str());
                return false;
            }
            it->second.value = value;
            return true;
        }
        return false;
    }

    bool CVarSystem::SetBool(const std::string& name, bool value) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        auto it = m_CVars.find(name);
        if (it != m_CVars.end() && std::holds_alternative<bool>(it->second.value)) {
            if (static_cast<uint32_t>(it->second.flags) & static_cast<uint32_t>(CVarFlags::ReadOnly)) {
                CORE_LOG_WARN("[CVarSystem] Refused to modify ReadOnly CVar: %s", name.c_str());
                return false;
            }
            it->second.value = value;
            return true;
        }
        return false;
    }

    bool CVarSystem::SetString(const std::string& name, const std::string& value) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        auto it = m_CVars.find(name);
        if (it != m_CVars.end() && std::holds_alternative<std::string>(it->second.value)) {
            if (static_cast<uint32_t>(it->second.flags) & static_cast<uint32_t>(CVarFlags::ReadOnly)) {
                CORE_LOG_WARN("[CVarSystem] Refused to modify ReadOnly CVar: %s", name.c_str());
                return false;
            }
            it->second.value = value;
            return true;
        }
        return false;
    }

    std::string CVarSystem::GetValueAsString(const std::string& name) const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        auto it = m_CVars.find(name);
        if (it != m_CVars.end()) {
            if (std::holds_alternative<int>(it->second.value)) {
                return std::to_string(std::get<int>(it->second.value));
            } else if (std::holds_alternative<float>(it->second.value)) {
                return std::to_string(std::get<float>(it->second.value));
            } else if (std::holds_alternative<bool>(it->second.value)) {
                return std::get<bool>(it->second.value) ? "true" : "false";
            } else if (std::holds_alternative<std::string>(it->second.value)) {
                return std::get<std::string>(it->second.value);
            }
        }
        return "";
    }

    bool CVarSystem::SetValueFromString(const std::string& name, const std::string& valueStr) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        auto it = m_CVars.find(name);
        if (it == m_CVars.end()) return false;

        if (static_cast<uint32_t>(it->second.flags) & static_cast<uint32_t>(CVarFlags::ReadOnly)) {
            CORE_LOG_WARN("[CVarSystem] Refused to modify ReadOnly CVar: %s", name.c_str());
            return false;
        }

        try {
            if (std::holds_alternative<int>(it->second.value)) {
                it->second.value = std::stoi(valueStr);
            } else if (std::holds_alternative<float>(it->second.value)) {
                it->second.value = std::stof(valueStr);
            } else if (std::holds_alternative<bool>(it->second.value)) {
                std::string lowerStr = valueStr;
                std::transform(lowerStr.begin(), lowerStr.end(), lowerStr.begin(), ::tolower);
                it->second.value = (lowerStr == "true" || lowerStr == "1" || lowerStr == "on");
            } else if (std::holds_alternative<std::string>(it->second.value)) {
                it->second.value = valueStr;
            }
            return true;
        } catch (...) {
            CORE_LOG_WARN("[CVarSystem] Failed to parse value '%s' for CVar: %s", valueStr.c_str(), name.c_str());
            return false;
        }
    }

} // namespace eng::runtime
