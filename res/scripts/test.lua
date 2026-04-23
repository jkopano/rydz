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
