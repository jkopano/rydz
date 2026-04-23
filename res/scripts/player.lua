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
 
    -- Dane skryptowe — odpowiednik komponentu Player{} z C++
    world:set_lua_component(player_entity, {
        is_player  = true,
        speed      = PLAYER_SPEED,
        health     = 100,
        max_health = 100,
    })
 
    print("[player.lua] Gracz stworzony, entity ID: " .. tostring(player_entity))
    print("[player.lua] UWAGA: Transform musi byc dodany przez C++ (insert_component)")
    print("[player.lua] Na razie testujemy LuaComponent i Input")
end)
 
-- ── System ruchu ─────────────────────────────────────────────────────────────
 
local function PlayerMovement(world)
    if not player_entity then return end
 
    local data = world:get_lua_component(player_entity)
    if not data or not data.is_player then return end
 
    local t = world:get_component(player_entity, Components.Transform)
    if not t then return end  -- encja może nie mieć Transform jeszcze
 
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
        t.translation = {
            x = pos.x + dx,
            y = pos.y,
            z = pos.z + dz,
        }
    end
end
 
Rydz.register_system(Schedule.Update, PlayerMovement, "PlayerMovement")
 
-- ── System HUD (debug) ────────────────────────────────────────────────────────
 
local frame = 0
local function DebugHUD(world)
    frame = frame + 1
    if frame % 300 == 0 then  -- co 300 klatek (~5s przy 60fps)
        if player_entity then
            local data = world:get_lua_component(player_entity)
            if data then
                print(string.format("[HUD] Klatka %d | HP: %d/%d | FPS: %.0f",
                    frame, data.health, data.max_health, 1.0 / Time.delta))
            end
        end
    end
end
 
Rydz.register_system(Schedule.Update, DebugHUD, "DebugHUD")
