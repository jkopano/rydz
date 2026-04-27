-- Logika gracza — odpowiednik spawn_player + player_movement_system z C++
-- Gracz jest kostką poruszającą się w rzucie izometrycznym
 
print("[player.lua] Zaladowany!")
 
-- ── Stałe ────────────────────────────────────────────────────────────────────
 
local PLAYER_SPEED = 5.0  -- jednostki na sekundę
 
-- Wektory kierunków izometrycznych (identyczne jak w C++ player_movement_system)
local ISO_FORWARD = { x = -0.7071, y = 0.0, z = -0.7071 } -- W
local ISO_RIGHT   = { x =  0.7071, y = 0.0, z = -0.7071 } -- D
 
-- ── Startup: stwórz encję gracza ─────────────────────────────────────────────
 
Rydz.on_startup(function(world)
    print("[player.lua] Tworzenie gracza...")
 
    player_entity = world:spawn()
 
    -- Dane skryptowe
    world:set_lua_component(player_entity, {
        is_player  = true,
        speed      = PLAYER_SPEED,
        health     = 100,
        max_health = 100,
    })
 
    print("[player.lua] Gracz stworzony, entity ID: " .. tostring(player_entity))
    
    -- Dodanie reprezentacji 3D
    print("[player.lua] Tworzenie grafiki...")
    local tex_handle = world:load_texture("res/textures/stone.jpg")
    local mat_handle = world:add_material_from_texture(tex_handle)
    local mesh_handle = world:add_mesh_cube(1.0, 1.0, 1.0)
    
    world:add_mesh3d(player_entity, mesh_handle)
    world:add_mesh_material3d(player_entity, mat_handle)
    world:add_transform(player_entity, 0.0, 0.5, 0.0)

    -- Przypisz znacznik kamery do gracza
    world:insert_component(player_entity, Components.CameraTarget)

    print("[player.lua] Komponenty dodane!")
end)
 
-- ── System ruchu ─────────────────────────────────────────────────────────────
 
local function PlayerMovement(world)
    for entity in world:each_lua_with("is_player", true) do
        local data = world:get_lua_component(entity)
        local t    = world:get_component(entity, Components.Transform)
        if not t then return end

        local speed = data.speed * Time.delta
        local dx, dz = 0.0, 0.0

        if Input.is_key_down(Input.KEY_W) then
            dx = dx + ISO_FORWARD.x * speed
            dz = dz + ISO_FORWARD.z * speed
        end
        if Input.is_key_down(Input.KEY_S) then
            dx = dx - ISO_FORWARD.x * speed
            dz = dz - ISO_FORWARD.z * speed
        end
        if Input.is_key_down(Input.KEY_D) then
            dx = dx + ISO_RIGHT.x * speed
            dz = dz + ISO_RIGHT.z * speed
        end
        if Input.is_key_down(Input.KEY_A) then
            dx = dx - ISO_RIGHT.x * speed
            dz = dz - ISO_RIGHT.z * speed
        end

        if dx ~= 0 or dz ~= 0 then
            local pos = t.translation
            t.translation = { x = pos.x + dx, y = pos.y, z = pos.z + dz }
        end
    end
end
 
Rydz.register_system(Schedule.Update, PlayerMovement, "PlayerMovement")
 
-- ── System HUD (debug) ────────────────────────────────────────────────────────
 
local frame = 0
local function DebugHUD(world)
    frame = frame + 1
    if frame % 300 ~= 0 then return end

    for entity in world:each_lua_with("is_player", true) do
        local data = world:get_lua_component(entity)
        print(string.format("[HUD] Klatka %d | HP: %d/%d | FPS: %.0f",
            frame, data.health, data.max_health, 1.0 / Time.delta))
    end
end
 
Rydz.register_system(Schedule.Update, DebugHUD, "DebugHUD")
