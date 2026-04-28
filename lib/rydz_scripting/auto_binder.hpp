#pragma once
#include <tuple>
#include <type_traits>
#include <string>
#include "rydz_ecs/mod.hpp"
#include "bindings/bind_world.hpp"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

namespace scripting {

    // --- Type Traits for auto-binder ---
    
    template<typename T>
    struct LuaArg {
        static T get(lua_State* L, int idx) {
            // default unhandled
            static_assert(sizeof(T) == 0, "Unsupported argument type for auto-binder");
            return T{};
        }
    };

    template<> struct LuaArg<float> { static float get(lua_State* L, int idx) { return (float)luaL_checknumber(L, idx); } };
    template<> struct LuaArg<int> { static int get(lua_State* L, int idx) { return (int)luaL_checkinteger(L, idx); } };
    template<> struct LuaArg<bool> { static bool get(lua_State* L, int idx) { return lua_toboolean(L, idx) != 0; } };
    template<> struct LuaArg<std::string> { static std::string get(lua_State* L, int idx) { return std::string(luaL_checkstring(L, idx)); } };
    
    // Niejawny dostęp do _world
    template<> struct LuaArg<ecs::World*> { 
        static ecs::World* get(lua_State* L, int idx) {
            lua_getglobal(L, "_world");
            auto w = check_world(L, -1);
            lua_pop(L, 1);
            return w;
        } 
    };

    template<typename T>
    struct LuaRet {
        static int push(lua_State* L, const T& val) {
            // default unhandled
            static_assert(sizeof(T) == 0, "Unsupported return type for auto-binder");
            return 0;
        }
    };

    template<> struct LuaRet<void> { static int push(lua_State* L) { return 0; } };
    template<> struct LuaRet<float> { static int push(lua_State* L, float val) { lua_pushnumber(L, val); return 1; } };
    template<> struct LuaRet<int> { static int push(lua_State* L, int val) { lua_pushinteger(L, val); return 1; } };
    template<> struct LuaRet<bool> { static int push(lua_State* L, bool val) { lua_pushboolean(L, val); return 1; } };
    template<> struct LuaRet<std::string> { static int push(lua_State* L, const std::string& val) { lua_pushstring(L, val.c_str()); return 1; } };

    // --- Unpacker ---

    template<typename T> struct TypeTag { using type = T; };
    
    template<auto Func> struct LuaFunctionWrapper;

    template<typename R, typename... Args, R(*Func)(Args...)>
    struct LuaFunctionWrapper<Func> {
        
        template<std::size_t... Is>
        static int invoke(lua_State* L, std::index_sequence<Is...>) {
            
            int stack_idx = 1;
            auto get_arg_func = [&](auto type_tag) {
                using ArgType = typename decltype(type_tag)::type;
                if constexpr (std::is_same_v<ArgType, ecs::World*>) {
                    return LuaArg<ecs::World*>::get(L, 0); 
                } else {
                    return LuaArg<ArgType>::get(L, stack_idx++);
                }
            };
            
            // Gwarantowana kolejnosc inicjalizacji od lewej do prawej
            std::tuple<Args...> args { get_arg_func(TypeTag<Args>{})... };

            if constexpr (std::is_void_v<R>) {
                std::apply(Func, args);
                return 0;
            } else {
                R ret = std::apply(Func, args);
                return LuaRet<R>::push(L, ret);
            }
        }

        static int call(lua_State* L) {
            return invoke(L, std::index_sequence_for<Args...>{});
        }
    };

    // Glowna funkcja rejestrujaca dla uzytkownika
    template<auto Func>
    inline void bind_function(lua_State* L, const char* name) {
        lua_pushcfunction(L, LuaFunctionWrapper<Func>::call);
        lua_setfield(L, -2, name);
    }
}
