#pragma once

// Phase 1: Core
#include "rydz_ecs/access.hpp"
#include "rydz_ecs/entity.hpp"
#include "rydz_ecs/resource.hpp"
#include "rydz_ecs/storage.hpp"
#include "rydz_ecs/ticks.hpp"
#include "rydz_ecs/world.hpp"

// Phase 2: Query & System
#include "rydz_ecs/bundle.hpp"
#include "rydz_ecs/query.hpp"
#include "rydz_ecs/system.hpp"

// Phase 3: Commands & Events
#include "rydz_ecs/command.hpp"
#include "rydz_ecs/event.hpp"

// Phase 4: Schedule & Conditions
#include "rydz_ecs/condition.hpp"
#include "rydz_ecs/schedule.hpp"

// Phase 5: State, Hierarchy, Plugin
#include "rydz_ecs/hierarchy.hpp"
#include "rydz_ecs/plugin.hpp"
#include "rydz_ecs/state.hpp"

// Phase 6: App & Integration
#include "rydz_ecs/app.hpp"
#include "rydz_ecs/time.hpp"

// Rendering Layer
#include "rydz_ecs/asset.hpp"
#include "rydz_ecs/camera3d.hpp"
#include "rydz_ecs/frustum.hpp"
#include "rydz_ecs/light.hpp"
#include "rydz_ecs/lod.hpp"
#include "rydz_ecs/material3d.hpp"
#include "rydz_ecs/mesh3d.hpp"
#include "rydz_ecs/name.hpp"
#include "rydz_ecs/render_batches.hpp"
#include "rydz_ecs/render_config.hpp"
#include "rydz_ecs/render_plugin.hpp"
#include "rydz_ecs/transform.hpp"
#include "rydz_ecs/visibility.hpp"
