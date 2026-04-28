#pragma once
#include "rydz_ecs/fwd.hpp"
#include "rydz_ecs/storage.hpp"
#include "rydz_math/mod.hpp"
#include "types.hpp"
#include <string_view>
#include <variant>

namespace ai {

using namespace rydz_math;
using namespace ecs;

// ── FSM States ─────────────────────────────────────────────────────────────

struct Idle {
  f32 wait_timer = 0.0f;
};

struct Chase {
  Entity target{};
  f32 lost_timer = 0.0f;
};

struct Attack {
  Entity target{};
  f32 cooldown = 0.0f;
  f32 windup_timer = 0.0f;
  bool committed = false;
};

struct Dead {
  f32 despawn_timer = 2.0f;
};

// ── EnemyFSM component ────────────────────────────────────────────────────

using EnemyState = Variant<Idle, Chase, Attack, Dead>;

struct EnemyFSM {
  using Storage = SparseSetStorage<EnemyFSM>;
  EnemyState current = Idle{};
};

// ── Helper: state name for debug logging ───────────────────────────────────

inline auto state_name(EnemyState const& state) -> std::string_view {
  return std::visit(
    over{
      [](Idle const&) -> std::string_view { return "Idle"; },
      [](Chase const&) -> std::string_view { return "Chase"; },
      [](Attack const&) -> std::string_view { return "Attack"; },
      [](Dead const&) -> std::string_view { return "Dead"; },
    },
    state
  );
}

// ── Enemy configuration ───────────────────────────────────────────────────

struct EnemyConfig {
  using Storage = SparseSetStorage<EnemyConfig>;
  f32 detect_range = 12.0f;
  f32 attack_range = 2.0f;
  f32 chase_speed = 5.0f;
  f32 attack_cooldown = 1.5f;
  f32 attack_windup = 0.4f;
  f32 lost_timeout = 3.0f;
  f32 damage = 20.0f;
};

// ── Health component ──────────────────────────────────────────────────────

struct Health {
  using Storage = SparseSetStorage<Health>;
  f32 current = 100.0f;
  f32 max = 100.0f;

  [[nodiscard]] auto is_dead() const -> bool { return current <= 0.0f; }

  auto take_damage(f32 amount) -> void {
    current = std::max(0.0f, current - amount);
  }
};

// ── Tag components ────────────────────────────────────────────────────────

struct EnemyTag {
  using Storage = SparseSetStorage<EnemyTag>;
};
struct MeleeTag {
  using Storage = SparseSetStorage<MeleeTag>;
};
struct RangedTag {
  using Storage = SparseSetStorage<RangedTag>;
};

// ── Bundles ───────────────────────────────────────────────────────────────

struct MeleeEnemyBundle {
  using T = ecs::Bundle;
  EnemyTag enemy_tag;
  MeleeTag melee_tag;
  EnemyFSM fsm;
  EnemyConfig config;
  Health health;
};

struct RangedEnemyBundle {
  using T = ecs::Bundle;
  EnemyTag enemy_tag;
  RangedTag ranged_tag;
  EnemyFSM fsm;
  EnemyConfig config;
  Health health;
};

// ── Factory helpers ───────────────────────────────────────────────────────

inline auto make_melee_config() -> EnemyConfig {
  return EnemyConfig{
    .detect_range = 10.0f,
    .attack_range = 2.5f,
    .chase_speed = 6.0f,
    .attack_cooldown = 1.2f,
    .attack_windup = 0.3f,
    .lost_timeout = 3.0f,
    .damage = 25.0f,
  };
}

inline auto make_ranged_config() -> EnemyConfig {
  return EnemyConfig{
    .detect_range = 18.0f,
    .attack_range = 12.0f,
    .chase_speed = 3.5f,
    .attack_cooldown = 2.0f,
    .attack_windup = 0.6f,
    .lost_timeout = 4.0f,
    .damage = 15.0f,
  };
}

} // namespace ai
