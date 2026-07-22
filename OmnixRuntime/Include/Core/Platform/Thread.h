#pragma once
#include <thread>
#include <string>
#include <tuple>
#include <utility>
#include <type_traits>
#include "Core/Threading/ThreadUtils.h"

namespace eng::platform {

    class Thread {
    public:
        using id = std::thread::id;
        using native_handle_type = std::thread::native_handle_type;

        Thread() noexcept = default;
        
        ~Thread() {
            if (m_Thread.joinable()) {
                m_Thread.join();
            }
        }

        // Disable copy
        Thread(const Thread&) = delete;
        Thread& operator=(const Thread&) = delete;

        // Enable move
        Thread(Thread&& other) noexcept : m_Thread(std::move(other.m_Thread)) {}
        
        Thread& operator=(Thread&& other) noexcept {
            if (this != &other) {
                if (m_Thread.joinable()) {
                    m_Thread.join();
                }
                m_Thread = std::move(other.m_Thread);
            }
            return *this;
        }

        // Constructor 1: Spawns thread with a custom name and core affinity index
        template <typename Function, typename... Args>
        explicit Thread(const std::string& name, int32_t affinityIndex, Function&& f, Args&&... args) {
            m_Thread = std::thread([
                name,
                affinityIndex,
                fn = std::forward<Function>(f),
                argsTuple = std::make_tuple(std::forward<Args>(args)...)
            ]() mutable {
                if (!name.empty()) {
                    eng::core::SetCurrentThreadName(name.c_str());
                }
                if (affinityIndex >= 0) {
                    eng::core::SetCurrentThreadAffinity(static_cast<uint32_t>(affinityIndex));
                }
                std::apply(fn, std::move(argsTuple));
            });
        }

        // Constructor 2: Standard spawning constructor without name/affinity (SFINAE-disabled for name/affinity arguments)
        template <typename Function, typename... Args,
                  typename = std::enable_if_t<!std::is_same_v<std::decay_t<Function>, std::string> && 
                                              !std::is_convertible_v<std::decay_t<Function>, const char*>>>
        explicit Thread(Function&& f, Args&&... args) {
            m_Thread = std::thread(std::forward<Function>(f), std::forward<Args>(args)...);
        }

        void Join() {
            if (m_Thread.joinable()) {
                m_Thread.join();
            }
        }

        void Detach() {
            if (m_Thread.joinable()) {
                m_Thread.detach();
            }
        }

        bool IsJoinable() const noexcept {
            return m_Thread.joinable();
        }

        id GetId() const noexcept {
            return m_Thread.get_id();
        }

        native_handle_type GetNativeHandle() noexcept {
            return m_Thread.native_handle();
        }

        // Lowercase std::thread compatibility API
        void join() { Join(); }
        void detach() { Detach(); }
        bool joinable() const noexcept { return IsJoinable(); }
        id get_id() const noexcept { return GetId(); }
        native_handle_type native_handle() noexcept { return GetNativeHandle(); }

        std::thread& GetUnderlyingThread() noexcept {
            return m_Thread;
        }

    private:
        std::thread m_Thread;
    };

} // namespace eng::platform
