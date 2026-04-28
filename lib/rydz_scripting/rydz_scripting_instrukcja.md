
# Instroukcja oraz przydatne informacje na temat modułu `rydz_scripting`

## 1. Globalne zmienne i API dostępne w Lua

### `_world` — obiekt świata gry

Główny punkt dostępu do ECS. Wszystkie operacje na encjach i komponentach.

| Metoda | Opis | Zwraca |
|---|---|---|
| `_world:spawn()` | Tworzy nową encję | `entity` (liczba) |
| `_world:despawn(entity)` | Usuwa encję | — |
| `_world:is_alive(entity)` | Sprawdza czy encja żyje | `bool` |
| `_world:set_lua_component(entity, table)` | Przypisuje tabelę Lua do encji | — |
| `_world:get_lua_component(entity)` | Zwraca tabelę Lua encji | `table` lub `nil` |
| `_world:get_component(entity, handle)` | Zwraca proxy komponentu C++ | proxy lub `nil` |
| `_world:add_transform(entity, x, y, z)` | Dodaje Transform do encji | — |
| `_world:each_lua()` | Iterator po encjach z LuaComponent | iterator |
| `_world:each_lua_with(key, value)` | Iterator po encjach z LuaComponent gdzie `table[key]==value` | iterator |
| `_world:load_texture(path)` | Ładuje teksturę z dysku | handle |
| `_world:add_mesh_cube(w, h, d)` | Tworzy siatkę sześcianu | handle |
| `_world:add_material_from_texture(tex)` | Tworzy materiał z tekstury | handle |
| `_world:add_mesh3d(entity, mesh)` | Dodaje Mesh3d do encji | — |
| `_world:add_mesh_material3d(entity, mat)` | Dodaje MeshMaterial3d do encji | — |

### `Components` — handlery komponentów C++

Tabela mapująca nazwy na liczby całkowite. Używana jako drugi argument `get_component`.

```lua
Components.Transform     -- handle do komponentu Transform
Components.CameraTarget  -- handle do znacznika kamery (jeśli zarejestrowany)
-- wartości są liczbami (0, 1, 2...) — nie polegaj na konkretnej wartości
```

### `Input` — odczyt klawiatury

```lua
Input.is_key_down(key)     -- czy klawisz jest wciśnięty (trzymany)
Input.is_key_pressed(key)  -- czy klawisz został wciśnięty w tej klatce
Input.is_key_released(key) -- czy klawisz został zwolniony w tej klatce

-- Stałe klawiszy:
Input.KEY_W, Input.KEY_S, Input.KEY_A, Input.KEY_D
Input.KEY_UP, Input.KEY_DOWN, Input.KEY_LEFT, Input.KEY_RIGHT
Input.KEY_SPACE, Input.KEY_ENTER, Input.KEY_ESCAPE
Input.KEY_SHIFT, Input.KEY_CTRL
Input.KEY_Q, Input.KEY_E, Input.KEY_F, Input.KEY_R
Input.KEY_TAB, Input.KEY_BACKSPACE
Input.KEY_1, Input.KEY_2, Input.KEY_3

-- Można też użyć surowego kodu raylib:
Input.is_key_down(87)  -- 87 = KEY_W
```

### `Time` — czas klatki

```lua
Time.delta   -- czas trwania poprzedniej klatki w sekundach (np. 0.016 przy 60fps)
```

### `Rydz` — rejestracja systemów

```lua
Rydz.register_system(schedule, function, name)
-- schedule: Schedule.Update lub Schedule.Startup
-- function: funkcja przyjmująca world jako argument
-- name: opcjonalna nazwa (do logowania błędów)

Rydz.on_startup(function)
-- skrót dla Rydz.register_system(Schedule.Startup, function)
```

### `Schedule` — stałe harmonogramu

```lua
Schedule.Startup   -- faza inicjalizacji (wykonuje się raz)
Schedule.Update    -- faza głównej pętli gry (co klatkę)
```

### Proxy `Transform`

Zwracany przez `world:get_component(entity, Components.Transform)`.

```lua
local t = world:get_component(entity, Components.Transform)
-- odczyt:
t.translation  -- tabela {x, y, z}
t.scale        -- tabela {x, y, z}
t.rotation     -- tabela {x, y, z} (lub {x,y,z,w} dla kwaternionów)

-- zapis:
t.translation = { x = 1.0, y = 0.5, z = 0.0 }
t.scale       = { x = 2.0, y = 2.0, z = 2.0 }
```

