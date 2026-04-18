//Entity {u32 index, u32 generation} -> lua_Integer (double w LuaJit)
#pragma once 

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

#include <rydz_ecs/entity.hpp>

namespace scripting {

	//Pakowanie do uint64: gen - gorne 32 bity, index dolne
	inline lua_Integer pack_entity(ecs::Entity e) {
		uint64_t packed = ((uint64_t)e.generation_val << 32) | (uint64_t)e.index_val;
		return (lua_Integer)packed;
	}

	inline ecs::Entity unpack_entity(lua_Integer val) {
		uint64_t packed = (uint64_t)val;
		ecs::Entity e{};
		e.index_val = (uint32_t)(packed & 0xFFFFFFFF);
		e.generation_val = (uint32_t)(packed >> 32);
		return e;
	}

	//sprawdza czy idx jest int, jesli tak to odpakowuje i zwraca entity w przeciwnym razie wyrzuci blad
	inline ecs::Entity check_entity(lua_State* L, int idx) {
		lua_Integer val = luaL_checkinteger(L, idx);
		return unpack_entity(val);
	}

	//push entity jako naszego spakowanego integera na stos
	inline void push_entity(lua_State* L, ecs::Entity e) {
		lua_pushinteger(L, pack_entity(e));
	}

}