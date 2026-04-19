//Definiuje funkcjê rejestruj¹c¹ gobalne API silnika w VM Lua oraz strukturê LuaResource ktora jest zasobem ECS opakowuj¹cym VM Lua

#pragma once

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

#include <rydz_ecs/mod.hpp>
#include <rydz_scripting/bindings/bind_world.hpp>
#include <string>
#include <stdexcept>
#include <filesystem>

namespace scripting {

	//Rejestracja metatabeli w globalnym rejestrze Lua, tworzenie globalnej tabeli Rydz oraz globalnej tablicy z polami etykiet harmonogramów
	inline void register_rydz_api(lua_State* L) {
		register_world_metatable(L);

		lua_newtable(L);
		lua_setglobal(L, "Rydz");

		lua_newtable(L);
		lua_pushstring(L, "Startup"); lua_setfield(L, -2, "Startup");
		lua_pushstring(L, "Update");  lua_setfield(L, -2, "Update");
		lua_setglobal(L, "Schedule");
	}

	//Zasób przechowuj¹cy maszynê wirtualn¹ Lua
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

		LuaResource() = default;

		//init - stworzenie vm, otwarcie bibliotek i rejestracja api silnika
		void init() {
			L = luaL_newstate();
			luaL_openlibs(L);
			register_rydz_api(L);
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
}