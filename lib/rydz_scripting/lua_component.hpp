//Definiuje komponent ECS przechowuj¹cy referencjê do tabeli Lua w rejestrze globalnym VM. Pozwala to na przypisanie dowolnych danych Lua (tabeli) do encji ECS.
//Uzywany przez bind world i przechowywany w spasesetstorage w ramach world
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

		//Wrzuca powi¹zan¹ tabelê Lua na stos, jesli brak tabeli wrzuca nil
		void push(lua_State* L) const {
			if (table_ref == LUA_NOREF) {
				lua_pushnil(L);
			} else {
				lua_rawgeti(L, LUA_REGISTRYINDEX, table_ref);
			}
		}

		//Zwalnia referencjê w rejestrze Lua, pozwala GC odzyskaæ pamiêæ tabeli
		void release(lua_State* L) {
			if (table_ref != LUA_NOREF) {
				luaL_unref(L, LUA_REGISTRYINDEX, table_ref);
				table_ref = LUA_NOREF;
			}
		}
	};
}