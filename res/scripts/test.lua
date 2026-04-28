-- Weryfikacja podstawowych funkcji modułu skryptowania
 
print("[test.lua] Zaladowany!")
 
-- Sprawdź że globale są dostępne
print("[test.lua] Components.Transform = " .. tostring(Components.Transform))
print("[test.lua] Schedule.Update = " .. tostring(Schedule.Update))
print("[test.lua] Schedule.Startup = " .. tostring(Schedule.Startup))
 
Rydz.on_startup(function(world)
    print("[test.lua] Startup!")
    print("[test.lua]   _world = " .. tostring(world))
 
    -- Test spawn/despawn
    local e = world:spawn()
    print("[test.lua]   Spawn entity: " .. tostring(e))
    print("[test.lua]   is_alive: " .. tostring(world:is_alive(e)))
 
    -- Test LuaComponent
    world:set_lua_component(e, { test_val = 42, name = "testowy" })
    local comp = world:get_lua_component(e)
    print("[test.lua]   LuaComponent.test_val = " .. tostring(comp.test_val))
    print("[test.lua]   LuaComponent.name = " .. comp.name)
 
    -- Despawn i sprawdź
    world:despawn(e)
    print("[test.lua]   Po despawn is_alive: " .. tostring(world:is_alive(e)))
 
    print("[test.lua] Wszystkie testy podstawowe ZALICZONE!")
end)

Rydz.on_startup(function(world)
    -- kilka encji z różnymi danymi
    local e1 = world:spawn()
    world:set_lua_component(e1, { tag = "gracz", active = true, hp = 100 })

    local e2 = world:spawn()
    world:set_lua_component(e2, { tag = "wrog", active = true, hp = 50 })

    local e3 = world:spawn()
    world:set_lua_component(e3, { tag = "gracz", active = false, hp = 80 })

    -- Test filtrowania po boolean
    local count_active = 0
    for e in world:each_lua_with("active", true) do
        count_active = count_active + 1
    end
    print("[Test each_lua_with] active=true: " .. count_active .. " (oczekiwane: 2 + gracz z player.lua)")

    -- Test filtrowania po stringu
    local count_gracz = 0
    for e in world:each_lua_with("tag", "gracz") do
        count_gracz = count_gracz + 1
    end
    print("[Test each_lua_with] tag=gracz: " .. count_gracz .. " (oczekiwane: 2)")

    -- Test filtrowania po nieistniejącym polu
    local count_none = 0
    for e in world:each_lua_with("nieistniejace_pole", true) do
        count_none = count_none + 1
    end
    print("[Test each_lua_with] nieistniejace_pole: " .. count_none .. " (oczekiwane: 0)")
end)
 
-- System sprawdzający Input i Time co sekundę
local timer = 0.0
Rydz.register_system(Schedule.Update, function(world)
    timer = timer + Time.delta
    if timer >= 3.0 then
        timer = 0.0
        print(string.format("[test.lua] Update dziala! delta=%.4f | W=%s | A=%s",
            Time.delta,
            tostring(Input.is_key_down(Input.KEY_W)),
            tostring(Input.is_key_down(Input.KEY_A))
        ))
    end
end, "TestUpdate")
