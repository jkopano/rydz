#pragma once

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

#include <rydz_ecs/mod.hpp>
#include "bind_entity.hpp"
#include "../component_registry.hpp"
#include "rydz_math/mod.hpp"

namespace scripting {
    using Transform = rydz_math::Transform;
    using Vec3 = rydz_math::Vec3;

	static constexpr const char* TRANSFORM_MT = "rydz.Transform";

	//Proxy - brak cachowania bo storage moze sie przeniesc, trzymamy world* + entity i pytamy ecs przy kazdym dostepie
	struct TransformProxy {
		ecs::World* world;
		ecs::Entity entity;
	};

	inline Transform* get_transform_safe(lua_State* L, TransformProxy* proxy) {
		auto* t = proxy->world->get_component<Transform>(proxy->entity);
		if (!t) luaL_error(L, "Entity has no Transform component");
		return t;
	}

	inline int lua_transform_index(lua_State* L) {
		auto* proxy = (TransformProxy*)luaL_checkudata(L, 1, TRANSFORM_MT);
		const char* key = luaL_checkstring(L, 2);
		auto* t = get_transform_safe(L, proxy);

        if (strcmp(key, "translation") == 0) {
            lua_newtable(L);
            lua_pushnumber(L, t->translation.GetX()); lua_setfield(L, -2, "x");
            lua_pushnumber(L, t->translation.GetY()); lua_setfield(L, -2, "y");
            lua_pushnumber(L, t->translation.GetZ()); lua_setfield(L, -2, "z");
            return 1;
        }
        if (strcmp(key, "scale") == 0) {
            lua_newtable(L);
            lua_pushnumber(L, t->scale.GetX()); lua_setfield(L, -2, "x");
            lua_pushnumber(L, t->scale.GetY()); lua_setfield(L, -2, "y");
            lua_pushnumber(L, t->scale.GetZ()); lua_setfield(L, -2, "z");
            return 1;
        }
        if (strcmp(key, "rotation") == 0) {
            lua_newtable(L);
            lua_pushnumber(L, t->rotation.GetX()); lua_setfield(L, -2, "x");
            lua_pushnumber(L, t->rotation.GetY()); lua_setfield(L, -2, "y");
            lua_pushnumber(L, t->rotation.GetZ()); lua_setfield(L, -2, "z");
            lua_pushnumber(L, t->rotation.GetW()); lua_setfield(L, -2, "w");
            return 1;
        }
        lua_pushnil(L);
        return 1;
	}

    inline int lua_transform_newindex(lua_State* L) {
        auto* proxy = (TransformProxy*)luaL_checkudata(L, 1, TRANSFORM_MT);
        const char* key = luaL_checkstring(L, 2);
        luaL_checktype(L, 3, LUA_TTABLE);
        auto* t = get_transform_safe(L, proxy);

        auto read_field = [&](const char* f) -> float {
            lua_getfield(L, 3, f);
            float v = (float)luaL_optnumber(L, -1, 0.0);
            lua_pop(L, 1);
            return v;
            };

        if (strcmp(key, "translation") == 0) {
            t->translation = Vec3(read_field("x"), read_field("y"), read_field("z"));
            return 0;
        }
        if (strcmp(key, "scale") == 0) {
            t->scale = Vec3(read_field("x"), read_field("y"), read_field("z"));
            return 0;
        }
        if (strcmp(key, "rotation") == 0) {
            t->rotation = rydz_math::Quat(read_field("x"), read_field("y"), read_field("z"), read_field("w"));
            return 0;
        }
        return luaL_error(L, "Transform has no field '%s'", key);
    }

    inline int push_transform_proxy(lua_State* L, ecs::World* world, ecs::Entity e) {
        auto* proxy = (TransformProxy*)lua_newuserdata(L, sizeof(TransformProxy));
        proxy->world = world;
        proxy->entity = e;
        luaL_getmetatable(L, TRANSFORM_MT);
        lua_setmetatable(L, -2);
        return 1;
    }

    inline void register_transform_metatable(lua_State* L) {
        luaL_newmetatable(L, TRANSFORM_MT);

        lua_pushcfunction(L, lua_transform_index);
        lua_setfield(L, -2, "__index");

        lua_pushcfunction(L, lua_transform_newindex);
        lua_setfield(L, -2, "__newindex");

        lua_pushcfunction(L, [](lua_State* L) -> int {
            lua_pushstring(L, "Transform");
            return 1;
            });
        lua_setfield(L, -2, "__tostring");

        lua_pop(L, 1);
    }

    inline int register_transform_in_registry() {
        return ComponentRegistry::get().register_component<Transform>("Transform",
            [](lua_State* L, ecs::World* world, ecs::Entity e) -> int {
                return push_transform_proxy(L, world, e);
            }
        );
    }
}