Proxy **nie cachuje** danych — każdy odczyt i zapis odpytuje ECS bezpośrednio. Zmiana `t.translation` natychmiast modyfikuje pozycję encji w silniku.

---

## 2. Pisanie skryptów gry — wzorce i przykłady

### Szablon skryptu

```lua
-- res/scripts/moj_system.lua

-- Zmienne lokalne — widoczne przez wszystkie funkcje w tym pliku
-- dzięki mechanizmowi upvalue
local moja_encja = nil
local licznik = 0

-- Inicjalizacja — wykonuje się raz po starcie
Rydz.on_startup(function(world)
    moja_encja = world:spawn()

    -- Grafika 3D
    local tex = world:load_texture("res/textures/moja_tekstura.png")
    local mat = world:add_material_from_texture(tex)
    local mesh = world:add_mesh_cube(1.0, 1.0, 1.0)
    world:add_mesh3d(moja_encja, mesh)
    world:add_mesh_material3d(moja_encja, mat)
    world:add_transform(moja_encja, 0.0, 0.5, 0.0)

    -- Dane logiki gry
    world:set_lua_component(moja_encja, {
        typ    = "gracz",
        hp     = 100,
        speed  = 5.0,
    })

    print("[moj_system.lua] Zainicjalizowany!")
end)

-- System Update — wykonuje się każdą klatkę
local function MojSystem(world)
    licznik = licznik + 1

    -- Iteracja po encjach z konkretnym typem
    for entity in world:each_lua_with("typ", "gracz") do
        local data = world:get_lua_component(entity)
        local t    = world:get_component(entity, Components.Transform)
        if not t then return end

        -- Logika ruchu
        local speed = data.speed * Time.delta
        if Input.is_key_down(Input.KEY_W) then
            local pos = t.translation
            t.translation = { x = pos.x, y = pos.y, z = pos.z - speed }
        end
    end
end

Rydz.register_system(Schedule.Update, MojSystem, "MojSystem")
```

### Komunikacja między skryptami

Skrypty ładowane z tego samego katalogu współdzielą przestrzeń nazw Lua (ten sam `lua_State`). Zmienna globalna zdefiniowana w jednym skrypcie jest widoczna w innym. Zmienne lokalne (`local`) są widoczne tylko w swoim pliku.

```lua
-- plik_a.lua
wspolna_zmienna = 42       -- globalna — widoczna wszędzie

-- plik_b.lua
print(wspolna_zmienna)     -- wypisze 42, jeśli plik_a.lua załadowany wcześniej
```

**Uwaga na kolejność ładowania:** pliki są ładowane przez `std::filesystem::directory_iterator` — kolejność zależy od systemu plików. Jeśli skrypt B zależy od zmiennej z skryptu A, użyj `on_startup` (gdzie kolejność jest deterministyczna) zamiast inicjalizacji na poziomie modułu.

### Wzorzec — wiele typów encji

```lua
-- Zamiast wielu globalnych zmiennych:
local encje = {}  -- lokalna tabela jako rejestr

Rydz.on_startup(function(world)
    encje.gracz = world:spawn()
    world:set_lua_component(encje.gracz, { typ = "gracz", hp = 100 })

    encje.wrog = world:spawn()
    world:set_lua_component(encje.wrog, { typ = "wrog", hp = 30 })
end)

Rydz.register_system(Schedule.Update, function(world)
    -- Iteracja po wszystkich wrogach:
    for entity in world:each_lua_with("typ", "wrog") do
        -- logika AI
    end
end, "WrogAI")
```

---

## 3. Konsola deweloperska

