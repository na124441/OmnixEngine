#pragma once
#include <functional>
#include <string>

namespace eng::renderer {

/// A single rendering pass.  It owns a name (useful for logs / debug markers)
/// and a callable that will be executed each frame.
struct Pass
{
    std::string                name;      // human‑readable
    std::function<void()>      execute;   // what the pass does

    Pass() = default;
    Pass(std::string n, std::function<void()> fn)
        : name(std::move(n)), execute(std::move(fn)) {}
};

} // namespace eng::renderer
