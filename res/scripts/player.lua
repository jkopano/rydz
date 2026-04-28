-- Logika gracza - odpowiednik spawn_player + player_movement_system z C++
-- Gracz jest kostka poruszajaca sie w rzucie izometrycznym
 
print("[player.lua] Zaladowany!")
 
-- ─ Stale ─
 
local PLAYER_SPEED = 5.0  -- jednostki na sekunde
 
-- Wektory kierunkow izometrycznych (identyczne jak w C++ player_movement_system)
local ISO_FORWARD = { x = -0.7071, y = 0.0, z = -0.7071 } -- W
local ISO_RIGHT   = { x =  0.7071, y = 0.0, z = -0.7071 } -- D
 
-- ─ Startup: stwórz encje gracza ─
 
Rydz.on_startup(function(world)
    print("[player.lua] Tworzenie gracza...")
 
    local tex_handle = asset_loader("res/textures/stone.jpg")
 
    player_entity = world:spawn({
        { Components.LuaComponent, {
            is_player  = true,
            speed      = PLAYER_SPEED,
            health     = 100,
            max_health = 100,
        }},
        { Components.Mesh3d, Components.Mesh3d.cube(1.0, 1.0, 1.0) },
        { Components.MeshMaterial3d, Components.MeshMaterial3d.StandardMaterial(tex_handle) },
        { Components.Transform, { translation = { x = 0.0, y = 0.5, z = 0.0 } } },
        { Components.CameraTarget }
    })
 
    print("[player.lua] Gracz stworzony, entity ID: " .. tostring(player_entity))
end)
 
-- ─ System ruchu ─
 
local function PlayerMovement(world)
    for entity, data, t in world:each(Components.LuaComponent, Components.Transform) do
        if not data.is_player then goto continue end

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
        ::continue::
    end
end
 
Rydz.register_system(Schedule.Update, PlayerMovement, "PlayerMovement")
 
-- ─ System HUD (debug) ─
 
local frame = 0
local function DebugHUD(world)
    frame = frame + 1
    if frame % 300 ~= 0 then return end

    for entity, data in world:each(Components.LuaComponent) do
        if not data.is_player then goto continue end
        print(string.format("[HUD] Klatka %d | HP: %d/%d | FPS: %.0f",
            frame, data.health, data.max_health, 1.0 / Time.delta))
        ::continue::
    end
end
 
Rydz.register_system(Schedule.Update, DebugHUD, "DebugHUD")
