//Definiuje funkcję rejestrującą gobalne API silnika w VM Lua oraz strukturę LuaResource ktora jest zasobem ECS opakowującym VM Lua

#pragma once

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

#include <rydz_ecs/mod.hpp>
#include <rydz_scripting/bindings/bind_world.hpp>
#include "rydz_scripting/bindings/bind_transform.hpp"
#include "rydz_scripting/bindings/bind_input.hpp"
#include "rydz_scripting/bindings/bind_time.hpp"
#include "rydz_scripting/bindings/bind_assets.hpp"
#include "rydz_scripting/component_registry.hpp"
#include "rydz_scripting/lua_system_registry.hpp"
#include <string>
#include <stdexcept>
#include <filesystem>

namespace scripting {

	//Rejestracja metatabeli w globalnym rejestrze Lua, tworzenie globalnej tabeli Rydz oraz globalnej tablicy z polami etykiet harmonogramów
	inline void register_rydz_api(lua_State* L) {
		//Metatabele
		register_world_metatable(L);
		register_transform_metatable(L);

		//Rejestracja komponentów
		register_transform_in_registry();

		register_input_table(L);
		register_time_table(L);
		register_asset_bindings(L);
		register_assets_in_registry();

		//globalna tabela Components
		lua_newtable(L);
		scripting::ComponentRegistry::get().fill_lua_table(L);
		lua_setglobal(L, "Components");

		//globalna tabela Rydz
		lua_newtable(L);

		// Rydz.register_system(schedule_name, function [, name])
		lua_pushcfunction(L, [](lua_State* L) -> int {
			const char* schedule = luaL_checkstring(L, 1);
			luaL_checktype(L, 2, LUA_TFUNCTION);
			
			std::string sys_name;
			if (lua_gettop(L) >= 3 && lua_type(L, 3) == LUA_TSTRING) {
				sys_name = lua_tostring(L, 3);
			} else {
				lua_Debug ar;
				if (lua_getstack(L, 1, &ar) && lua_getinfo(L, "Sl", &ar)) {
					sys_name = std::string(ar.short_src) + ":" + std::to_string(ar.currentline);
				} else {
					static int anon_counter = 0;
					sys_name = std::string(schedule) + "_anon_" + std::to_string(++anon_counter);
				}
			}

			lua_pushvalue(L, 2);
			int func_ref = luaL_ref(L, LUA_REGISTRYINDEX);

			// Pobierz wskaźnik do registry — ustawiany przez startup system w scene_new.hpp
			lua_getfield(L, LUA_REGISTRYINDEX, "_sys_registry");
			auto* reg = (scripting::LuaSystemRegistry*)lua_touserdata(L, -1);
			lua_pop(L, 1);

			if (reg) {
				reg->register_system(schedule, sys_name, func_ref);
			}
			else {
				fprintf(stderr, "[Lua] register_system: wywolaj po ustawieniu _sys_registry!\n");
			}
			return 0;
			});
		lua_setfield(L, -2, "register_system");

		// Rydz.on_startup(function) — skrót
		lua_pushcfunction(L, [](lua_State* L) -> int {
			luaL_checktype(L, 1, LUA_TFUNCTION);
			
			std::string sys_name;
			lua_Debug ar;
			if (lua_getstack(L, 1, &ar) && lua_getinfo(L, "Sl", &ar)) {
				sys_name = std::string(ar.short_src) + ":" + std::to_string(ar.currentline);
			} else {
				static int startup_counter = 0;
				sys_name = "startup_anon_" + std::to_string(++startup_counter);
			}

			lua_pushvalue(L, 1);
			int func_ref = luaL_ref(L, LUA_REGISTRYINDEX);

			lua_getfield(L, LUA_REGISTRYINDEX, "_sys_registry");
			auto* reg = (scripting::LuaSystemRegistry*)lua_touserdata(L, -1);
			lua_pop(L, 1);

			if (reg) reg->register_system("Startup", sys_name, func_ref);
			return 0;
			});
		lua_setfield(L, -2, "on_startup");

		lua_setglobal(L, "Rydz");

		//Stałe schedule
		lua_newtable(L);
		lua_pushstring(L, "Startup"); lua_setfield(L, -2, "Startup");
		lua_pushstring(L, "Update");  lua_setfield(L, -2, "Update");
		lua_setglobal(L, "Schedule");
	}

	//Zasób przechowujący maszynę wirtualną Lua
	struct LuaResource {

		using T = ecs::Resource;
		lua_State* L = nullptr;

		//zakaz kopiowania LuaResource
		LuaResource(const LuaResource&) = delete;
		LuaResource& operator=(const LuaResource&) = delete;

		//Przenoszenie LuaResource
		LuaResource(LuaResource&& other) noexcept : L(other.L) { 
			other.L = nullptr; 
		}

		LuaResource& operator=(LuaResource&& other) noexcept {
			if (this != &other) {
				if (L) lua_close(L);
				L = other.L;
				other.L = nullptr;
			}
			return *this;
		}

		//stworzenie vm, otwarcie bibliotek
		LuaResource() {
			L = luaL_newstate();
			if (!L) fprintf(stderr, "[Scripting] Nie mozna utworzyc Lua VM\n");
			luaL_openlibs(L);
		}

		//Wczytanie i kompilacja pliku skryptowego lua, w przypadku bledu jego obsluga, jesli ok wykonanie poprzez pcall
		void load_file(const std::string& path) {
			if (luaL_dofile(L, path.c_str()) != LUA_OK) {
				std::string err = lua_tostring(L, -1);
				lua_pop(L, 1);
				throw std::runtime_error("[LUA] Error in " + path + ":\n" + err);
			}
		}

		//wczytanie directory i wykonanie load_file dla kazdego pliu .lua
		void load_scripts_dir(const std::string& dir) {
			if (!std::filesystem::exists(dir)) return;
			for (auto& entry : std::filesystem::directory_iterator(dir)) {
				if (entry.path().extension() == ".lua") {
					load_file(entry.path().string());
				}
			}
		}

		//Znalezienie funkcji w skrypcie Lua i stworzenie do niej referencji w c++ poprzez zapis w rejestrze z ID
		int get_function_ref(const std::string& name) {
			lua_getglobal(L, name.c_str());
			if (!lua_isfunction(L, -1)) {
				lua_pop(L, 1);
				return LUA_NOREF;
			}
			return luaL_ref(L, LUA_REGISTRYINDEX);
		}

		~LuaResource() {
			if (L) lua_close(L);
		}
	};

	//Plugin - odpowiednik console::scripting_plugin
	inline void scripting_plugin(ecs::App& app) {
		if (!app.world().has_resource<LuaResource>()) {
			app.insert_resource(LuaResource{});
		}
	}
}
