print("[Lua] Skrypt załadowany!")

Rydz.on_startup(function(world)
    print("[Lua] Startup działa! World: " .. tostring(world))
    e = world:spawn()
    print("[Lua] Spawn: " .. tostring(e))
end)

Rydz.register_system(Schedule.Update, function(world)
    -- puste — sprawdzamy że nie crashuje
end, "EmptyUpdate")