local function is_nixos()
	return is_host("linux") and os.exists("/etc/nixos")
end

if is_plat("linux") and not is_nixos() then
	-- Toolchain
	set_toolchains("clang")

	local function add_luajit(target)
		local luajit_link = os.iorun("which luajit"):gsub("%s+$", "")
		local luajit = luajit_link ~= "" and os.iorunv("readlink", { "-f", luajit_link }):gsub("%s+$", "") or ""
		local luajit_prefix = luajit ~= "" and path.directory(path.directory(luajit)) or nil
		if not luajit_prefix then
			raise("LuaJIT binary not found in PATH. Install LuaJIT or add it to PATH.")
		end
		target:add("includedirs", path.join(luajit_prefix, "include", "luajit-2.1"), { public = true })
		target:add("linkdirs", path.join(luajit_prefix, "lib"))
		target:add("links", "luajit-5.1")
		target:add("rpathdirs", path.join(luajit_prefix, "lib"))
	end

	-- Debug flags
	if is_mode("debug") then
		add_cxflags("-fsanitize=address,undefined", "-fno-omit-frame-pointer")
		add_ldflags("-fsanitize=address,undefined")
		add_cxflags("-fno-sanitize-recover=undefined")
		add_cxflags("-Wshadow")
	end

	-- Release/Profile flags
	if is_mode("release") or is_mode("profile") then
		add_cxflags("-march=native")
	end

	-- Raylib platform setup
	target("raylib")
		add_defines("_GLFW_X11")
		add_syslinks("GL", "X11", "pthread", "dl", "m", "rt")

	-- LuaJIT usage in targets that use sol2/console
	target("main")
		on_load(add_luajit)

	for _, name in ipairs(examples or {}) do
		target("example_" .. name)
			on_load(add_luajit)
	end
end
