//Wskaznik na world jako userdata - lua zarzadza zmienna userdata ale nie korzysta z niej bo nie potrafi
//Metatabela  - tabella lua ktora dziala jak instrukcja obslugi, gdy lua probuje wywolac metode na obiekcie userdata patrzy w jego tabele na pole index aby znalezc odpowiednia metode
#pragma once

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

#include <rydz_ecs/mod.hpp>
#include "bind_entity.hpp"
#include "rydz_scripting/lua_component.hpp"
#include "rydz_scripting/lua_resource.hpp"
#include "rydz_scripting/component_registry.hpp"

namespace scripting {

	template<>
	struct LuaComponentTraits<scripting::LuaComponent> {
		static void bind_methods(lua_State* L) {}
		static void init_from_lua(lua_State* L, ecs::World* w, ecs::Entity e, int val_idx) {
			if (lua_istable(L, val_idx)) {
				lua_pushvalue(L, val_idx);
				int ref = luaL_ref(L, LUA_REGISTRYINDEX);
				w->insert_component(e, scripting::LuaComponent{ ref });
			}
		}
	};

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

	//Pobranie world* ze stosu z weryfikacją metatabeli
	inline ecs::World* check_world(lua_State* L, int idx) {
		auto* ud = (WorldUserdata*)luaL_checkudata(L, idx, WORLD_MT);
		return ud->world;
	}

	//-- METODY --

