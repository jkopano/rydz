//Wskaznik na world jako userdata - lua zarzadza zmienna userdata ale nie korzysta z niej bo nie potrafi
//Metatabela  - tabella lua ktora dziala jak instrukcja obslugi, gdy lua probuje wywolac metode na obiekcie userdata patrzy w jego tabele na pole index aby znalezc odpowiednia metode
#pragma once

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

#include <rydz_ecs/world.hpp>
#include "bind_entity.hpp"

namespace scripting {

	//Klucz metatabeli w registry
	static constexpr const char* WORLD_MT = "rydz.World";

	//FullUserdata - wskaznik na world
	struct WorldUserdata {
		ecs::World* world;
	};

	//Alokacja pamieci dla userdata, wrzucenie na stos oraz przypisanie do userdata jego metatabeli
	inline void push_world(lua_State* L, ecs::World* world) {
		auto* ud = (WorldUserdata*)lua_newuserdata(L, sizeof(WorldUserdata));
		ud->world = world;
		luaL_getmetatable(L, WORLD_MT);
		lua_setmetatable(L, -2);
	}

	//Pobranie world* ze stosu z weryfikacj¹ metatabeli
	inline ecs::World* check_world(lua_State* L, int idx) {
		auto* ud = (WorldUserdata*)luaL_checkudata(L, idx, WORLD_MT);
		return ud->world;
	}

	//-- METODY --

	//world:spawn() -> entity
	inline int lua_world_spawn(lua_State* L) {
		ecs::World* world = check_world(L, 1);
		ecs::Entity e = world->spawn();
		push_entity(L, e);
		return 1;
	}

	//world:despawn(entity)
	inline int lua_world_despawn(lua_State* L) {
		ecs::World* world = check_world(L, 1);
		ecs::Entity e = check_entity(L, 2);
		world->despawn(e);
		return 0;
	}

	// world:is_alive(entity) -> bool
	inline int lua_world_is_alive(lua_State* L) {
		ecs::World* world = check_world(L, 1);
		ecs::Entity e = check_entity(L, 2);
		lua_pushboolean(L, world->entities.is_alive(e) ? 1 : 0);
		return 1;
	}

	//Rejestracja metatabeli
	inline void register_world_metatable(lua_State* L) {
		luaL_newmetatable(L, WORLD_MT);

		lua_newtable(L);

		lua_pushcfunction(L, lua_world_spawn);
		lua_setfield(L, -2, "spawn");

		lua_pushcfunction(L, lua_world_despawn);
		lua_setfield(L, -2, "despawn");

		lua_pushcfunction(L, lua_world_is_alive);
		lua_setfield(L, -2, "is_alive");

		//W przyszlosci kolejne ...

		lua_setfield(L, -2, "__index");

		lua_pushcfunction(L, [](lua_State* L) -> int {
			lua_pushstring(L, "World");
			return 1;
		});
		lua_setfield(L, -2, "__tostring");

		lua_pop(L, 1);
	}

	//eksponuje world jako globalna zmienn¹ _world w Lua
	inline void expose_world_global(lua_State* L, ecs::World* world) {
		push_world(L, world);
		lua_setglobal(L, "_world");
	}

}