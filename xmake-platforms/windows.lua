if is_plat("windows") then
	-- Toolchain
	set_toolchains("clang-cl")

	-- Windows-specific defines
	add_defines("NOMINMAX", "_CRT_SECURE_NO_WARNINGS")

	-- LuaJIT package for Windows
	add_requires("luajit v2.1.0-beta3")

	local function is_msvc_like()
		if type(is_toolchain) == "function" then
			return is_toolchain("msvc") or is_toolchain("clang-cl")
		end
		local tc = get_config("toolchain") or ""
		return tc == "msvc" or tc == "clang-cl"
	end

	-- Debug flags
	if is_mode("debug") and is_msvc_like() then
		add_cxflags("/fsanitize=address")
		add_ldflags("/fsanitize=address")
	end

	-- Release/Profile flags
	if is_mode("release") or is_mode("profile") then
		if is_msvc_like() then
			add_cxflags("/arch:AVX2")
		else
			add_cxflags("-mavx2")
		end
	end

	-- Raylib platform setup
	target("raylib")
		add_defines("_GLFW_WIN32")
		add_syslinks("gdi32", "user32", "shell32", "winmm", "opengl32")

	-- LuaJIT usage in targets that use sol2/console
	target("main")
		add_packages("luajit")

	for _, name in ipairs(examples or {}) do
		target("example_" .. name)
			add_packages("luajit")
	end
end
