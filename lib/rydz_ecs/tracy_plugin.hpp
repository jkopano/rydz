#pragma once

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#else
#define FrameMark
#define FrameMarkNamed(x)
#define ZoneScoped
#define ZoneScopedN(x)
#define ZoneText(x, y)
#define TracyAlloc(ptr, size)
#define TracyFree(ptr)
#endif