Konsola (otwierana przez `` ` `` lub `F1`) wykonuje Lua w tym samym VM co skrypty. Widzi wszystkie globalne zmienne i funkcje.

### Ważna zasada: `local` vs globalne

W konsoli każda komenda to osobne wywołanie `luaL_dostring`. Zmienne `local` **nie przeżywają** między komendami:

```lua
-- ŹLE — e znika po tej komendzie:
> local e = _world:spawn()
> print(e)  -- nil!

-- DOBRZE — e jest globalna, przeżywa:
> e = _world:spawn()
> print(e)  -- liczba ✓
```

### Przydatne komendy konsolowe

```lua
-- Podstawowe operacje na encjach:
> e = _world:spawn()
> print(e)
> print(_world:is_alive(e))
> _world:despawn(e)
> print(_world:is_alive(e))

-- LuaComponent przez konsolę:
> e = _world:spawn()
> _world:set_lua_component(e, { hp = 999, name = "testowy" })
> print(_world:get_lua_component(e).hp)
> print(_world:get_lua_component(e).name)

-- Inspekcja komponentów:
> print(Components.Transform)
> t = _world:get_component(player_entity, Components.Transform)
> print(t.translation.x)
> print(t.translation.y)

-- Teleportacja gracza:
> t = _world:get_component(player_entity, Components.Transform)
> t.translation = { x = 5.0, y = 0.5, z = 0.0 }

-- Iteracja przez konsolę:
> count = 0
> for e in _world:each_lua() do count = count + 1 end
> print(count)

-- Filtrowanie:
> for e in _world:each_lua_with("typ", "gracz") do print(e) end

-- Informacje o czasie:
> print(Time.delta)
> print(1.0 / Time.delta)  -- FPS

-- Test inputu:
> print(Input.is_key_down(Input.KEY_W))

-- Modyfikacja danych gracza na żywo:
> d = _world:get_lua_component(player_entity)
> d.speed = 20.0
> _world:set_lua_component(player_entity, d)
```

---

## 4. Instrukcja obsługi — dodawanie skryptowania do nowej sceny

Ta sekcja opisuje krok po kroku jak od zera podłączyć moduł skryptowania do nowej sceny i zacząć pisać logikę gry w Lua.

### Krok 1 — Dodaj includes do pliku sceny

Na górze pliku sceny (np. `src/moja_scena.hpp`) dodaj:

```cpp
#include "rydz_scripting/lua_resource.hpp"
#include "rydz_scripting/lua_system_registry.hpp"
#include "rydz_scripting/script_scheduler.hpp"
```

### Krok 2 — Napisz funkcję `init_lua_scripting`

Skopiuj tę funkcję do pliku sceny — jest identyczna dla każdej sceny:

```cpp
inline void init_lua_scripting(ecs::World& world) {
    auto* lua = world.get_resource<scripting::LuaResource>();
    auto* reg = world.get_resource<scripting::LuaSystemRegistry>();
    if (!lua) { fprintf(stderr, "[Scripting] Brak LuaResource!\n"); return; }

    scripting::register_rydz_api(lua->L);
    scripting::expose_world_global(lua->L, &world);

    if (reg) {
        lua_pushlightuserdata(lua->L, reg);
        lua_setfield(lua->L, LUA_REGISTRYINDEX, "_sys_registry");
    }

    const std::string dir = "res/scripts/";
    if (!std::filesystem::exists(dir)) return;
    for (auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.path().extension() != ".lua") continue;
        std::string path = entry.path().string();
        if (luaL_dofile(lua->L, path.c_str()) != LUA_OK) {
            fprintf(stderr, "[Lua] %s: %s\n", path.c_str(), lua_tostring(lua->L, -1));
            lua_pop(lua->L, 1);
        }
    }
}
```

Dodaj `#include <filesystem>` na górze pliku.

### Krok 3 — Skonfiguruj plugin sceny

W funkcji pluginu sceny dodaj w tej kolejności:

```cpp
inline void moja_scena_plugin(ecs::App& app) {
    // 1. Pluginy infrastrukturalne (kolejność ma znaczenie)
    app.add_plugin(Input::install);
    app.add_plugin(scripting::scripting_plugin);  // ← tworzy LuaResource
    app.add_plugin(engine::console_plugin);       // ← tworzy konsolę (opcjonalne)

    // 2. Wstaw rejestr systemów Lua (PRZED add_systems)
    app.insert_resource(scripting::LuaSystemRegistry{});

    // 3. Twoje systemy startup (środowisko 3D — kamera, oświetlenie itp.)
    app.add_systems(Startup, setup_camera);
    app.add_systems(Startup, setup_lighting);

    // 4. Inicjalizacja Lua + uruchomienie startup systemów Lua
    //    .chain() GWARANTUJE że init wykonuje się przed runner
    app.add_systems(ecs::ScheduleLabel::Startup,
        group(init_lua_scripting, scripting::lua_startup_runner).chain());

    // 5. Systemy Update — Lua runner
    app.add_systems(ecs::ScheduleLabel::Update, scripting::lua_update_runner);

    // 6. Konsola (opcjonalne — na końcu renderowania)
    app.add_systems(RenderPassSet::Cleanup,
        group(engine::ConsoleRenderSystem).before(FramePass::end));
}
```

### Krok 4 — Podłącz plugin w `main.cpp`

```cpp
#include "moja_scena.hpp"

int main() {
    ecs::App app;
    app.add_plugin(render_plugin);       // lub inny plugin renderowania
    app.add_plugin(moja_scena_plugin);   // ← Twoja scena
    app.run();
}
```

