//Odpowiada za stworzenie wirtualnej maszyny Lua i udostępnienie jej dla innych systemów
#pragma once

#include "rydz_ecs/mod.hpp"
#include <iostream>

//wylaczenie name manglingu dla C
extern "C" {
	#include "lua.h"
	#include "lualib.h"
	#include "lauxlib.h"
}

namespace engine {

	struct LuaResource {
		using T = ecs::Resource;
		lua_State* vm = nullptr;

		LuaResource() {
			vm = luaL_newstate();
			if (!vm) std::cerr << "Nie można utworzyć wirtualnej maszyny Lua" << "\n";
			luaL_openlibs(vm);
		}

		~LuaResource() {
			if (vm) {
				lua_close(vm);
			}
			vm = nullptr;
		}

		LuaResource(const LuaResource&) = delete;
		LuaResource& operator=(const LuaResource&) = delete;

		LuaResource(LuaResource&& other) noexcept : vm(other.vm) {
			other.vm = nullptr;
		}

		LuaResource& operator=(LuaResource&& other) noexcept {
			if (this != &other) {
				if (vm) {
					lua_close(vm);
				}
				vm = other.vm;
				other.vm = nullptr;
			}
			return *this;
		}
	};

	inline void scripting_plugin(ecs::App& app)
	{
		if (!app.world().has_resource<LuaResource>())
		{
			app.insert_resource(LuaResource{});
		}
	}

}