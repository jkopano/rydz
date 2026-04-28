#pragma once
#include "enemy_fsm.hpp"
#include "enemy_systems.hpp"
#include "rydz_ecs/mod.hpp"
#include "rydz_graphics/mod.hpp"
#include "rydz_log/mod.hpp"

namespace ai {

using namespace ecs;

// ── Test spawn system — ręcznie ustawione pozycje do testów ────────────────

inline void spawn_test_enemies(
  Cmd cmd,
  ResMut<Assets<ecs::Mesh>> meshes,
  ResMut<Assets<ecs::Material>> materials,
  NonSendMarker
) {
  // 1. Melee enemy — czerwony cube
  auto melee_mesh = meshes->add(mesh::cube(0.8f, 1.6f, 0.8f));
  auto melee_mat = materials->add(StandardMaterial::from_color(Color::RED));

  cmd.spawn(
    MeleeEnemyBundle{
      .enemy_tag = {},
      .melee_tag = {},
      .fsm = EnemyFSM{.current = Idle{}},
      .config = make_melee_config(),
      .health = Health{.current = 100.0f, .max = 100.0f},
    },
    Mesh3d{melee_mesh},
    MeshMaterial3d{melee_mat},
    Transform::from_xyz(5.0f, 0.8f, 5.0f)
  );

  // 2. Ranged enemy — niebieski cube (wyższy i chudszy)
  auto ranged_mesh = meshes->add(mesh::cube(0.6f, 1.8f, 0.6f));
  auto ranged_mat = materials->add(StandardMaterial::from_color(Color::BLUE));

  cmd.spawn(
    RangedEnemyBundle{
      .enemy_tag = {},
      .ranged_tag = {},
      .fsm = EnemyFSM{.current = Idle{}},
      .config = make_ranged_config(), // Używa dużego attack_range (12.0)
      .health = Health{.current = 60.0f, .max = 60.0f},
    },
    Mesh3d{ranged_mesh},
    MeshMaterial3d{ranged_mat},
    Transform::from_xyz(-6.0f, 0.9f, 3.0f)
  );

  info("[AI] Spawned 1 melee (RED) and 1 ranged (BLUE) test enemies");
}

// ── Plugin ─────────────────────────────────────────────────────────────────

inline void enemy_ai_plugin(App& app) {
  // Spawn test enemies at startup
  app.add_systems(ScheduleLabel::Startup, spawn_test_enemies);

  // AI systems — chained: perception → movement → attack → death
  app.add_systems(
    ScheduleLabel::Update,
    group(
      enemy_perception_system,
      enemy_movement_system,
      enemy_attack_system,
      enemy_death_system
    )
      .chain()
  );
}

} // namespace ai
