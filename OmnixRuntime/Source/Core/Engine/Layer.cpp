#include "Core/Engine/Layer.h"

namespace eng::core {

    Layer::Layer(const std::string& debugName)
        : m_DebugName(debugName), m_Enabled(true) {}

    Layer::~Layer() = default;

} // namespace eng::core
