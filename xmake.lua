-- rules + options
add_rules("mode.debug", "mode.release", "mode.profile")
add_rules("plugin.compile_commands.autoupdate", { outputdir = "." })

option("tracy")
set_default(false)
set_showmenu(true)
set_description("Enable Tracy profiler")
option_end()

-- common toolchain + diagnostics flags
set_languages("c++23")
add_includedirs("lib")
set_warnings("all", "extra")

-- common dependencies
add_requires("taskflow", "gtest", "benchmark", "joltphysics", "glaze")
add_requires("sol2 v3.3.0", { configs = { includes_lua = false } })

-- `xmake f --tracy=y` lub `xmake f -m profile`
local tracy_enabled = has_config("tracy") and get_config("tracy") or is_mode("profile")
if tracy_enabled then
	add_requires("tracy")
	add_defines("TRACY_ENABLE")
end

if is_mode("release") or is_mode("profile") then
	set_optimize("fastest")
	if is_mode("profile") then
		set_symbols("debug")
	else
		set_symbols("hidden")
	end
end

-- targets common
target("raylib")
set_kind("static")
set_default(false)
set_languages("c11")
add_files(
	"lib/external/raylib/src/rcore.c",
	"lib/external/raylib/src/rshapes.c",
	"lib/external/raylib/src/rtextures.c",
	"lib/external/raylib/src/rtext.c",
	"lib/external/raylib/src/rmodels.c",
	"lib/external/raylib/src/raudio.c",
	"lib/external/raylib/src/rglfw.c"
)
add_includedirs("lib/external/raylib/src", { public = true })
add_includedirs("lib/external/raylib/src/external/glfw/include", { private = true })
add_defines("PLATFORM_DESKTOP", "GRAPHICS_API_OPENGL_33", { public = true })
add_defines("SUPPORT_GPU_SKINNING", { public = true })
add_cxflags("-w")
if is_mode("release") then
	set_optimize("fastest")
end

target("main")
set_kind("binary")
set_default(true)
set_rundir("$(projectdir)")
add_files("src/*.cpp")
add_deps("raylib")
add_packages("taskflow", "joltphysics", "sol2", "glaze")

if tracy_enabled then
	add_packages("tracy")
end
set_pcxxheader("lib/pch.hpp")

target("tests")
set_kind("binary")
set_default(false)
add_files("tests/*.cpp")
add_deps("raylib")
add_packages("gtest", "taskflow", "joltphysics", "glaze")

target("bench")
set_kind("binary")
set_default(false)
add_files("benches/*.cpp")
add_deps("raylib")
add_packages("benchmark", "taskflow", "joltphysics", "glaze")

examples = {
	"01_hello_window",
	"02_ecs_basics",
	"03_input",
	"04_events",
	"05_states",
	"06_rendering",
	"07_lighting",
	"08_hierarchy",
	"09_spawn_despawn",
	"10_custom_material",
	"11_sets",
}

for _, name in ipairs(examples) do
	target("example_" .. name)
	set_kind("binary")
	set_default(false)
	set_rundir("$(projectdir)")
	add_files("examples/" .. name .. "/main.cpp")
	add_deps("raylib")
	add_packages("taskflow", "joltphysics", "sol2", "glaze")
	if tracy_enabled then
		add_packages("tracy")
	end
	set_pcxxheader("lib/pch.hpp")
end

includes("xmake-platforms/windows.lua")
includes("xmake-platforms/nixos.lua")
includes("xmake-platforms/linux.lua")
