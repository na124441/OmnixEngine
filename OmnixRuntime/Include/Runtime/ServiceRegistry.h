#pragma once

#include <unordered_map>
#include <typeindex>
#include <memory>
#include <mutex>

namespace eng::runtime {

    /**
     * @class ServiceRegistry
     * @brief A type-erased singleton locator that provides dynamic lookup of services via interface types.
     */
    class ServiceRegistry {
    public:
        ServiceRegistry() = default;
        ~ServiceRegistry() { Clear(); }

        ServiceRegistry(const ServiceRegistry&) = delete;
        ServiceRegistry& operator=(const ServiceRegistry&) = delete;

        /**
         * @brief Register a service implementation mapped to an interface type.
         */
        template<typename Interface>
        void RegisterService(std::shared_ptr<Interface> service) {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_Services[typeid(Interface)] = std::static_pointer_cast<void>(service);
        }

        /**
         * @brief Retrieve a registered service by interface type.
         * @return std::shared_ptr to the service, or nullptr if not registered.
         */
        template<typename Interface>
        std::shared_ptr<Interface> GetService() const {
            std::lock_guard<std::mutex> lock(m_Mutex);
            auto it = m_Services.find(typeid(Interface));
            if (it != m_Services.end()) {
                return std::static_pointer_cast<Interface>(it->second);
            }
            return nullptr;
        }

        /**
         * @brief Unregister a service by interface type.
         */
        template<typename Interface>
        void UnregisterService() {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_Services.erase(typeid(Interface));
        }

        /**
         * @brief Clear all registered services.
         */
        void Clear() {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_Services.clear();
        }

    private:
        mutable std::mutex m_Mutex;
        std::unordered_map<std::type_index, std::shared_ptr<void>> m_Services;
    };

} // namespace eng::runtime
