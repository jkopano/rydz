#pragma once
#include "math.hpp"
#include "rydz_ecs/fwd.hpp"
#include "rydz_ecs/mod.hpp"
#include "rydz_ecs/storage.hpp"
#include "rydz_graphics/render_plugin.hpp"
#include "rydz_graphics/mod.hpp"
#include <algorithm>
#include <print>

using namespace ecs;
using namespace math;

inline void scene_plugin(App &app) { app.add_plugin(Input::install); }
