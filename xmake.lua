add_rules("mode.debug", "mode.release")

add_requires("abseil", "taskflow", "gtest", "benchmark", "meshoptimizer", "bvh")

set_languages("c++23")
add_includedirs("lib")

if is_mode("debug") then
	add_cxflags("-fsanitize=address,undefined", "-fno-omit-frame-pointer")
	add_ldflags("-fsanitize=address,undefined")

	add_cxflags("-fno-sanitize-recover=undefined")

	add_cxflags("-Wall", "-Wextra", "-Wpedantic", "-Wshadow")
end

if is_mode("release") then
	if os.getenv("NIX_ENFORCE_NO_NATIVE") then
		add_cxflags("-march=x86-64-v3")
	else
		add_cxflags("-march=native")
	end
	set_symbols("hidden")
	set_optimize("fastest")
end

-- Local raylib built from source
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
	add_includedirs("lib/external/raylib/src", {public = true})
	add_includedirs("lib/external/raylib/src/external/glfw/include", {private = true})
	add_defines("PLATFORM_DESKTOP", "GRAPHICS_API_OPENGL_33", {public = true})
	add_defines("_GLFW_X11")
	add_syslinks("GL", "X11", "pthread", "dl", "m", "rt")
	-- Suppress warnings in third-party code
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
	add_packages("taskflow", "abseil", "meshoptimizer", "bvh")
	set_pcxxheader("lib/pch.hpp")

target("tests")
	set_kind("binary")
	set_default(false)
	add_files("tests/*.cpp")
	add_deps("raylib")
	add_packages("gtest", "taskflow", "abseil", "meshoptimizer", "bvh")

target("bench")
	set_kind("binary")
	set_default(false)
	add_files("benches/*.cpp")
	add_deps("raylib")
	add_packages("benchmark", "taskflow", "abseil", "meshoptimizer", "bvh")
