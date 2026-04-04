#pragma once
#include "math.hpp"
#include "rl.hpp"
#include "rydz_ecs/fwd.hpp"
#include "rydz_ecs/rydz_ecs.hpp"
#include "rydz_ecs/storage.hpp"
#include "rydz_graphics/render_plugin.hpp"
#include "rydz_graphics/rydz_graphics.hpp"
#include <algorithm>
#include <print>

using namespace ecs;
using namespace math;

inline void scene_plugin(App &app) { app.add_plugin(Input::install); }
