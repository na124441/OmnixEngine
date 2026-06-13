#pragma once
#include "Runtime/Public/RuntimeContext.h"

namespace eng::runtime {

    class ImportLogPanel {
    public:
        void Initialize(RuntimeContext* context);
        void Render();

    private:
        RuntimeContext* m_Context = nullptr;
    };

} // namespace eng::runtime
