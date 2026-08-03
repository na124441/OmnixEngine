#pragma once

namespace eng::runtime {

    enum class GameSessionState
    {
        None,
        Starting,
        Playing,
        Paused,
        Completed,
        Failed,
        Restarting
    };

} // namespace eng::runtime
