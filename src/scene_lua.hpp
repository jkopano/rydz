#pragma once
#include "math.hpp"
#include "rl.hpp"
#include "rydz_camera/mod.hpp"
#include "rydz_console/console.hpp"
#include "rydz_console/scripting.hpp"
#include "rydz_ecs/fwd.hpp"
#include "rydz_ecs/mod.hpp"
#include "rydz_ecs/schedule.hpp"
#include "rydz_ecs/storage.hpp"
#include "rydz_graphics/mod.hpp"
#include "rydz_graphics/render_plugin.hpp"
#include "rydz_scripting/lua_resource.hpp"
#include "rydz_scripting/lua_system_registry.hpp"
#include "rydz_scripting/script_scheduler.hpp"
#include <filesystem>
#include <print>

using namespace ecs;
using namespace math;

// ── Run condition ─────────────────────────────────────────────────────────────

inline bool is_gameplay_active(Res<engine::ConsoleState> console) {
    return !console->is_open;
}

// ── Startup: środowisko 3D (minimalne — oświetlenie + podłoga) ───────────────

inline void setup_camera(Cmd cmd, NonSendMarker) {
    cmd.spawn(IsometricCameraBundle::setup(
        Vec3::sZero(), Vec3(10.0f, 10.0f, 10.0f), 20.0f, 12.0f));
}

inline void setup_lighting(Cmd cmd, NonSendMarker) {
    cmd.spawn(AmbientLight{
        .color = {60, 60, 70, 255},
        .intensity = 0.35f,
        });
    cmd.spawn(DirectionalLight{
        .color = {255, 244, 220, 255},
        .direction = Vec3(-0.6f, -1.0f, -0.4f).Normalized(),
        .intensity = 0.9f,
        });
}

inline void spawn_ground(Cmd cmd,
    ResMut<Assets<ecs::Mesh>> meshes,
    ResMut<Assets<ecs::Texture>> textures,
    ResMut<Assets<ecs::Material>> materials,
    NonSendMarker) {
    auto plane_h = meshes->add(mesh::plane(20.0f, 20.0f, 1, 1));
    auto plane_mat = materials->add(StandardMaterial::from_texture(
        textures->add(gl::load_texture("res/textures/brick.png"))));
    cmd.spawn(Mesh3d{ plane_h }, MeshMaterial3d{ plane_mat }, Transform{});
}

// ── Startup: inicjalizacja modułu skryptowania ────────────────────────────────
//
// WAŻNE: ten system musi wykonać się PRZED lua_startup_runner.
// Kolejność rejestracji w add_systems(Startup, ...) wyznacza kolejność
// wykonania — rejestrujemy go jako pierwszy startup system skryptowy.

inline void init_lua_scripting(ecs::World& world) {
    auto* lua = world.get_resource<engine::LuaResource>();
    auto* reg = world.get_resource<scripting::LuaSystemRegistry>();
    if (!lua) {
        fprintf(stderr, "[Scripting] Brak LuaResource!\n");
        return;
    }

    // 1. Zarejestruj całe API silnika (metatabelki, Components, Input, Time, Rydz, Schedule)
    scripting::register_rydz_api(lua->vm);

    // 2. Eksponuj _world — musi być przed luaL_dofile bo skrypty mogą go użyć
    scripting::expose_world_global(lua->vm, &world);

    // 3. Ustaw wskaźnik do rejestru systemów — musi być przed luaL_dofile
    //    bo skrypty wywołują Rydz.register_system() podczas ładowania
    if (reg) {
        lua_pushlightuserdata(lua->vm, reg);
        lua_setfield(lua->vm, LUA_REGISTRYINDEX, "_sys_registry");
    }

    // 4. Załaduj wszystkie pliki .lua z res/scripts/ (nierekurencyjnie)
    const std::string scripts_dir = "res/scripts/";
    if (!std::filesystem::exists(scripts_dir)) {
        fprintf(stderr, "[Scripting] Katalog '%s' nie istnieje!\n", scripts_dir.c_str());
        return;
    }

    for (auto& entry : std::filesystem::directory_iterator(scripts_dir)) {
        if (entry.path().extension() != ".lua") continue;

        std::string path = entry.path().string();
        fprintf(stdout, "[Scripting] Ladowanie: %s\n", path.c_str());

        if (luaL_dofile(lua->vm, path.c_str()) != LUA_OK) {
            fprintf(stderr, "[Lua] Blad w %s:\n  %s\n",
                path.c_str(),
                lua_tostring(lua->vm, -1));
            lua_pop(lua->vm, 1);
            // Kontynuujemy — błąd w jednym skrypcie nie blokuje pozostałych
        }
    }
}

// ── Plugin ────────────────────────────────────────────────────────────────────

inline void scene_lua_plugin(App& app) {
    // Pluginy infrastrukturalne
    app.add_plugin(Input::install);
    app.add_plugin(system_multithreading({ true }));
    app.add_plugin(engine::scripting_plugin); // tworzy engine::LuaResource
    app.add_plugin(engine::console_plugin);   // tworzy ConsoleState
    app.add_plugin(camera_plugin);

    // Zasób rejestru systemów Lua — musi być wstawiony przed Startup
    app.insert_resource(scripting::LuaSystemRegistry{});

    // ── Startup ──────────────────────────────────────────────────────────────

    // Środowisko 3D
    app.add_systems(Startup, setup_camera);
    app.add_systems(Startup, setup_lighting);
    app.add_systems(Startup, spawn_ground);
    // Nie ma spawn_player — gracz tworzony jest przez Lua (Rydz.on_startup)

    // Inicjalizacja Lua — MUSI być przed lua_startup_runner
    app.add_systems(ecs::ScheduleLabel::Startup, init_lua_scripting);

    // Uruchomienie systemów startowych zarejestrowanych przez skrypty
    app.add_systems(ecs::ScheduleLabel::Startup, scripting::lua_startup_runner);

    // ── Update ───────────────────────────────────────────────────────────────

    // Uruchomienie systemów update zarejestrowanych przez skrypty
    // (run_if nie jest tu potrzebne — skrypt sam może sprawdzić stan konsoli)
    app.add_systems(ecs::ScheduleLabel::Update, scripting::lua_update_runner);

    // Konsola — renderowanie na końcu
    app.add_systems(RenderPassSet::Cleanup,
        group(engine::ConsoleRenderSystem).before(FramePass::end));
}