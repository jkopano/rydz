-- rules + options
add_rules("mode.debug", "mode.release", "mode.profile")
add_rules("plugin.compile_commands.autoupdate", { outputdir = "." })

option("tracy")
set_default(false)
set_showmenu(true)
set_description("Enable Tracy profiler")
option_end()

-- toolchains (clangd likes clang compile commands)
if is_plat("windows") then
	set_toolchains("clang-cl")
elseif is_plat("linux") then
	set_toolchains("clang")
	if os.getenv("NIX_PATH") then
		add_ldflags("-fuse-ld=mold", { force = true })
	end
end

-- helpers
local function is_msvc_like()
	if not is_plat("windows") then
		return false
	end
	if type(is_toolchain) == "function" then
		return is_toolchain("msvc") or is_toolchain("clang-cl")
	end
	local tc = get_config("toolchain") or ""
	return tc == "msvc" or tc == "clang-cl"
end

local tracy_enabled = has_config("tracy") and get_config("tracy") or is_mode("profile")
-- local tracy_enabled = false
local function add_tracy()
	if tracy_enabled then
		add_packages("tracy")
		if is_plat("linux") then
			-- system_name_of() uses dladdr(); exported executable symbols are
			-- needed so Tracy gets function names instead of raw addresses.
			add_ldflags("-rdynamic", { force = true })
		end
	end
end

-- common language / warnings
set_languages("c++23")
add_includedirs("lib")
set_warnings("all", "extra")

if is_plat("windows") then
	-- Dla clang-cl / msvc (jeśli używasz clang-cl, to zrozumie te flagi)
	add_cxflags("-Wfloat-conversion", "-Wconstant-conversion")
else
	-- Dla czystego Clanga na Linuxie
	add_cxflags("-Wfloat-conversion", "-Wconversion")
end

-- common dependencies
add_requires("taskflow", "gtest", "benchmark", "joltphysics", "glaze", "glm")
add_requires("fmt", "spdlog", { configs = { external_fmt = true } })

if is_plat("windows") then
    add_cxflags("-UJPH_FLOATING_POINT_EXCEPTIONS_ENABLED", {force = true})
end

if tracy_enabled then
	add_requires("tracy")
	add_defines("TRACY_ENABLE")
end

if is_plat("windows") then
	add_defines("NOMINMAX", "_CRT_SECURE_NO_WARNINGS", "_ENABLE_EXTENDED_ALIGNED_STORAGE")
	add_requires("luajit v2.1")
elseif is_plat("linux") then
	add_requires("luajit v2.1")
end

-- debug flags
if is_mode("debug") then
	if is_plat("windows") then
		if is_msvc_like() then
			-- Windows MSVC Sanitizer (wymaga zainstalowanego komponentu ASan w VS)
			add_cxflags("/fsanitize=address")
			add_ldflags("/fsanitize=address")
		end
	else
		add_cxflags("-fsanitize=address,undefined", "-fno-omit-frame-pointer")
		add_ldflags("-fsanitize=address,undefined")
		add_cxflags("-fno-sanitize-recover=undefined")
	end
	add_cxflags("-Wshadow")
end

-- release/profile flags
if is_mode("release") or is_mode("profile") then
	if is_plat("windows") then
		if is_msvc_like() then
			add_cxflags("/arch:AVX2") -- Odpowiednik x86-64-v3 dla MSVC
		else
			add_cxflags("-mavx2")
		end
	else
		if os.getenv("NIX_ENFORCE_NO_NATIVE") then
			add_cxflags("-march=x86-64-v3")
		else
			add_cxflags("-march=native")
		end
	end
	set_optimize("fastest")
	if is_mode("profile") then
		set_symbols("debug")
	else
		set_symbols("hidden")
	end
end

-- targets
target("raylib")
set_kind("static")
set_default(false)
set_languages("c11")
-- sources
add_files(
	"lib/external/raylib/src/rcore.c",
	"lib/external/raylib/src/rshapes.c",
	"lib/external/raylib/src/rtextures.c",
	"lib/external/raylib/src/rtext.c",
	"lib/external/raylib/src/rmodels.c",
	"lib/external/raylib/src/raudio.c",
	"lib/external/raylib/src/rglfw.c"
)
-- includes
add_includedirs("lib/external/raylib/src", { public = true })
add_includedirs("lib/external/raylib/src/external/glfw/include", { private = true })
-- defines
add_defines("PLATFORM_DESKTOP", "GRAPHICS_API_OPENGL_43", { public = true })
add_defines("SUPPORT_GPU_SKINNING", { public = true })

-- platform syslinks
if is_plat("windows") then
	add_defines("_GLFW_WIN32")
	add_syslinks("gdi32", "user32", "shell32", "winmm", "opengl32")
else
	add_defines("_GLFW_X11")
	add_syslinks("GL", "X11", "pthread", "dl", "m", "rt")
end

add_cxflags("-w") -- Wycisz ostrzeżenia w raylib
if is_mode("release") then
	set_optimize("fastest")
end

target("main")
set_kind("binary")
set_default(true)
set_rundir("$(projectdir)")
add_files("src/*.cpp")
add_deps("raylib")
add_packages("taskflow", "joltphysics", "glaze", "glm", "spdlog", "fmt")
if is_plat("windows") then
	add_packages("luajit")
elseif is_plat("linux") then
	add_packages("luajit")
end
add_tracy()
set_pcxxheader("lib/pch.hpp")

target("modules")
set_kind("phony")
set_default(false)
add_headerfiles("lib/rydz_**/*.hpp")
-- add_deps("raylib")
-- add_packages("taskflow", "joltphysics", "glaze")
-- if is_plat("windows") then
-- 	add_packages("luajit")
-- elseif is_plat("linux") then
-- 	add_packages("luajit")
-- end

target("tests")
set_kind("binary")
set_default(false)
add_files("tests/*.cpp")
add_deps("raylib")
add_packages("gtest", "taskflow", "joltphysics", "glaze", "glm", "spdlog", "fmt")

target("bench")
set_kind("binary")
set_default(false)
add_files("benches/*.cpp")
add_deps("raylib")
add_packages("benchmark", "taskflow", "joltphysics", "glaze", "glm", "spdlog", "fmt")

local examples = {
	"01_hello_window",
	"02_ecs_basics",
	"03_input",
	"04_messages",
	"05_states",
	"06_rendering",
	"07_lighting",
	"08_hierarchy",
	"09_spawn_despawn",
	"10_custom_material",
	"11_sets",
	"12_observers",
	"13_physics",
	"14_audio_system",
}

for _, name in ipairs(examples) do
	target("example_" .. name)
	set_kind("binary")
	set_default(false)
	set_rundir("$(projectdir)")
	add_files("examples/" .. name .. "/main.cpp")
	add_deps("raylib")
	add_packages("taskflow", "joltphysics", "glaze", "glm", "spdlog", "fmt")
	if is_plat("windows") then
		add_packages("luajit")
	elseif is_plat("linux") then
		add_packages("luajit")
	end
	add_tracy()
	set_pcxxheader("lib/pch.hpp")
end
