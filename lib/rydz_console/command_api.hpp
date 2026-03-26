#pragma once
#include "rydz_console/console.hpp"
#include "rydz_console/scripting.hpp"
#include "rydz_ecs/rydz_ecs.hpp"
#include <sol/sol.hpp>
#include <string>

namespace engine {

class ConsoleAPI {
public:
  template <typename Func>
  static void bind_system(ecs::World &world, const std::string &command_name,
                          Func ecs_system) {
    auto *lua = world.get_resource<LuaResource>();
    auto *console = world.get_resource<ConsoleState>();

    if (!lua)
      return;

    ecs::World *w_ptr = &world;

    lua->vm[command_name] = [w_ptr, console, command_name, ecs_system]() {
      auto sys = ecs::make_system(ecs_system);
      sys->run(*w_ptr);

      if (console) {
        console->log("[API] Wykonano: " + command_name);
      }
    };
  }
};

template <typename... LuaArgs> struct BindCommand {
  template <typename BuilderFunc>
  static void to(ecs::World &world, const std::string &name,
                 BuilderFunc builder) {
    auto *lua = world.get_resource<LuaResource>();
    auto *console = world.get_resource<ConsoleState>();

    if (!lua)
      return;

    ecs::World *w_ptr = &world;

    lua->vm[name] = [w_ptr, console, name,
                     builder](sol::optional<LuaArgs>... args) {
      bool is_valid = (args.has_value() && ...);

      if (!is_valid) {
        if (console) {
          console->log("[Błąd] Komenda '" + name +
                       "' - zły typ lub brak parametrów.");
          console->log("       Oczekiwano " +
                       std::to_string(sizeof...(LuaArgs)) + " argumentów.");
        }
        return;
      }

      auto sys = ecs::make_system(builder(args.value()...));
      sys->run(*w_ptr);

      if (console) {
        console->log("[API] Wykonano: " + name);
      }
    };
  }
};

} // namespace engine
