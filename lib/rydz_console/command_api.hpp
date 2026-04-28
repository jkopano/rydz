#pragma once
#include "rydz_console/console.hpp"
#include "rydz_console/scripting.hpp"
#include "rydz_scripting/lua_resource.hpp"
#include "rydz_ecs/mod.hpp"
#include <functional>
#include <string>
#include <unordered_map>

extern "C" {
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"
}

namespace console {

using LuaCommand = std::function<std::string(lua_State *)>;
inline std::unordered_map<std::string, LuaCommand> lua_commands;

inline int lua_command_gateway(lua_State *L) {
  const char *cmd_name = lua_tostring(L, lua_upvalueindex(1));
  auto it = lua_commands.find(cmd_name);
  if (it == lua_commands.end()) {
    lua_pushstring(L, ("Nieznana komenda: " + std::string(cmd_name)).c_str());
    return lua_error(L);
  }
  std::string err = it->second(L);
  if (!err.empty()) {
    lua_pushstring(L, err.c_str());
    return lua_error(L);
  }
  return 0;
}

inline void register_lua_command(lua_State *L, const std::string &name,
                                 LuaCommand lcmd) {

  lua_commands[name] = std::move(lcmd);

  lua_pushstring(L, name.c_str());
  lua_pushcclosure(L, lua_command_gateway, 1);
  lua_setglobal(L, name.c_str());
}

template <typename T> T lua_get(lua_State *L, int idx);

template <> inline int lua_get<int>(lua_State *L, int idx) {
  if (!lua_isnumber(L, idx))
    throw std::runtime_error("Oczekiwano liczby całkowitej");
  return (int)lua_tointeger(L, idx);
}

template <> inline float lua_get<float>(lua_State *L, int idx) {
  if (!lua_isnumber(L, idx))
    throw std::runtime_error("Oczekiwano liczby zmiennoprzecinkowej");
  return (float)lua_tonumber(L, idx);
}

template <> inline std::string lua_get<std::string>(lua_State *L, int idx) {
  if (!lua_isstring(L, idx))
    throw std::runtime_error("Oczekiwano string");
  return std::string(lua_tostring(L, idx));
}

template <> inline bool lua_get<bool>(lua_State *L, int idx) {
  if (!lua_isboolean(L, idx))
    throw std::runtime_error("Oczekiwano boolean");
  return lua_toboolean(L, idx) != 0;
}

class ConsoleAPI {
public:
  template <typename Func>
  static void bind_system(ecs::World &world, const std::string &command_name,
                          Func ecs_system) {

    auto *lua = world.get_resource<scripting::LuaResource>();
    auto *console = world.get_resource<ConsoleState>();
    if (!lua)
      return;

    ecs::World *w_ptr = &world;

    register_lua_command(
        lua->L, command_name,
        [w_ptr, console, command_name, ecs_system](lua_State *) -> std::string {
          auto sys = ecs::make_system(ecs_system);
          sys->run(*w_ptr);
          if (console)
            console->log("[API] Wykonano: " + command_name);
          return "";
        });
  }
};

template <typename... LuaArgs> struct BindCommand {
  template <typename BuilderFunc>
  static void to(ecs::World &world, const std::string &command_name,
                 BuilderFunc builder) {

    auto *lua = world.get_resource<scripting::LuaResource>();
    auto *console = world.get_resource<ConsoleState>();
    if (!lua)
      return;

    ecs::World *w_ptr = &world;

    register_lua_command(
        lua->L, command_name,
        [w_ptr, console, command_name, builder](lua_State *L) -> std::string {
          int arg_count = lua_gettop(L);
          int expected_count = (int)sizeof...(LuaArgs);
          if (arg_count < expected_count) {

            return "Oczekiwano " + std::to_string(expected_count) +
                   " argumentów, otrzymano " + std::to_string(arg_count);
          }

          try {

            int idx = 1;
            auto sys = ecs::make_system(builder(lua_get<LuaArgs>(L, idx++)...));
            sys->run(*w_ptr);
            if (console)
              console->log("[API] Wykonano: " + command_name);
            return "";
          } catch (const std::exception &e) {

            return std::string("Błąd podczas wykonywania komendy: ") + e.what();
          }
        });
  }
};

} // namespace engine
