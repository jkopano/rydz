///header do includowania lua c api Luaresource istnieje w rydz_scripting/lua_resource.hpp
#pragma once

#include "rydz_ecs/mod.hpp"
#include <iostream>

//wylaczenie name manglingu dla C
extern "C" {
	#include "lua.h"
	#include "lualib.h"
	#include "lauxlib.h"
}