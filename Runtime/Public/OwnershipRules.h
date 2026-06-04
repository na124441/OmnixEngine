#pragma once

#include <string>

/**
 * @file OwnershipRules.h
 * @brief Authoritative laws of system ownership, destruction authority, and lifetime tracking inside Omnix Engine.
 *
 * PHILOSOPHY:
 * 1. Single Ownership Principle: Every resource or system must have ONE clear owner.
 * 2. Nested Lifetime Rule: An owned system's lifetime is strictly bounded by its owner's lifetime.
 * 3. Destruction Authority: ONLY the designated owner may delete or reset a subsystem.
 * 4. Safe Observation: Non-owning observers reference systems using raw pointers or weak pointers.
 *    Observers must NEVER delete or hold references that outlive the owner.
 */

namespace eng::runtime {

    enum class OwnershipLevel {
        Authoritative,      // The owner (e.g., EngineRuntime owning IRenderer). Responsible for lifetime and cleanup.
        Collaborative,      // Shared ownership using shared_ptr. Used sparingly for assets/data buffers.
        Observer            // Non-owning reference. Uses raw pointer; lifetime guaranteed to be nested.
    };

    /**
     * @brief Structure to register ownership rules in documentation and code.
     */
    struct OwnershipDeclaration {
        std::string systemName;
        std::string ownerName;
        OwnershipLevel level;
        std::string destructionAuthority;
    };

} // namespace eng::runtime
