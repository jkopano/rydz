//Rejestr komponentów indeksowany liczbami calkowitymi, kazdy komponent c++ dostaje stały ID przypisany podczas rejestracji
//globalna tabela w Lua eksportuje te ID jako stałe
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

	struct ComponentFunctions {
		//Push proxy komponentu na stos Lua. zwraca liczbę pushniętych wartosci
		using PushFn = std::function<int(lua_State*, ecs::World*, ecs::Entity)>;
		//Sprawdza czy encja ma komponent
		using HasFn = std::function<bool(ecs::World*, ecs::Entity)>;
		//Zwraca true jesli dodanie sie powiodlo
		using InsertFn = std::function<bool(ecs::World*, ecs::Entity)>;

		std::string name; //debug
		PushFn push;
		HasFn has;
		InsertFn insert;
	};

	//Singleton - globalny rejestr komponentów indeksowany przez int, indeksy przydzielane kolejno podczas rejestracji, trafiają do tabeli Components w Lua jako stałe
	class ComponentRegistry {
	public:
		static ComponentRegistry& get() {
			static ComponentRegistry instance;
			return instance;
		}

		//Rejestruje komponent i zwraca jego indeks
		template<typename T>
		int register_component(const std::string& name, ComponentFunctions::PushFn push_fn, ComponentFunctions::HasFn has_fn = nullptr, ComponentFunctions::InsertFn insert_fn = nullptr) {
			ComponentFunctions fns;
			fns.name = name;
			fns.push = std::move(push_fn);
			fns.has = has_fn ? std::move(has_fn) : [](ecs::World* w, ecs::Entity e) -> bool {
				return w->get_component<T>(e) != nullptr;
				};
			fns.insert = insert_fn ? std::move(insert_fn) : [](ecs::World* w, ecs::Entity e) -> bool {
				if constexpr (std::is_default_constructible_v<T>) {
					w->insert_component<T>(e, T{});
					return true;
				}
				return false;
				};
			int handle = (int)m_components.size();
			m_components.push_back(std::move(fns));
			return handle;
		}

		//Lookup po int, zwraca nullptr jesli poza zakresem
		const ComponentFunctions* lookup(int handle) const {
			if (handle < 0 || handle >= (int)m_components.size()) return nullptr;
			return &m_components[handle];
		}

		//Wypelnia tabelę Lua na szczycie stosu parami name=handle
		void fill_lua_table(lua_State* L) const {
			for (int i = 0; i < (int)m_components.size(); ++i) {
				lua_pushinteger(L, i);
				lua_setfield(L, -2, m_components[i].name.c_str());
			}
		}

		int count() const { return(int)m_components.size(); }

	private:
		std::vector<ComponentFunctions> m_components;
	};

}
