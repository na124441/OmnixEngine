#pragma once

// Detect the target OS (set by CMake) and expose a macro for the rest of the code.
#if defined(_WIN32) || defined(_WIN64)
#define ENG_PLATFORM_WINDOWS 1
#elif defined(__linux__)
#define ENG_PLATFORM_LINUX   1
#elif defined(__APPLE__) && defined(__MACH__)
#define ENG_PLATFORM_MACOS   1
#else
#error "Unsupported platform"
#endif

// Public include list – just re‑export the three subsystems.
#include "window/Window.h"
#include "input/Input.h"
#include "time/Timer.h"
