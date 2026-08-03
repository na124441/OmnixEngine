#pragma once

#include "Runtime/RuntimeContext.h"

namespace eng::runtime {

    class ConsolePanel {
    public:
        void Initialize(RuntimeContext* context);
        void Render();

    private:
        RuntimeContext* m_Context = nullptr;
    };

} // namespace eng::runtime
