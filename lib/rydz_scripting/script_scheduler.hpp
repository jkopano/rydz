#pragma once

#include "rydz_console/scripting.hpp"
#include "lua_system_registry.hpp"
#include "bindings/bind_world.hpp"
#include "bindings/bind_input.hpp"
#include "bindings/bind_time.hpp"

namespace scripting {

	inline void run_lua_systems(ecs::World& world, const std::string& schedule) {
		auto* lua_res = world.get_resource<engine::LuaResource>();
		auto* registry = world.get_resource<LuaSystemRegistry>();
		if (!lua_res || !registry) return;

		const auto* systems = registry->get_systems(schedule);
		if (!systems || systems->empty()) return;

		lua_State* L = lua_res->vm;

		for (const auto& sys : *systems) {
			lua_rawgeti(L, LUA_REGISTRYINDEX, sys.func_ref);
			push_world(L, &world);
			if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
				const char* err = lua_tostring(L, -1);
				fprintf(stderr, "[Lua] System '%s' error: %s\n", sys.name.c_str(), err ? err : "(unknown)");
				lua_pop(L, 1);
			}
		}
	}

	inline void lua_startup_runner(ecs::World& world) {
		run_lua_systems(world, "Startup");
	}

	inline void lua_update_runner(ecs::World& world) {
		auto* lua_res = world.get_resource<engine::LuaResource>();
		if (!lua_res) return;

		update_time_table(lua_res->vm, world);

		run_lua_systems(world, "Update");
	}
}