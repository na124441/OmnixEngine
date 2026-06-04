#include "Timer.h"

Timer::TimePoint Timer::s_startTime = Timer::Clock::now();
Timer::TimePoint Timer::s_lastTime = Timer::Clock::now();
double          Timer::s_deltaSec = 0.0;

void Timer::Init()
{
    s_startTime = Clock::now();
    s_lastTime = s_startTime;
    s_deltaSec = 0.0;
}

void Timer::Update()
{
    TimePoint now = Clock::now();
    std::chrono::duration<double> diff = now - s_lastTime;
    s_deltaSec = diff.count();
    s_lastTime = now;
}

double Timer::GetElapsedSeconds()
{
    return std::chrono::duration<double>(Clock::now() - s_startTime).count();
}

double Timer::GetDeltaSeconds()
{
    return s_deltaSec;
}
