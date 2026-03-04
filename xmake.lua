add_rules("mode.debug", "mode.release")

add_requires("raylib", "abseil", "taskflow", "gtest", "benchmark", "meshoptimizer", "bvh")

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

target("main")
set_kind("binary")
set_default(true)
set_rundir("$(projectdir)")
add_files("src/*.cpp")
add_packages("raylib", "taskflow", "abseil", "meshoptimizer", "bvh")
set_pcxxheader("lib/pch.hpp")

target("tests")
set_kind("binary")
set_default(false)
add_files("tests/*.cpp")
add_packages("gtest", "taskflow", "raylib", "abseil")

target("bench")
set_kind("binary")
set_default(false)
add_files("benches/*.cpp")
add_packages("benchmark", "taskflow", "raylib", "abseil")
