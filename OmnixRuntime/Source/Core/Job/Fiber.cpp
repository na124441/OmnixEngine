#include "Core/Job/Fiber.h"
#include <windows.h>
#include <stdexcept>

#ifdef Yield
#undef Yield
#endif

namespace eng::core {

    struct FiberData {
        Fiber::EntryFn entry;
        void* userData;
    };

    static thread_local void* t_MainFiber = nullptr;

    static VOID CALLBACK FiberEntryPoint(LPVOID lpParameter) {
        FiberData* data = static_cast<FiberData*>(lpParameter);
        if (data && data->entry) {
            try {
                data->entry(data->userData);
            } catch (...) {}
        }
        if (t_MainFiber) {
            SwitchToFiber(t_MainFiber);
        }
    }

    Fiber::Fiber(EntryFn entry, void* userData) {
        if (!t_MainFiber) {
            t_MainFiber = ConvertThreadToFiber(nullptr);
            if (!t_MainFiber) {
                t_MainFiber = GetCurrentFiber();
            }
        }

        FiberData* data = new FiberData{ entry, userData };
        m_FiberHandle = CreateFiber(0, FiberEntryPoint, data);
        m_FiberData = data;

        if (!m_FiberHandle) {
            delete data;
            throw std::runtime_error("Failed to create Windows fiber");
        }
    }

    Fiber::~Fiber() {
        if (m_FiberHandle) {
            DeleteFiber(m_FiberHandle);
        }
        delete static_cast<FiberData*>(m_FiberData);
    }

    void Fiber::SwitchTo() {
        if (m_FiberHandle) {
            SwitchToFiber(m_FiberHandle);
        }
    }

    void Fiber::Yield() {
        if (t_MainFiber) {
            SwitchToFiber(t_MainFiber);
        }
    }

} // namespace eng::core
