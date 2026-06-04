#include "Runtime/Public/OwnershipValidation.h"
#include "Core/Logger.h"
#include <vector>
#include <string>
#include <algorithm>
#include <mutex>

namespace eng::runtime {

    static std::vector<std::string> g_StartupSequence;
    static std::vector<std::string> g_ShutdownSequence;
    static std::mutex g_ValidationMutex;

    // The authoritative sequence of dynamic subsystems
    static const std::vector<std::string> k_AuthoritativeOrder = {
        "Input",
        "ECS",
        "Scheduler",
        "Renderer",
        "Assets",
        "Scene"
    };

    void RegisterSystemStartup(const std::string& name) {
        std::lock_guard<std::mutex> lock(g_ValidationMutex);
        g_StartupSequence.push_back(name);
        LOG_INFO("[Validation] Registered Startup: {}", name);
    }

    void RegisterSystemShutdown(const std::string& name) {
        std::lock_guard<std::mutex> lock(g_ValidationMutex);
        g_ShutdownSequence.push_back(name);
        LOG_INFO("[Validation] Registered Shutdown: {}", name);
    }

    bool ValidateExecutionSequence() {
        std::lock_guard<std::mutex> lock(g_ValidationMutex);
        bool valid = true;

        // 1. Validate Startup Order
        int lastIndex = -1;
        for (const auto& sys : g_StartupSequence) {
            auto it = std::find(k_AuthoritativeOrder.begin(), k_AuthoritativeOrder.end(), sys);
            if (it != k_AuthoritativeOrder.end()) {
                int index = std::distance(k_AuthoritativeOrder.begin(), it);
                if (index < lastIndex) {
                    LOG_ERROR("[Validation] STARTUP ORDER VIOLATION: '{}' started up out of order!", sys);
                    valid = false;
                }
                lastIndex = index;
            }
        }

        // 2. Validate Shutdown Order (must be exact reverse of Startup Order)
        if (g_StartupSequence.size() != g_ShutdownSequence.size()) {
            LOG_ERROR("[Validation] LIFECYCLE MISMATCH: Registered startup count ({}) does not match shutdown count ({}).", 
                      g_StartupSequence.size(), g_ShutdownSequence.size());
            valid = false;
        } else {
            size_t count = g_StartupSequence.size();
            for (size_t i = 0; i < count; ++i) {
                const auto& startupSys = g_StartupSequence[i];
                const auto& shutdownSys = g_ShutdownSequence[count - 1 - i];
                if (startupSys != shutdownSys) {
                    LOG_ERROR("[Validation] SHUTDOWN ORDER VIOLATION: Startup system '{}' at index {} was not matched by shutdown system '{}' at reverse index.",
                              startupSys, i, shutdownSys);
                    valid = false;
                }
            }
        }

        if (valid) {
            LOG_INFO("[Validation] Lifecycle sequence validated successfully. Startup and shutdown orders are deterministic and correct!");
        } else {
            LOG_ERROR("[Validation] Lifecycle sequence validation failed!");
        }

        return valid;
    }

} // namespace eng::runtime
