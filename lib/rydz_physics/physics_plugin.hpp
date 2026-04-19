#pragma once

#include "contact_listener.hpp"
#include "physics_world.hpp"
#include "rydz_ecs/plugin.hpp"
#include "systems.hpp"

#include "rydz_ecs/app.hpp"

namespace physics {

enum class PhysicsSet : u8 {
  PreStep,
  Step,
  PostStep,
};

struct PhysicsPlugin : public ecs::IPlugin {
  PhysicsSettings cfg{};

  auto build(ecs::App& app) -> void override {
    app.init_resource<ecs::FixedTime>();

    auto const* existing = app.world().get_resource<PhysicsSettings>();
    PhysicsSettings settings =
      existing != nullptr ? *existing : PhysicsSettings{};

    if (existing == nullptr) {
      app.insert_resource(cfg);
    }

    if (auto* ft = app.world().get_resource<ecs::FixedTime>()) {
      ft->hz = settings.fixed_hz;
    }

    {
      auto pw = PhysicsWorld(settings);
      pw.contact_listener = std::make_unique<PhysicsContactListener>();
      pw.system->SetContactListener(pw.contact_listener.get());
      app.world().insert_resource(std::move(pw));
    }

    app.init_resource<PhysicsInterp>();
    app.add_message<ContactEvent>();

    app.configure_set(
      ecs::FixedUpdate,
      ecs::configure(
        PhysicsSet::PreStep, PhysicsSet::Step, PhysicsSet::PostStep
      )
        .chain()
    );

    app.add_systems(
      PhysicsSet::PreStep,
      ecs::group(
        store_previous_transforms,
        flush_body_despawn_queue,
        flush_body_spawn_queue,
        sync_kinematic_to_jolt,
        apply_forces
      )
        .chain()
    );

    app.add_systems(PhysicsSet::Step, step_simulation);

    app.add_systems(
      PhysicsSet::PostStep,
      ecs::group(sync_transforms, sync_velocities, process_contact_events)
        .chain()
    );
  }
};

} // namespace physics
