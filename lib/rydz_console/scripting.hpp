//Odpowiada za stworzenie wirtualnej maszyny Lua i udostępnienie jej dla innych systemów
#pragma once

#include "rydz_ecs/rydz_ecs.hpp"
#include <sol/sol.hpp>
#include <iostream>

namespace engine {

	struct LuaResource {
		using T = ecs::Resource;
		sol::state vm;
	};

	inline void scripting_plugin(ecs::App& app)
	{
		if (!app.world().has_resource<LuaResource>())
		{
			LuaResource lua_res;
			lua_res.vm.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table);
			app.insert_resource(std::move(lua_res));
		}
	}

}