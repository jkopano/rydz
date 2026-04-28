//Rejestr komponentow
#pragma once

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

#include <rydz_ecs/mod.hpp>
#include <type_traits>
#include <vector>
#include <functional>
#include <string>

namespace scripting {

	struct ComponentTypeUD {
		int handle;
		const char* name;
	};

	template<typename T>
	struct LuaComponentTraits {
		static void bind_methods(lua_State* L) {}
		static void init_from_lua(lua_State* L, ecs::World* w, ecs::Entity e, int val_idx) {
			// default: nothing
		}
	};

	struct ComponentFunctions {
		using PushFn = std::function<int(lua_State*, ecs::World*, ecs::Entity)>;
		using HasFn = std::function<bool(ecs::World*, ecs::Entity)>;
		using InsertFn = std::function<bool(ecs::World*, ecs::Entity)>;
		using GetEntitiesFn = std::function<std::vector<ecs::Entity>(ecs::World*)>;
		using InitFromLuaFn = std::function<void(lua_State*, ecs::World*, ecs::Entity, int)>;
		using BindMethodsFn = std::function<void(lua_State*)>;

		std::string name; 
		PushFn push;
		HasFn has;
		InsertFn insert;
		GetEntitiesFn get_entities;
		InitFromLuaFn init_from_lua;
		BindMethodsFn bind_methods;
	};

	class ComponentRegistry {
	public:
		static ComponentRegistry& get() {
			static ComponentRegistry instance;
			return instance;
		}

		template<typename T>
		int register_component(const std::string& name, ComponentFunctions::PushFn push_fn = nullptr) {
			ComponentFunctions fns;
			fns.name = name;
			fns.push = std::move(push_fn);
			
			fns.has = [](ecs::World* w, ecs::Entity e) -> bool {
				return w->get_component<T>(e) != nullptr;
			};
			
			fns.insert = [](ecs::World* w, ecs::Entity e) -> bool {
				if constexpr (std::is_default_constructible_v<T>) {
					w->insert_component<T>(e, T{});
					return true;
				}
				return false;
			};

			fns.get_entities = [](ecs::World* w) {
				std::vector<ecs::Entity> res;
				auto* storage = w->get_storage<T>();
				if (storage) {
					for (auto e : storage->entities()) res.push_back(e);
				}
				return res;
			};

			fns.init_from_lua = [](lua_State* L, ecs::World* w, ecs::Entity e, int val_idx) {
				LuaComponentTraits<T>::init_from_lua(L, w, e, val_idx);
			};

			fns.bind_methods = [](lua_State* L) {
				LuaComponentTraits<T>::bind_methods(L);
			};

			int handle = (int)m_components.size();
			m_components.push_back(std::move(fns));
			return handle;
		}

		const ComponentFunctions* lookup(int handle) const {
			if (handle < 0 || handle >= (int)m_components.size()) return nullptr;
			return &m_components[handle];
		}

		void fill_lua_table(lua_State* L) const {
			for (int i = 0; i < (int)m_components.size(); ++i) {
				auto* ud = (ComponentTypeUD*)lua_newuserdata(L, sizeof(ComponentTypeUD));
				ud->handle = i;
				ud->name = m_components[i].name.c_str();

				std::string mt_name = "rydz.ComponentType." + m_components[i].name;
				luaL_newmetatable(L, mt_name.c_str());
				
				lua_newtable(L);
				if (m_components[i].bind_methods) {
					m_components[i].bind_methods(L);
				}
				lua_setfield(L, -2, "__index");
				
				lua_setmetatable(L, -2);
				lua_setfield(L, -2, m_components[i].name.c_str());
			}
		}

		int count() const { return (int)m_components.size(); }

	private:
		std::vector<ComponentFunctions> m_components;
	};

}
