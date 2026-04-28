#pragma once
#include "enemy_fsm.hpp"
#include "rydz_ecs/mod.hpp"
#include "rydz_log/mod.hpp"
#include "rydz_math/mod.hpp"
#include <print>

namespace ai {

using namespace ecs;
using namespace rydz_math;

// ── Helper: log state transition ───────────────────────────────────────────

inline void log_transition(
  Entity entity, EnemyState const& from, EnemyState const& to
) {
  info(
    "[AI] Entity ({},{}) : {} -> {}",
    entity.index(),
    entity.generation(),
    state_name(from),
    state_name(to)
  );
}

// Sets new state and logs the transition. Returns true if state actually
// changed.
inline auto try_transition(EnemyFSM& fsm, EnemyState new_state, Entity entity)
  -> bool {
  auto old_name = state_name(fsm.current);
  auto new_name = state_name(new_state);
  if (old_name == new_name) {
    return false;
  }
  log_transition(entity, fsm.current, new_state);
  fsm.current = std::move(new_state);
  return true;
}

// ── System 1: Perception — decide state transitions ────────────────────────

// Helper to get distance to player (XZ plane only for isometric game)
inline auto distance_xz(Vec3 const& a, Vec3 const& b) -> f32 {
  Vec3 diff = a - b;
  diff.y = 0.0f; // ignore vertical
  return diff.length();
}

inline void enemy_perception_system(
  Query<
    Entity,
    Mut<EnemyFSM>,
    Mut<Health>,
    EnemyConfig,
    Transform,
    With<EnemyTag>,
    Without<Player>> enemy_query,
  Query<Entity, Transform, With<Player>, Without<EnemyTag>> player_query,
  Res<Time> time
) {
  // Find player position
  Vec3 player_pos = Vec3::ZERO;
  Entity player_entity{};
  bool player_found = false;
  for (auto [entity, pt] : player_query.iter()) {
    player_pos = pt->translation;
    player_entity = entity;
    player_found = true;
    break;
  }

  if (!player_found) {
    return;
  }

  f32 dt = time->delta_seconds;

  for (auto [entity, fsm, health, config, transform] : enemy_query.iter()) {
    // Death takes priority over everything
    if (health->is_dead()) {
      if (!std::holds_alternative<Dead>(fsm->current)) {
        try_transition(*fsm, Dead{}, entity);
      }
      continue;
    }

    f32 dist = distance_xz(transform->translation, player_pos);

    std::visit(
      over{
        // ── Idle ──────────────────────────────────────────────
        [&](Idle& idle) {
          idle.wait_timer += dt;
          // Detect player
          if (dist <= config->detect_range) {
            try_transition(*fsm, Chase{.target = player_entity}, entity);
          }
        },

        // ── Chase ─────────────────────────────────────────────
        [&](Chase& chase) {
          if (dist <= config->attack_range) {
            // In attack range -> attack
            try_transition(
              *fsm,
              Attack{
                .target = chase.target,
                .cooldown = 0.0f,
                .windup_timer = config->attack_windup,
                .committed = false,
              },
              entity
            );
          } else if (dist > config->detect_range) {
            // Lost the player
            chase.lost_timer += dt;
            if (chase.lost_timer >= config->lost_timeout) {
              try_transition(*fsm, Idle{}, entity);
            }
          } else {
            chase.lost_timer = 0.0f;
          }
        },

        // ── Attack ────────────────────────────────────────────
        [&](Attack&) {
          // Attack lifecycle (windup -> commit -> cooldown -> chase)
          // is fully managed by enemy_attack_system.
          // We don't interrupt it here to prevent cooldown bypassing
          // or attack stuttering.
        },

        // ── Dead ──────────────────────────────────────────────
        [&](Dead&) {
          // no transitions from Dead
        },
      },
      fsm->current
    );
  }
}

// ── System 2: Movement — move enemies based on state ───────────────────────

inline void enemy_movement_system(
  Query<Mut<Transform>, EnemyFSM, EnemyConfig, With<EnemyTag>, Without<Player>>
    enemy_query,
  Query<Transform, With<Player>, Without<EnemyTag>> player_query,
  Res<Time> time
) {
  Vec3 player_pos = Vec3::ZERO;
  bool player_found = false;
  for (auto [pt] : player_query.iter()) {
    player_pos = pt->translation;
    player_found = true;
    break;
  }

  if (!player_found) {
    return;
  }

  f32 dt = time->delta_seconds;

  for (auto [transform, fsm, config] : enemy_query.iter()) {
    std::visit(
      over{
        // Idle: no movement
        [](Idle const&) {},

        // Chase: move toward player (simple steering)
        [&](Chase const&) {
          Vec3 dir = player_pos - transform->translation;
          dir.y = 0.0f; // stay on ground plane
          if (dir.length_sq() > 0.01f) {
            dir = dir.normalized();
            transform->translation =
              transform->translation + dir * (config->chase_speed * dt);

            // Face the player (rotate on Y axis)
            transform->look_at(
              Vec3(player_pos.x, transform->translation.y, player_pos.z)
            );
          }
        },

        // Attack: slight tracking during windup, stop when committed
        [&](Attack const& attack) {
          if (!attack.committed) {
            // Slow tracking during windup
            Vec3 dir = player_pos - transform->translation;
            dir.y = 0.0f;
            if (dir.length_sq() > 0.01f) {
              transform->look_at(
                Vec3(player_pos.x, transform->translation.y, player_pos.z)
              );
            }
          }
        },

        // Dead: no movement
        [](Dead const&) {},
      },
      fsm->current
    );
  }
}

// ── System 3: Attack — handle windup, commit, damage, cooldown ─────────────

inline void enemy_attack_system(
  Query<
    Entity,
    Mut<EnemyFSM>,
    EnemyConfig,
    Transform,
    With<EnemyTag>,
    Without<Player>> enemy_query,
  Query<Mut<Health>, Transform, With<Player>, Without<EnemyTag>> player_query,
  Res<Time> time
) {
  // Get player health (mutable) and position
  Health* player_health = nullptr;
  Vec3 player_pos = Vec3::ZERO;
  for (auto [hp, pt] : player_query.iter()) {
    player_health = hp;
    player_pos = pt->translation;
    break;
  }

  f32 dt = time->delta_seconds;

  for (auto [entity, fsm, config, transform] : enemy_query.iter()) {
    if (auto* attack = std::get_if<Attack>(&fsm->current)) {
      if (!attack->committed) {
        // Windup phase
        attack->windup_timer -= dt;
        if (attack->windup_timer <= 0.0f) {
          attack->committed = true;
          info("[AI] Enemy {} attacks", entity.index());

          // Deal damage to player
          if (player_health != nullptr) {
            // f32 dist = distance_xz(transform->translation, player_pos);
            // if (dist <= config->attack_range * 1.3f) {
            //   player_health->take_damage(config->damage);
            //   info(
            //     "[AI] Enemy {} attack hit! Player HP: {:.0f}/{:.0f}",
            //     entity.index(),
            //     player_health->current,
            //     player_health->max
            //   );
            // } else {
            //   info(
            //     "[AI] Enemy {} attack missed! Player out of range.",
            //     entity.index()
            //   );
            // }
          } else {
            info("[AI] Player has no hp");
          }
        }
      } else {
        // Cooldown phase after attack
        attack->cooldown += dt;
        if (attack->cooldown >= config->attack_cooldown) {
          // Return to chase
          try_transition(*fsm, Chase{.target = attack->target}, entity);
        }
      }
    }
  }
}

// ── System 4: Death — despawn timer ────────────────────────────────────────

inline void enemy_death_system(
  Query<Entity, Mut<EnemyFSM>, Mut<Transform>, With<EnemyTag>> query,
  Cmd cmd,
  Res<Time> time
) {
  f32 dt = time->delta_seconds;

  for (auto [entity, fsm, transform] : query.iter()) {
    if (auto* dead = std::get_if<Dead>(&fsm->current)) {
      dead->despawn_timer -= dt;

      // Sink into ground as visual feedback
      transform->translation.y -= 0.5f * dt;

      if (dead->despawn_timer <= 0.0f) {
        info("[AI] Enemy {} despawned", entity.index());
        cmd.despawn(entity);
      }
    }
  }
}

} // namespace ai
