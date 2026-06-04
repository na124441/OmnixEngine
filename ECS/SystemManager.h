#pragma once

#include <memory>
#include <unordered_map>
#include <string>
#include <set>
#include <cassert>
#include <typeinfo>
#include "ECSconfig.h"

// Base System class
class System {
public:
    std::set<Entity> m_Entities;
    virtual ~System() = default;
    virtual std::shared_ptr<System> Clone() const {
        return nullptr;
    }
};

// SystemManager
class SystemManager {
public:
    std::unique_ptr<SystemManager> Clone() const {
        auto clone = std::make_unique<SystemManager>();
        clone->m_Signatures = m_Signatures;
        for (const auto& pair : m_Systems) {
            if (pair.second) {
                auto clonedSys = pair.second->Clone();
                if (clonedSys) {
                    clone->m_Systems[pair.first] = clonedSys;
                }
            }
        }
        return clone;
    }

    template<typename T>
    std::shared_ptr<T> RegisterSystem() {
        std::string typeName = std::string(typeid(T).name());

        assert(m_Systems.find(typeName) == m_Systems.end() &&
               "Registering system more than once.");

        auto system = std::make_shared<T>();
        m_Systems[typeName] = system;
        return system;
    }

    template<typename T>
    void SetSignature(Signature signature) {
        std::string typeName = std::string(typeid(T).name());

        assert(m_Systems.find(typeName) != m_Systems.end() &&
               "System used before registered.");

        m_Signatures[typeName] = signature;
    }

    template<typename T>
    std::shared_ptr<T> GetSystem() {
        std::string typeName = std::string(typeid(T).name());

        auto it = m_Systems.find(typeName);
        if (it == m_Systems.end()) {
            return nullptr;
        }

        return std::static_pointer_cast<T>(it->second);
    }

    void EntitySignatureChanged(Entity entity, Signature entitySignature) {
        for (auto const& pair : m_Systems) {
            auto const& type = pair.first;
            auto const& system = pair.second;
            auto const& systemSignature = m_Signatures[type];

            if ((entitySignature & systemSignature) == systemSignature) {
                system->m_Entities.insert(entity);
            } else {
                system->m_Entities.erase(entity);
            }
        }
    }

    void EntityDestroyed(Entity entity) {
        for (auto const& pair : m_Systems) {
            auto const& system = pair.second;
            system->m_Entities.erase(entity);
        }
    }

private:
    std::unordered_map<std::string, Signature> m_Signatures;
    std::unordered_map<std::string, std::shared_ptr<System>> m_Systems;
};
