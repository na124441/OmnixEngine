#pragma once
#include <string>
#include <vector>
#include "ECS/ECSconfig.h"

#ifndef OMNIX_ENTITY_DEFINED
#define OMNIX_ENTITY_DEFINED
using Entity = std::uint32_t;
constexpr Entity INVALID_ENTITY = 0;
#endif

class Scene;

namespace eng::runtime {

    enum class ValidationSeverity {
        Info,
        Warning,
        Error,
        Fatal
    };

    struct ValidationResult {
        ValidationSeverity Severity;
        std::string Message;
        Entity RelatedEntity = INVALID_ENTITY;
        std::string ComponentName;
    };

    class GameplayValidator {
    public:
        GameplayValidator() = default;
        ~GameplayValidator() = default;

        std::vector<ValidationResult> ValidateScene(Scene& scene);
        bool HasFatalErrors(const std::vector<ValidationResult>& results) const;
    };

} // namespace eng::runtime
