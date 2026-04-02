#pragma once

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#define ZoneScopedTransient(name) ZoneTransientN(___tracy_scoped_zone, name, true)
#else
#define FrameMark
#define FrameMarkNamed(x)
#define ZoneScoped
#define ZoneScopedN(x)
#define ZoneScopedTransient(name)
#define ZoneText(x, y)
#define TracyAlloc(ptr, size)
#define TracyFree(ptr)
#endif
