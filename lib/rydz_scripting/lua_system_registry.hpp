//Rejestr systemów Lua, wspiera hot-reload skryptów .lua
#pragma once

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

#include <rydz_ecs/mod.hpp>
#include <string>
#include <vector>
#include <unordered_map>

namespace scripting {

	struct LuaSystem {
		std::string name; //Nazwa do logowania bledow
		int func_ref; //LUA_REGISTRYINDEX ref do funkcji Lua
	};
	
	struct LuaSystemRegistry {
		using T = ecs::Resource;

		//Klucz: nazwa schedule
		std::unordered_map<std::string, std::vector<LuaSystem>> systems;

		void register_system(const std::string& schedule, const std::string& name, int func_ref) {
			auto& sys_list = systems[schedule];

			for (auto& sys : sys_list) {
				if (sys.name == name) {
					sys.func_ref = func_ref;
					return;
				}
			}

			sys_list.push_back(LuaSystem{ name, func_ref });
		}

		const std::vector<LuaSystem>* get_systems(const std::string& schedule) const {
			auto it = systems.find(schedule);
			return (it != systems.end()) ? &it->second : nullptr;
		}
	};

};

