#pragma once
#include "rydz_ecs/rydz_ecs.hpp"
#include "rydz_console/scripting.hpp"
#include "rydz_console/console.hpp"
#include <string>

namespace engine {

    class ConsoleAPI {
    public:
        template <typename Func>
        static void bind_system(
            ecs::ResMut<LuaResource>& lua, ecs::World& world,
            const std::string& command_name, Func ecs_system,
            ecs::ResMut<ConsoleState>* console = nullptr
        ) {
            ecs::World* w_ptr = &world;
            engine::ConsoleState* c_ptr = console ? &(**console) : nullptr;
            lua->vm[command_name] = [w_ptr, c_ptr, command_name, ecs_system]() {
                auto sys = ecs::make_system(ecs_system);
                sys->run(*w_ptr);
                if (c_ptr) c_ptr->log("[API] Wykonano: " + command_name);
                };
        }
    };

    template <typename... LuaArgs>
    struct BindCommand {
        template <typename BuilderFunc>
        static void to(
            ecs::ResMut<LuaResource>& lua, ecs::World& world,
            const std::string& name, BuilderFunc builder,
            ecs::ResMut<ConsoleState>* console = nullptr
        ) {
            ecs::World* w_ptr = &world;
            engine::ConsoleState* c_ptr = console ? &(**console) : nullptr;

            lua->vm[name] = [w_ptr, c_ptr, name, builder](sol::optional<LuaArgs>... args) {

                bool is_valid = (args.has_value() && ...);

                if (!is_valid) {
                    if (c_ptr) {
                        c_ptr->log("[Blad] Komenda '" + name + "' otrzymala zly typ parametru.");
                        c_ptr->log("       Oczekiwano " + std::to_string(sizeof...(LuaArgs)) + " parametru/ow.");
                    }
                    return; 
                }

                auto sys = ecs::make_system(builder(args.value()...));
                sys->run(*w_ptr);

                if (c_ptr) c_ptr->log("[API] Wykonano: " + name);
                };
        }
    };

} 