#pragma once

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

#include <rydz_ecs/mod.hpp>
#include <rydz_graphics/mod.hpp>
#include <rydz_graphics/material/standard_material.hpp>
#include "bind_world.hpp"

namespace scripting {

    // Helper ulatwiajacy rejestracje Handles jako Userdata
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

    inline void register_asset_bindings(lua_State* L) {
        // Rejestracja dedykowanych metatabeli zeby nie mieszac typow Handles
        luaL_newmetatable(L, "rydz.HandleTexture"); lua_pop(L, 1);
        luaL_newmetatable(L, "rydz.HandleMesh"); lua_pop(L, 1);
        luaL_newmetatable(L, "rydz.HandleMaterial"); lua_pop(L, 1);

        // Rozszerzamy tablice __index metatabeli World (ustawiana w bind_world.hpp)
        luaL_getmetatable(L, WORLD_MT);
        lua_getfield(L, -1, "__index");  // push __index table
        
        // --- ASSETS ---

        // world:load_texture(path) -> HandleTexture
        lua_pushcfunction(L, [](lua_State* L) -> int {
            ecs::World* world = check_world(L, 1);
            const char* path = luaL_checkstring(L, 2);
            
            auto* textures = world->get_resource<ecs::Assets<gl::Texture>>();
            if(!textures) return luaL_error(L, "Brak zasobu Assets<Texture> w świecie ECS!");
            
            auto handle = textures->add(gl::Texture(path));
            push_handle<gl::Texture>(L, handle, "rydz.HandleTexture");
            return 1;
        });
        lua_setfield(L, -2, "load_texture");

        // world:add_mesh_cube(width, height, length) -> HandleMesh
        lua_pushcfunction(L, [](lua_State* L) -> int {
            ecs::World* world = check_world(L, 1);
            float w = (float)luaL_optnumber(L, 2, 1.0f);
            float h = (float)luaL_optnumber(L, 3, 1.0f);
            float l = (float)luaL_optnumber(L, 4, 1.0f);

            auto* meshes = world->get_resource<ecs::Assets<gl::Mesh>>();
            if(!meshes) return luaL_error(L, "Brak zasobu Assets<Mesh> w świecie ECS!");
            
            auto handle = meshes->add(gl::Mesh::cube(w, h, l));
            push_handle<gl::Mesh>(L, handle, "rydz.HandleMesh");
            return 1;
        });
        lua_setfield(L, -2, "add_mesh_cube");

        // world:add_material_from_texture(tex_handle) -> HandleMaterial
        lua_pushcfunction(L, [](lua_State* L) -> int {
            ecs::World* world = check_world(L, 1);
            auto tex_handle = check_handle<gl::Texture>(L, 2, "rydz.HandleTexture");
            
            auto* materials = world->get_resource<ecs::Assets<ecs::Material>>();
            if(!materials) return luaL_error(L, "Brak zasobu Assets<Material> w świecie ECS!");
            
            auto handle = materials->add(ecs::StandardMaterial::from_texture(tex_handle));
            push_handle<ecs::Material>(L, handle, "rydz.HandleMaterial");
            return 1;
        });
        lua_setfield(L, -2, "add_material_from_texture");

        // --- COMPONENTS ---
        
        // world:add_mesh3d(entity, mesh_handle)
        lua_pushcfunction(L, [](lua_State* L) -> int {
            ecs::World* world = check_world(L, 1);
            ecs::Entity e = check_entity(L, 2);
            auto mesh_handle = check_handle<gl::Mesh>(L, 3, "rydz.HandleMesh");
            
            world->insert_component(e, ecs::Mesh3d{mesh_handle});
            return 0;
        });
        lua_setfield(L, -2, "add_mesh3d");

        // world:add_mesh_material3d(entity, material_handle)
        lua_pushcfunction(L, [](lua_State* L) -> int {
            ecs::World* world = check_world(L, 1);
            ecs::Entity e = check_entity(L, 2);
            auto mat_handle = check_handle<ecs::Material>(L, 3, "rydz.HandleMaterial");
            
            world->insert_component(e, ecs::MeshMaterial3d{mat_handle});
            return 0;
        });
        lua_setfield(L, -2, "add_mesh_material3d");

        // Zdejmujemy __index + metatabele WORLD_MT
        lua_pop(L, 2);
    }
}
