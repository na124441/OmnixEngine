#pragma once

#include "RenderingEngine/Core/Profiling/profiler.h"

#if defined(ENG_ENABLE_PROFILE)
#define OMNIX_PROFILE_SCOPE(name) ENG_PROFILE_SCOPE(name)
#else
#define OMNIX_PROFILE_SCOPE(name) ((void)0)
#endif