	//world:spawn() -> entity
	inline int lua_world_spawn(lua_State* L) {
		ecs::World* world = check_world(L, 1);
		ecs::Entity e = world->spawn();

		if (lua_gettop(L) >= 2 && lua_istable(L, 2)) {
			lua_pushnil(L);
			while (lua_next(L, 2) != 0) {
				if (lua_istable(L, -1)) {
					lua_rawgeti(L, -1, 1);
					if (lua_isuserdata(L, -1)) {
						auto* ud = (ComponentTypeUD*)lua_touserdata(L, -1);
						const auto* fns = ComponentRegistry::get().lookup(ud->handle);
						if (fns) {
							if (fns->insert) fns->insert(world, e);
							lua_rawgeti(L, -2, 2);
							int data_idx = lua_gettop(L);
							if (!lua_isnil(L, data_idx) && fns->init_from_lua) {
								fns->init_from_lua(L, world, e, data_idx);
							}
							lua_pop(L, 1); // pop data
						}
					}
					lua_pop(L, 1); // pop userdata
				}
				lua_pop(L, 1); // pop row
			}
		}

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

		//world:get_lua_component(entity)
		lua_pushcfunction(L, [](lua_State* L) -> int {
			ecs::World* world = check_world(L, 1);
			ecs::Entity e = check_entity(L, 2);

			auto* comp = world->get_component<scripting::LuaComponent>(e);
			if (!comp) {
				lua_pushnil(L);
				return 1;
			}
			comp->push(L);
			return 1;
		});
		lua_setfield(L, -2, "get_lua_component");

		//world:set_lua_component(entity, table)
		lua_pushcfunction(L, [](lua_State* L) -> int {
			ecs::World* world = check_world(L, 1);
			ecs::Entity e = check_entity(L, 2);
			luaL_checktype(L, 3, LUA_TTABLE);

			lua_pushvalue(L, 3);
			int new_ref = luaL_ref(L, LUA_REGISTRYINDEX);

			auto* comp = world->get_component<scripting::LuaComponent>(e);
			if (comp) {
				comp->release(L);
				comp->table_ref = new_ref;
			}
			else {
				world->insert_component(e, scripting::LuaComponent{ new_ref });
			}
			return 0;
			});
		lua_setfield(L, -2, "set_lua_component");

		// world:each_lua()
		lua_pushcfunction(L, [](lua_State* L) -> int {
			ecs::World* world = check_world(L, 1);

			auto* storage = world->get_storage<scripting::LuaComponent>();

			lua_newtable(L); 
			if (storage) {
				int i = 1;
				for (const ecs::Entity& e : storage->entities()) {
					push_entity(L, e);
					lua_rawseti(L, -2, i++);
				}
			}
			lua_pushinteger(L, 0); 

			lua_pushcclosure(L, [](lua_State* L) -> int {
				lua_Integer idx = lua_tointeger(L, lua_upvalueindex(2)) + 1;
				lua_rawgeti(L, lua_upvalueindex(1), (int)idx);
				if (lua_isnil(L, -1)) {
					return 1; 
				}
				lua_pushinteger(L, idx);
				lua_replace(L, lua_upvalueindex(2)); 
				return 1;
				}, 2);

			return 1;
			});
		lua_setfield(L, -2, "each_lua");

		// world:each_lua_with("is_player", true) -> iterator
		// Zwraca tylko encje których LuaComponent.key == value
		lua_pushcfunction(L, [](lua_State* L) -> int {
			ecs::World* world = check_world(L, 1);
			const char* key = luaL_checkstring(L, 2);

			auto* storage = world->get_storage<scripting::LuaComponent>();

			lua_newtable(L); // tabela encji wynikowych [pozycja 4 na stosie]
			int count = 1;

			if (storage) {
				for (const ecs::Entity& e : storage->entities()) {
					auto* comp = world->get_component<scripting::LuaComponent>(e);
					if (!comp || comp->table_ref == LUA_NOREF) continue;

					//pobranie pola `key` z tabeli LuaComponent
					lua_rawgeti(L, LUA_REGISTRYINDEX, comp->table_ref); 
					lua_getfield(L, -1, key);                          

					bool match = false;
					if (lua_type(L, -1) == lua_type(L, 3)) {
						match = (lua_equal(L, -1, 3) != 0);
					}

					lua_pop(L, 2);

					if (match) {
						push_entity(L, e);
						lua_rawseti(L, -2, count++); // tabela wynikowa[-1] = entity
					}
				}
			}

			//tabela wynikowa jako upvalue [1]
			lua_pushinteger(L, 0); // indeks jako upvalue [2]

			lua_pushcclosure(L, [](lua_State* L) -> int {
				lua_Integer idx = lua_tointeger(L, lua_upvalueindex(2)) + 1;
				lua_rawgeti(L, lua_upvalueindex(1), (int)idx);
				if (lua_isnil(L, -1)) {
					return 1;
				}
				lua_pushinteger(L, idx);
				lua_replace(L, lua_upvalueindex(2));
				return 1;
				}, 2);

			return 1;
			});
		lua_setfield(L, -2, "each_lua_with");

		// world:each(Components.Transform, ...) -> iterator
		lua_pushcfunction(L, [](lua_State* L) -> int {
			ecs::World* world = check_world(L, 1);
			
			std::vector<int> handles;
			for (int i = 2; i <= lua_gettop(L); i++) {
				if (lua_isuserdata(L, i)) {
					auto* ud = (ComponentTypeUD*)lua_touserdata(L, i);
					handles.push_back(ud->handle);
				}
			}

			if (handles.empty()) {
				lua_pushcclosure(L, [](lua_State* L) { return 0; }, 0);
				return 1;
			}

			std::vector<ecs::Entity> best_entities;
			const auto* fns_first = ComponentRegistry::get().lookup(handles[0]);
			if (fns_first && fns_first->get_entities) {
				best_entities = fns_first->get_entities(world);
			}

			std::vector<ecs::Entity> final_entities;
			for (auto e : best_entities) {
				bool has_all = true;
				for (size_t i = 1; i < handles.size(); i++) {
					const auto* fns = ComponentRegistry::get().lookup(handles[i]);
					if (!fns || !fns->has || !fns->has(world, e)) {
						has_all = false;
						break;
					}
				}
				if (has_all) final_entities.push_back(e);
			}

			lua_newtable(L);
			for (size_t i = 0; i < final_entities.size(); i++) {
				push_entity(L, final_entities[i]);
				lua_rawseti(L, -2, (int)i + 1);
			}

			lua_newtable(L);
			for (size_t i = 0; i < handles.size(); i++) {
				lua_pushinteger(L, handles[i]);
				lua_rawseti(L, -2, (int)i + 1);
			}

			lua_pushinteger(L, 0); 
			push_world(L, world);

			lua_pushcclosure(L, [](lua_State* L) -> int {
				int idx = (int)lua_tointeger(L, lua_upvalueindex(3)) + 1;
				lua_rawgeti(L, lua_upvalueindex(1), idx);
				if (lua_isnil(L, -1)) return 0;
				
				lua_pushinteger(L, idx);
				lua_replace(L, lua_upvalueindex(3));
				
				ecs::Entity e = check_entity(L, -1);
				ecs::World* world = check_world(L, lua_upvalueindex(4));

				int num_handles = (int)lua_objlen(L, lua_upvalueindex(2));
				int returns = 1;
				
				for (int i = 1; i <= num_handles; i++) {
					lua_rawgeti(L, lua_upvalueindex(2), i);
					int handle = (int)lua_tointeger(L, -1);
					lua_pop(L, 1);

					const auto* fns = ComponentRegistry::get().lookup(handle);
					if (fns && fns->push) {
						returns += fns->push(L, world, e);
					} else {
						lua_pushnil(L);
						returns++;
					}
				}
				return returns;
			}, 4);

			return 1;
		});
		lua_setfield(L, -2, "each");

		//world:get_component(entity, Components.Transform) -> proxy lub nil
		lua_pushcfunction(L, [](lua_State* L) -> int {
			ecs::World* world = check_world(L, 1);
			ecs::Entity e = check_entity(L, 2);
			if (!lua_isuserdata(L, 3)) return luaL_error(L, "Expected ComponentTypeUD userdata");
			auto* ud = (ComponentTypeUD*)lua_touserdata(L, 3);

			const auto* fns = ComponentRegistry::get().lookup(ud->handle);
			if (!fns) {
				return luaL_error(L, "Invalid component handle: %d", ud->handle);
			}
			if (!fns->has || !fns->has(world, e)) {
				lua_pushnil(L);
				return 1;
			}
			return fns->push ? fns->push(L, world, e) : 0;
			});
		lua_setfield(L, -2, "get_component");

		//world:insert_component(entity, Components.Transform) -> boolean (true if inserted, false otherwise)
		lua_pushcfunction(L, [](lua_State* L) -> int {
			ecs::World* world = check_world(L, 1);
			ecs::Entity e = check_entity(L, 2);
			if (!lua_isuserdata(L, 3)) return luaL_error(L, "Expected ComponentTypeUD userdata");
			auto* ud = (ComponentTypeUD*)lua_touserdata(L, 3);

			const auto* fns = ComponentRegistry::get().lookup(ud->handle);
			if (!fns) {
				return luaL_error(L, "Invalid component handle: %d", ud->handle);
			}

			if (fns->insert && fns->insert(world, e)) {
				lua_pushboolean(L, 1);
			} else {
				lua_pushboolean(L, 0);
			}
			return 1;
			});
		lua_setfield(L, -2, "insert_component");

		// world:add_transform(entity, x, y, z)
		lua_pushcfunction(L, [](lua_State* L) -> int {
			ecs::World* world = check_world(L, 1);
			ecs::Entity e = check_entity(L, 2);
			float x = (float)luaL_optnumber(L, 3, 0.0);
			float y = (float)luaL_optnumber(L, 4, 0.0);
			float z = (float)luaL_optnumber(L, 5, 0.0);
			world->insert_component(e, rydz_math::Transform::from_xyz(x, y, z));
			return 0;
			});
		lua_setfield(L, -2, "add_transform");

		lua_setfield(L, -2, "__index");

		lua_pushcfunction(L, [](lua_State* L) -> int {
			lua_pushstring(L, "World");
			return 1;
		});
		lua_setfield(L, -2, "__tostring");

		lua_pop(L, 1);
	}

	//eksponuje world jako globalna zmienną _world w Lua
	inline void expose_world_global(lua_State* L, ecs::World* world) {
		push_world(L, world);
		lua_setglobal(L, "_world");
	}

}
