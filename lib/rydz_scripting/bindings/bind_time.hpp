//Globalna tabela time aktualizowana kazd¹ klatk¹, time istnieje juz jako zasob ecs wiêc wystarczy go odpytac i przepisac wartosci do lua przed wywolaniem systemow lua

#pragma once

extern "C" {
#include "lua.h"
}

#include "rydz_ecs/mod.hpp"

namespace scripting {
	//Aktualizacja Time.delta przed kazdym update
	inline void update_time_table(lua_State* L, ecs::World& world) {
		auto* time = world.get_resource<ecs::Time>();
		if (!time) return;

		lua_getglobal(L, "Time");
		if (lua_isnil(L, -1)) {
			lua_pop(L, 1);
			lua_newtable(L);
			lua_setglobal(L, "Time");
			lua_getglobal(L, "Time");
		}

		// delta_seconds — tak samo jak w reszcie silnika
		lua_pushnumber(L, time->delta_seconds);
		lua_setfield(L, -2, "delta");

		lua_pop(L, 1);
	}

	// Inicjalizacja
	inline void register_time_table(lua_State* L) {
		lua_newtable(L);
		lua_pushnumber(L, 0.0f); lua_setfield(L, -2, "delta");
		lua_setglobal(L, "Time");
	}
}