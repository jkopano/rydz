//Globalna tabela input, input istnieje juz jako zasob ecs wiêc wystarczy go odpytac i przepisac wartosci do lua przed wywolaniem systemow lua

#pragma once 

extern "C" {
#include "lua.h"
}

#include "rydz_ecs/mod.hpp"
#include "rydz_console/scripting.hpp"
#include "bind_world.hpp"
#include "rydz_ecs/core/input.hpp"
#include <external/raylib/src/raylib.h>

namespace scripting {

    //helper do walidacji metatabeli
    static ecs::World* get_world_from_global(lua_State* L) {
        lua_getglobal(L, "_world");
        if (lua_isnil(L, -1)) { lua_pop(L, 1); return nullptr; }
        auto* ud = (WorldUserdata*)luaL_testudata(L, -1, WORLD_MT);
        lua_pop(L, 1);
        return ud ? ud->world : nullptr;
    }

	inline void register_input_table(lua_State* L) {
		lua_newtable(L);

		//Metody — pobieraj¹ World* z upvalue ¿eby odpytaæ ecs::Input (narazie uproszczona wersja world globalny)

        // is_key_down(keycode) -> bool
        lua_pushcfunction(L, [](lua_State* L) -> int {
            ecs::KeyCode key = (ecs::KeyCode)luaL_checkinteger(L, 1);
            auto* world = get_world_from_global(L);
            if (!world) { lua_pushboolean(L, 0); return 1; }
            auto* input = world->get_resource<ecs::Input>();
            lua_pushboolean(L, input && input->key_down(key) ? 1 : 0);
            return 1;
            });
        lua_setfield(L, -2, "is_key_down");

        // is_key_pressed(keycode) -> bool (tylko w tej klatce)
        lua_pushcfunction(L, [](lua_State* L) -> int {
            ecs::KeyCode key = (ecs::KeyCode)luaL_checkinteger(L, 1);
            auto* world = get_world_from_global(L);
            if (!world) { lua_pushboolean(L, 0); return 1; }
            auto* input = world->get_resource<ecs::Input>();
            lua_pushboolean(L, input && input->key_pressed(key) ? 1 : 0);
            return 1;
            });
        lua_setfield(L, -2, "is_key_pressed");

        // is_key_released(keycode) -> bool
        lua_pushcfunction(L, [](lua_State* L) -> int {
            ecs::KeyCode key = (ecs::KeyCode)luaL_checkinteger(L, 1);
            auto* world = get_world_from_global(L);
            if (!world) { lua_pushboolean(L, 0); return 1; }
            auto* input = world->get_resource<ecs::Input>();
            lua_pushboolean(L, input && input->key_released(key) ? 1 : 0);
            return 1;
            });
        lua_setfield(L, -2, "is_key_released");


        //Stale klawiszy jako pola tabeli input
        lua_pushinteger(L, KEY_W);              lua_setfield(L, -2, "KEY_W");
        lua_pushinteger(L, KEY_S);              lua_setfield(L, -2, "KEY_S");
        lua_pushinteger(L, KEY_A);              lua_setfield(L, -2, "KEY_A");
        lua_pushinteger(L, KEY_D);              lua_setfield(L, -2, "KEY_D");
        lua_pushinteger(L, KEY_SPACE);          lua_setfield(L, -2, "KEY_SPACE");
        lua_pushinteger(L, KEY_ENTER);          lua_setfield(L, -2, "KEY_ENTER");
        lua_pushinteger(L, KEY_ESCAPE);         lua_setfield(L, -2, "KEY_ESCAPE");
        lua_pushinteger(L, KEY_UP);             lua_setfield(L, -2, "KEY_UP");
        lua_pushinteger(L, KEY_DOWN);           lua_setfield(L, -2, "KEY_DOWN");
        lua_pushinteger(L, KEY_LEFT);           lua_setfield(L, -2, "KEY_LEFT");
        lua_pushinteger(L, KEY_RIGHT);          lua_setfield(L, -2, "KEY_RIGHT");
        lua_pushinteger(L, KEY_LEFT_SHIFT);     lua_setfield(L, -2, "KEY_SHIFT");
        lua_pushinteger(L, KEY_LEFT_CONTROL);   lua_setfield(L, -2, "KEY_CTRL");

        lua_setglobal(L, "Input");
	}
}