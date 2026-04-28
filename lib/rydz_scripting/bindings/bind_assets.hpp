#pragma once

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

#include <rydz_ecs/mod.hpp>
#include <rydz_graphics/mod.hpp>
#include <rydz_graphics/material/standard_material.hpp>
#include "bind_world.hpp"
#include "../auto_binder.hpp"
#include <string>

namespace scripting {

    template<typename T>
    inline void push_handle(lua_State* L, ecs::Handle<T> handle, const char* mt_name) {
        auto* ud = (ecs::Handle<T>*)lua_newuserdata(L, sizeof(ecs::Handle<T>));
        *ud = handle;
        luaL_getmetatable(L, mt_name);
        lua_setmetatable(L, -2);
    }

    template<typename T>
    inline ecs::Handle<T> check_handle(lua_State* L, int idx, const char* mt_name) {
        auto* ud = (ecs::Handle<T>*)luaL_checkudata(L, idx, mt_name);
        return *ud;
    }

    template<> struct LuaRet<ecs::Handle<gl::Mesh>> {
        static int push(lua_State* L, const ecs::Handle<gl::Mesh>& val) {
            push_handle(L, val, "rydz.HandleMesh"); return 1;
        }
    };
    template<> struct LuaRet<ecs::Handle<ecs::Material>> {
        static int push(lua_State* L, const ecs::Handle<ecs::Material>& val) {
            push_handle(L, val, "rydz.HandleMaterial"); return 1;
        }
    };
    template<> struct LuaRet<ecs::Handle<gl::Texture>> {
        static int push(lua_State* L, const ecs::Handle<gl::Texture>& val) {
            push_handle(L, val, "rydz.HandleTexture"); return 1;
        }
    };

    template<> struct LuaArg<ecs::Handle<gl::Texture>> {
        static ecs::Handle<gl::Texture> get(lua_State* L, int idx) {
            return check_handle<gl::Texture>(L, idx, "rydz.HandleTexture");
        }
    };
    template<> struct LuaArg<ecs::Handle<gl::Mesh>> {
        static ecs::Handle<gl::Mesh> get(lua_State* L, int idx) {
            return check_handle<gl::Mesh>(L, idx, "rydz.HandleMesh");
        }
    };
    template<> struct LuaArg<ecs::Handle<ecs::Material>> {
        static ecs::Handle<ecs::Material> get(lua_State* L, int idx) {
            return check_handle<ecs::Material>(L, idx, "rydz.HandleMaterial");
        }
    };

    // --- FACTORIES ---
    inline ecs::Handle<gl::Texture> load_texture(ecs::World* world, std::string path) {
        return world->get_resource<ecs::Assets<gl::Texture>>()->add(gl::load_texture(path.c_str()));
    }
    inline ecs::Handle<gl::Mesh> make_mesh_cube(ecs::World* world, float w, float h, float l) {
        return world->get_resource<ecs::Assets<gl::Mesh>>()->add(ecs::mesh::cube(w, h, l));
    }
    inline ecs::Handle<ecs::Material> make_material(ecs::World* world, ecs::Handle<gl::Texture> tex) {
        return world->get_resource<ecs::Assets<ecs::Material>>()->add(ecs::StandardMaterial::from_texture(tex));
    }

    // --- TRAITS ---
    template<>
    struct LuaComponentTraits<ecs::Mesh3d> {
        static void bind_methods(lua_State* L) {
            scripting::bind_function<make_mesh_cube>(L, "cube");
        }
        static void init_from_lua(lua_State* L, ecs::World* w, ecs::Entity e, int val_idx) {
            if (lua_isuserdata(L, val_idx)) {
                w->insert_component(e, ecs::Mesh3d{ check_handle<gl::Mesh>(L, val_idx, "rydz.HandleMesh") });
            }
        }
    };

    template<>
    struct LuaComponentTraits<ecs::MeshMaterial3d> {
        static void bind_methods(lua_State* L) {
            scripting::bind_function<make_material>(L, "StandardMaterial");
        }
        static void init_from_lua(lua_State* L, ecs::World* w, ecs::Entity e, int val_idx) {
            if (lua_isuserdata(L, val_idx)) {
                w->insert_component(e, ecs::MeshMaterial3d{ check_handle<ecs::Material>(L, val_idx, "rydz.HandleMaterial") });
            }
        }
    };

    inline void register_asset_bindings(lua_State* L) {
        luaL_newmetatable(L, "rydz.HandleTexture"); lua_pop(L, 1);
        luaL_newmetatable(L, "rydz.HandleMesh"); lua_pop(L, 1);
        luaL_newmetatable(L, "rydz.HandleMaterial"); lua_pop(L, 1);

        lua_pushvalue(L, LUA_GLOBALSINDEX);
        scripting::bind_function<load_texture>(L, "asset_loader");
        lua_pop(L, 1);
    }

    inline void register_assets_in_registry() {
        static bool registered = false;
        if (registered) return;
        registered = true;

        ComponentRegistry::get().register_component<ecs::Mesh3d>("Mesh3d");
        ComponentRegistry::get().register_component<ecs::MeshMaterial3d>("MeshMaterial3d");
    }
}