### Krok 5 — Utwórz katalog skryptów

```
res/
└── scripts/
    └── gracz.lua    ← tu piszesz logikę
```

### Krok 6 — Napisz pierwszy skrypt

Utwórz `res/scripts/gracz.lua`:

```lua
-- Zmienna lokalna — widoczna przez wszystkie funkcje w tym pliku
local player_entity = nil

-- Startup — tworzenie gracza
Rydz.on_startup(function(world)
    player_entity = world:spawn()

    -- Grafika 3D
    local tex  = world:load_texture("res/textures/stone.jpg")
    local mat  = world:add_material_from_texture(tex)
    local mesh = world:add_mesh_cube(1.0, 1.0, 1.0)
    world:add_mesh3d(player_entity, mesh)
    world:add_mesh_material3d(player_entity, mat)
    world:add_transform(player_entity, 0.0, 0.5, 0.0)

    -- Dane logiki
    world:set_lua_component(player_entity, {
        is_player = true,
        speed     = 5.0,
        hp        = 100,
    })

    print("[gracz.lua] Gracz stworzony! ID: " .. tostring(player_entity))
end)

-- System ruchu
local function Ruch(world)
    for entity in world:each_lua_with("is_player", true) do
        local t = world:get_component(entity, Components.Transform)
        if not t then return end

        local d = world:get_lua_component(entity)
        local s = d.speed * Time.delta
        local pos = t.translation

        local dx, dz = 0, 0
        if Input.is_key_down(Input.KEY_W) then dz = dz - s end
        if Input.is_key_down(Input.KEY_S) then dz = dz + s end
        if Input.is_key_down(Input.KEY_A) then dx = dx - s end
        if Input.is_key_down(Input.KEY_D) then dx = dx + s end

        if dx ~= 0 or dz ~= 0 then
            t.translation = { x = pos.x + dx, y = pos.y, z = pos.z + dz }
        end
    end
end

Rydz.register_system(Schedule.Update, Ruch, "Ruch")
```

### Krok 7 — Skompiluj i uruchom

```bash
xmake build main
xmake run
```

W stdout powinno pojawić się:
```
[gracz.lua] Gracz stworzony! ID: 1
```

Gracz powinien być widoczny na scenie i poruszać się klawiszami WSAD.

### Krok 8 — Testuj przez konsolę

Otwórz konsolę (`` ` `` lub `F1`) i sprawdź:

```lua
> print(_world)                    -- World
> print(Components.Transform)      -- liczba (np. 0 lub 1)
> t = _world:get_component(player_entity, Components.Transform)
> print(t.translation.x)           -- pozycja X gracza
> t.translation = {x=5, y=0.5, z=0}  -- teleportuj gracza
```

---

## 5. Najczęstsze błędy i rozwiązania

| Błąd | Przyczyna | Rozwiązanie |
|---|---|---|
| `attempt to index a nil value (global 'Rydz')` | Skrypt wczytany przed `register_rydz_api` | Sprawdź kolejność — `init_lua_scripting` musi być przed `luaL_dofile` |
| `attempt to index a nil value (global '_world')` | `expose_world_global` nie wywołane | Sprawdź że `expose_world_global` jest w `init_lua_scripting` przed `luaL_dofile` |
| `local e = ...` w konsoli, potem `e` to `nil` | `local` nie przeżywa między komendami | Używaj zmiennych globalnych w konsoli: `e = _world:spawn()` |
| `Invalid component handle: X` | Zły handle lub Components nie zainicjalizowane | Sprawdź `print(Components.Transform)` — jeśli `nil`, API nie jest zarejestrowane |
| Gracz istnieje ale się nie porusza | Transform nie dodany | Upewnij się że `world:add_transform(entity, x, y, z)` jest wywołane |
| Skrypt nie ładuje się | Błąd składni Lua lub zła ścieżka | Sprawdź stderr — `[Lua] res/scripts/X.lua: ...` |
| Błąd Lua crashuje silnik | `lua_call` zamiast `lua_pcall` | W `run_lua_systems` zawsze używamy `lua_pcall` — jeśli crashuje, błąd jest w C++ |
| `each_lua_with` zwraca 0 wyników | Pole nie istnieje lub zły typ | Sprawdź `print(world:get_lua_component(entity).pole)` czy faktycznie istnieje |
| `Input.is_key_down` zawsze `false` | `_world` nadpisane lub Input nie zainstalowany | Sprawdź `app.add_plugin(Input::install)` w pluginie sceny |
