#pragma once

#include <chrono>

/**
 * @brief High‑resolution timer used by the engine.
 *
 * The timer is a **static singleton** – no instances are required.
 * It stores the start time, the time of the last frame and the
 * delta‑time (seconds) between the two most recent calls to
 * Timer::Update().
 *
 * The API mirrors the pseudocode from the design notes:
 *   - Init()          – called once at engine bootstrap
 *   - Update()        – called once per frame
 *   - GetDeltaSeconds() – returns the last frame delta
 *   - GetElapsedSeconds() – time since Init()
 */
class Timer
{
public:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = Clock::time_point;

    /** Initialise the timer (engine start). */
    static void Init();

    /** Update the timer – must be called once per frame. */
    static void Update();

    /** Seconds elapsed since Init() was called. */
    static double GetElapsedSeconds();

    /** Delta time (seconds) between the last two Update() calls. */
    static double GetDeltaSeconds();

private:
    static TimePoint s_startTime;   ///< moment of Init()
    static TimePoint s_lastTime;    ///< moment of the previous Update()
    static double    s_deltaSec;    ///< cached delta time
};
