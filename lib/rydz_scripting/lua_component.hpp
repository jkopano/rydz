#pragma once

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

#include <rydz_ecs/fwd.hpp>

namespace scripting {
	struct LuaComponent {
		using T = ecs::Component;

		int table_ref = LUA_NOREF;

		void push(lua_State* L) const {
			if (table_ref == LUA_NOREF) {
				lua_pushnil(L);
			} else {
				lua_rawgeti(L, LUA_REGISTRYINDEX, table_ref);
			}
		}

		void release(lua_State* L) {
			if (table_ref != LUA_NOREF) {
				luaL_unref(L, LUA_REGISTRYINDEX, table_ref);
				table_ref = LUA_NOREF;
			}
		}
	};
}