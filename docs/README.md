# rydz_ecs — Dokumentacja

## Spis treści

1. [Przegląd](#przegląd)
2. [Szybki start](#szybki-start)
3. [Entity — Encje](#entity--encje)
4. [Komponenty](#komponenty)
5. [Storage — Przechowywanie komponentów](#storage--przechowywanie-komponentów)
6. [Systemy](#systemy)
7. [Parametry systemów](#parametry-systemów)
8. [Query — Zapytania](#query--zapytania)
9. [Filtry](#filtry)
10. [Komendy (Cmd)](#komendy-cmd)
11. [Zasoby (Resources)](#zasoby-resources)
12. [Eventy](#eventy)
13. [Harmonogram (Schedule)](#harmonogram-schedule)
14. [Kolejność i grupowanie systemów](#kolejność-i-grupowanie-systemów)
15. [Warunki uruchamiania (run_if)](#warunki-uruchamiania-run_if)
16. [Sety systemów](#sety-systemów)
17. [Równoległość](#równoległość)
18. [Stany (State)](#stany-state)
19. [Detekcja zmian (Change Detection)](#detekcja-zmian-change-detection)
20. [Bundle](#bundle)
21. [Pluginy](#pluginy)
22. [App](#app)

---

## Przegląd

`rydz_ecs` to framework ECS (Entity Component System) napisany w C++23. Inspirowany jest architekturą Bevy ECS. Kluczowe cechy:

- **Sparse-set storage** — domyślne przechowywanie komponentów z O(1) insert/remove/lookup
- **Automatyczna równoległość** — systemy są analizowane pod kątem dostępu do danych i uruchamiane równolegle przez Taskflow
- **Compile-time trait introspection** — parametry systemów są wyciągane z sygnatury funkcji w czasie kompilacji
- **Detekcja zmian** — śledzenie dodawania i modyfikacji komponentów przez system ticków
- **Deferred commands** — bezpieczne tworzenie/usuwanie encji bez unieważniania iteratorów

Namespace: `ecs`

---

## Szybki start

```cpp
#include "rydz_ecs/rydz_ecs.hpp"

struct Position { f32 x, y; };
struct Velocity { f32 x, y; };

void setup(ecs::Cmd cmd) {
    cmd.spawn(Position{0, 0}, Velocity{1, 2});
    cmd.spawn(Position{5, 5}, Velocity{-1, 0});
}

void movement(ecs::Query<ecs::Mut<Position>, Velocity> q, ecs::Res<ecs::Time> time) {
    q.each([&](ecs::Mut<Position> pos, const Velocity *vel) {
        pos->x += vel->x * time->delta_seconds;
        pos->y += vel->y * time->delta_seconds;
    });
}

int main() {
    ecs::App app;
    app.add_plugin(ecs::window_plugin({800, 600, "Demo", 60}))
       .add_systems(ecs::ScheduleLabel::Startup, setup)
       .add_systems(ecs::ScheduleLabel::Update, movement)
       .run();
}
```

---

## Entity — Encje

Encja to lekki identyfikator składający się z indeksu i generacji:

```cpp
struct Entity {
    u32 index_val;
    u32 generation_val;
};
```

Generacja zapobiega problemowi ABA — po zniszczeniu encji i ponownym użyciu tego samego indeksu, stary uchwyt (z niższą generacją) nie będzie już ważny.

`EntityManager` zarządza tworzeniem i recyklingiem encji:

```cpp
Entity e = world.spawn();             // Tworzy nową encję
world.despawn(e);                     // Niszczy encję (usuwa wszystkie komponenty)
bool alive = world.entities.is_alive(e);  // Sprawdza czy encja żyje
size_t n = world.entities.count();    // Liczba aktywnych encji
```

---

## Komponenty

Komponent to dowolna struktura C++. Nie wymaga żadnego markera — wystarczy zwykły struct:

```cpp
struct Position { f32 x, y; };
struct Health { i32 hp; };
struct EnemyTag {};  // Tag — komponent bez danych
```

Operacje na komponentach przez World:

```cpp
world.insert_component(entity, Position{10, 20});
auto *pos = world.get_component<Position>(entity);  // nullptr jeśli brak
world.remove_component<Position>(entity);
bool has = world.has_component<Position>(entity);
```

---

## Storage — Przechowywanie komponentów

Każdy typ komponentu ma osobne storage. Domyślnie używany jest `SparseSetStorage`.

### SparseSetStorage (domyślny)

Struktura danych:
- `sparse_` — tablica indeksowana `entity.index()`, mapuje na pozycję w dense
- `dense_data_` — gęsta tablica wartości komponentów
- `dense_ticks_` — gęsta tablica ticków (do detekcji zmian)
- `dense_entities_` — gęsta tablica odpowiadających encji

Cechy:
- O(1) insert, remove (swap-and-pop), lookup
- Gęsta iteracja — cache-friendly
- Usuwanie przez zamianę z ostatnim elementem

### HashMapStorage

Alternatywne storage oparte na `std::unordered_map`. Wybierane przez typedef w strukturze komponentu:

```cpp
struct RareComponent {
    using Storage = ecs::HashMapStorage<RareComponent>;
    int data;
};
```

Odpowiednie dla komponentów posiadanych przez niewiele encji, gdzie sparse array byłby marnotrawstwem pamięci.

---

## Systemy

System to zwykła funkcja. ECS analizuje jej sygnaturę w compile-time i automatycznie dostarcza odpowiednie parametry:

```cpp
void my_system(ecs::Query<Position> q, ecs::Res<ecs::Time> time) {
    // ...
}
```

Pod spodem systemy są opakowywane w `FunctionSystem<F>`, który:
1. Wyciąga typy argumentów przez `function_traits`
2. Dla każdego argumentu wywołuje `SystemParamTraits<T>::retrieve(world)` aby pobrać dane
3. Wywołuje funkcję z pobranymi parametrami

Interfejs bazowy:

```cpp
class ISystem {
    virtual void run(World &world) = 0;
    virtual std::string name() const = 0;
    virtual SystemAccess access() const;
};
```

---

## Parametry systemów

Dostępne typy parametrów:

| Typ parametru | Opis |
|---|---|
| `Query<Qs...>` | Zapytanie o encje z danymi komponentami |
| `Res<T>` | Odczyt zasobu (const) |
| `ResMut<T>` | Zapis zasobu (mutable) |
| `Cmd` | Kolejka komend (deferred spawn/despawn) |
| `EventWriter<E>` | Wysyłanie eventów |
| `EventReader<E>` | Odbiór eventów |
| `World &` | Ekskluzywny dostęp do świata (wymusza sekwencyjne wykonanie) |
| `NonSendMarker` | Wymusza wykonanie na main thread |

Każdy parametr implementuje `SystemParamTraits<T>` z metodami:
- `retrieve(World&)` — pobiera dane
- `access(SystemAccess&)` — deklaruje dostęp (do analizy równoległości)
- `available(const World&)` — sprawdza czy parametr jest dostępny

---

## Query — Zapytania

Query iteruje po encjach posiadających wymagane komponenty.

### Typy komponentów w Query

```cpp
Query<Position>                  // const Position* — odczyt
Query<Mut<Position>>             // Mut<Position> — zapis z detekcją zmian
Query<Opt<Position>>             // const Position* lub nullptr — opcjonalny
Query<Opt<Mut<Position>>>        // Mut<Position> (ptr może być null) — opcjonalny zapis
Query<Entity>                    // Entity — sam identyfikator encji
```

### Iteracja

```cpp
// for_each z lambda
q.each([](const Position *pos, Mut<Velocity> vel) {
    vel->x += pos->x;
});

// Range-based (zwraca tuple)
for (auto [pos, vel] : q.iter()) {
    // pos to const Position*, vel to Mut<Velocity>
}

// Jeden wynik
if (auto result = q.single()) {
    auto [pos, vel] = *result;
}
```

### Optymalizacja iteracji

Query automatycznie wybiera najmniejszą grupę encji do iteracji — szuka storage z najmniejszą liczbą elementów wśród wymaganych komponentów, a następnie sprawdza pozostałe warunki dla każdej encji z tej grupy.

---

## Filtry

Filtry to trailing parametry Query — muszą być na końcu listy typów:

```cpp
Query<Position, Velocity, With<Enemy>, Without<Dead>>
```

| Filtr | Opis |
|---|---|
| `With<T>` | Encja musi mieć komponent T |
| `Without<T>` | Encja nie może mieć komponentu T |
| `Added<T>` | Komponent T dodany w tym ticku |
| `Changed<T>` | Komponent T zmodyfikowany w tym ticku |
| `Or<F1, F2>` | Logiczne OR dwóch filtrów |

Filtry `With` i `Added` dostarczają kandydatów — jeśli żaden komponent w Query nie jest wymagany (np. same `Opt`), filtry mogą posłużyć jako źródło encji do iteracji.

---

## Komendy (Cmd)

Komendy pozwalają na bezpieczne modyfikacje świata z opóźnionym wykonaniem (po zakończeniu schedule):

```cpp
void spawn_system(ecs::Cmd cmd) {
    // Tworzenie encji z komponentami
    auto e = cmd.spawn(Position{0, 0}, Velocity{1, 0});

    // Pusta encja
    auto e2 = cmd.spawn_empty();

    // Dodawanie komponentów do istniejącej encji
    cmd.entity(e2.id()).insert(Position{5, 5});

    // Usuwanie komponentu
    cmd.entity(e.id()).remove<Velocity>();

    // Niszczenie encji
    cmd.despawn(some_entity);

    // Batch spawn
    std::vector<Position> positions = {{0,0}, {1,1}, {2,2}};
    cmd.spawn_batch(positions);

    // Wstawianie zasobu
    cmd.insert_resource(MyResource{42});
}
```

Komendy są kolejkowane w `CommandQueue` (zasób) i wykonywane po każdym etapie schedule przez `apply_commands()`.

---

## Zasoby (Resources)

Zasób to singleton — jedna instancja danego typu na cały World. Wymaga markera `using Type = Resource`:

```cpp
struct GameConfig {
    using T = ecs::Resource;
    f32 gravity = 9.81f;
    i32 max_enemies = 100;
};
```

Operacje:

```cpp
// Wstawianie
app.insert_resource(GameConfig{});
// lub
world.insert_resource(GameConfig{});

// Odczyt w systemie
void my_system(ecs::Res<GameConfig> config) {
    float g = config->gravity;
}

// Zapis w systemie
void my_system(ecs::ResMut<GameConfig> config) {
    config->max_enemies = 200;
}
```

Wbudowane zasoby: `Time`, `CommandQueue`, `Window`, `Events<E>`, `State<S>`, `NextState<S>`.

---

## Eventy

Eventy to dane przesyłane między systemami. Wymagają markera `using Type = Event`:

```cpp
struct DamageEvent {
    using T = ecs::Event;
    ecs::Entity target;
    i32 amount;
};
```

### Rejestracja

```cpp
app.add_event<DamageEvent>();
```

To tworzy zasób `Events<DamageEvent>` i rejestruje system aktualizacji w etapie `First`.

### Wysyłanie

```cpp
void attack_system(ecs::EventWriter<DamageEvent> writer) {
    writer.send(DamageEvent{target, 25});
}
```

### Odbiór

```cpp
void damage_system(ecs::EventReader<DamageEvent> reader) {
    reader.for_each([](const DamageEvent &e) {
        // obsługa
    });
}
```

### Cykl życia eventów

Eventy są double-buffered. Co ramkę (w etapie `First`):
- Bufor `prev` jest czyszczony
- Bufory `current` i `prev` zamieniają się miejscami

Oznacza to że event żyje przez **2 klatki** (bieżącą i następną). Każdy `EventReader` ma niezależny kursor — może być wielu niezależnych czytelników.

---

## Harmonogram (Schedule)

Pętla główna wykonuje etapy w stałej kolejności:

```
PreStartup → Startup → PostStartup     (jednorazowo przy starcie)

Każda klatka:
First → PreUpdate → Update → PostUpdate → ExtractRender → Render → PostRender → Last
```

Po każdym etapie wykonywany jest `apply_commands()`.

Dostępne etykiety (`ScheduleLabel`):

| Etykieta | Przeznaczenie |
|---|---|
| `PreStartup` | Przed startem (jednorazowo) |
| `Startup` | Inicjalizacja (jednorazowo) |
| `PostStartup` | Po inicjalizacji (jednorazowo) |
| `First` | Początek klatki (aktualizacja eventów, input) |
| `PreUpdate` | Przed główną logiką (tranzycje stanów) |
| `Update` | Główna logika gry |
| `PostUpdate` | Po głównej logice |
| `ExtractRender` | Ekstrakcja danych do renderingu |
| `Render` | Renderowanie |
| `PostRender` | Po renderowaniu |
| `Last` | Koniec klatki |
| `FixedUpdate` | Stały krok czasowy |

Dodawanie systemów:

```cpp
app.add_systems(ScheduleLabel::Update, my_system);
app.add_systems(ScheduleLabel::Startup, setup);
```

---

## Kolejność i grupowanie systemów

### group — pojedynczy system z opcjami

```cpp
app.add_systems(ScheduleLabel::Update,
    group(my_system).after(other_system).before(third_system));
```

### group — wiele systemów

```cpp
app.add_systems(ScheduleLabel::Update,
    group(system_a, system_b, system_c));
```

### chain — sekwencyjna kolejność

```cpp
// system_a → system_b → system_c (gwarantowana kolejność)
app.add_systems(ScheduleLabel::Update,
    group(system_a, system_b, system_c).chain());

// Alternatywnie:
app.add_systems(ScheduleLabel::Update,
    chain(system_a, system_b, system_c));
```

### Zależności jawne

```cpp
app.add_systems(ScheduleLabel::Update,
    group(physics).after(input_handler).before(render_prep));
```

Systemy z zależnościami są sortowane topologicznie (algorytm Kahna). Cykle powodują wyjątek.

---

## Warunki uruchamiania (run_if)

System może mieć warunek — jeśli zwróci `false`, system się nie wykona:

```cpp
// Lambda z parametrami systemowymi
app.add_systems(ScheduleLabel::Update,
    group(debug_draw).run_if([](ecs::Res<ecs::Input> input) {
        return input->key_down(KEY_F3);
    }));

// Jednorazowe wykonanie
app.add_systems(ScheduleLabel::Update,
    group(init_once).run_if(ecs::run_once()));

// Warunek stanu
app.add_systems(ScheduleLabel::Update,
    group(gameplay_logic).run_if(ecs::in_state(GameState::Playing)));

// Warunek na grupę
app.add_systems(ScheduleLabel::Update,
    group(sys_a, sys_b, sys_c).run_if(some_condition));
```

Warunek to funkcja o takiej samej sygnaturze jak system (może przyjmować `Res`, `ResMut` itd.), zwracająca `bool`.

---

## Sety systemów

Sety pozwalają grupować systemy logicznie i nakładać wspólne zależności/warunki:

```cpp
// Definicja setów jako enum
enum class GameSets { Input, Logic, Render };

// Przypisanie do setu
app.add_systems(GameSets::Input, group(handle_input));

app.add_systems(GameSets::Logic, group(update_physics, update_ai));

// Konfiguracja kolejności setów
app.configure_set(ScheduleLabel::Update,
    ecs::configure(GameSets::Input, GameSets::Logic, GameSets::Render).chain());

// Warunek na cały set
app.configure_set(ScheduleLabel::Update,
    ecs::configure(GameSets::Logic).run_if(ecs::in_state(GameState::Playing)));
```

Set użyty przez `add_systems(ecs::set(...), ...)` musi być wcześniej lub później
zarejestrowany przez `configure_set(...)`. Jeśli nie będzie skonfigurowany,
scheduler zgłosi wyjątek przy pierwszym uruchomieniu.

Sety mogą też być strukturami z markerem `using Type = Set`:

```cpp
struct PhysicsSet { using T = ecs::Set; };
// Użycie: app.add_systems(PhysicsSet{}, group(...))
```

---

## Równoległość

ECS automatycznie uruchamia kompatybilne systemy równolegle (przez bibliotekę Taskflow).

### Analiza dostępu

Każdy system deklaruje co czyta i co pisze (przez `SystemAccess`):
- `Query<Position>` → odczyt komponentu Position
- `Query<Mut<Position>>` → zapis komponentu Position
- `Res<T>` → odczyt zasobu T
- `ResMut<T>` → zapis zasobu T
- `World &` → ekskluzywny dostęp (blokuje równoległość)
- `NonSendMarker` → wymusza main thread

### Kiedy systemy mogą działać równolegle

Dwa systemy **nie kolidują** jeśli:
- Żaden nie jest ekskluzywny
- Nie piszą do tego samego komponentu/zasobu jednocześnie
- Jeden nie pisze tam, gdzie drugi czyta

### Batching

Po sortowaniu topologicznym, systemy bez kolizji są grupowane w batche i uruchamiane równolegle w ramach jednego batcha. Kolejne batche wykonują się sekwencyjnie.

### Wyłączanie

```cpp
world.set_multithreaded(false);  // Sekwencyjne wykonanie

// Lub przez system na starcie:
app.add_systems(ScheduleLabel::Startup,
    ecs::system_multithreading(ecs::TaskPoolOptions{.multithreaded = false}));
```

---

## Stany (State)

Maszyna stanów pozwala na warunkowe uruchamianie systemów i reagowanie na tranzycje:

```cpp
enum class GameState { Menu, Playing, Paused };

app.init_state(GameState::Menu);
```

### Tranzycje

```cpp
void pause_system(ecs::ResMut<ecs::NextState<GameState>> next, ecs::Res<ecs::Input> input) {
    if (input->key_pressed(KEY_ESCAPE)) {
        next->set(GameState::Paused);
    }
}
```

Tranzycje są przetwarzane w `PreUpdate`. Kolejność: `OnExit(stary)` → zmiana stanu → `OnTransition` → `OnEnter(nowy)`.

### Hooki stanów

```cpp
app.add_systems(OnEnter(GameState::Playing{}), start_game);
app.add_systems(OnExit(GameState::Playing{}), cleanup_game);
app.add_systems(OnTransition(GameState{}), on_any_state_change);
```

### Warunki na stan

```cpp
// Uruchom tylko gdy stan == Playing
group(gameplay).run_if(ecs::in_state(GameState::Playing))

// Uruchom gdy stan się zmienił
group(ui_update).run_if(ecs::state_changed<GameState>())
```

---

## Detekcja zmian (Change Detection)

Każdy komponent ma przypisane ticki:

```cpp
struct ComponentTicks {
    Tick added;    // Kiedy komponent został dodany
    Tick changed;  // Kiedy komponent został ostatnio zmodyfikowany
};
```

World inkrementuje globalny `change_tick_` co klatkę.

### Mut<T> — automatyczne śledzenie

`Mut<T>` oznacza komponent jako zmieniony przy pierwszym użyciu operatora `->`, `*` lub `get()`:

```cpp
void system(Query<Mut<Position>> q) {
    q.each([](Mut<Position> pos) {
        pos->x += 1;  // Automatycznie oznacza Position jako zmieniony
    });
}
```

Aby ominąć detekcję zmian:

```cpp
pos.bypass_change_detection().x += 1;  // Nie oznacza jako zmieniony
```

### Filtry detekcji

```cpp
// Nowo dodane komponenty
Query<Position, Added<Position>> q;

// Zmodyfikowane komponenty
Query<Mut<Health>, Changed<Health>> q;
```

---

## Bundle

Bundle to zestaw komponentów wstawianych razem. Wymaga markera `using Type = Bundle`:

```cpp
struct CharacterBundle {
    using T = ecs::Bundle;
    Position pos;
    Velocity vel;
    Health hp;
};

// Użycie:
cmd.spawn(CharacterBundle{
    .pos = {0, 0},
    .vel = {1, 0},
    .hp = {100}
});
```

Bundle jest rozkładany na pojedyncze komponenty przy wstawianiu. Można też mieszać Bundle z luźnymi komponentami:

```cpp
cmd.spawn(CharacterBundle{...}, EnemyTag{});
```

---

## Pluginy

Plugin to funkcja lub klasa konfigurująca App:

```cpp
// Funkcja
void physics_plugin(ecs::App &app) {
    app.insert_resource(PhysicsConfig{});
    app.add_systems(ecs::ScheduleLabel::Update, physics_step);
}

app.add_plugin(physics_plugin);

// Lambda
app.add_plugin([](ecs::App &app) {
    app.add_event<CollisionEvent>();
});

// Klasa (implementuje IPlugin)
class MyPlugin : public ecs::IPlugin {
    void build(ecs::App &app) override { ... }
};
MyPlugin plugin;
app.add_plugin(plugin);
```

### Wbudowane pluginy

```cpp
app.add_plugin(ecs::window_plugin({800, 600, "Tytuł", 60}))  // Okno raylib
   .add_plugin(ecs::time_plugin);                              // Zasób Time
```

---

## App

`App` to główny punkt wejścia — łączy World, Schedule i pluginy:

```cpp
ecs::App app;

app
    // Pluginy
    .add_plugin(ecs::window_plugin({800, 600, "Gra", 60}))

    // Zasoby
    .insert_resource(GameConfig{})

    // Stany
    .init_state(GameState::Menu)

    // Eventy
    .add_event<DamageEvent>()

    // Systemy
    .add_systems(ecs::ScheduleLabel::Startup, setup)
    .add_systems(ecs::ScheduleLabel::Update, group(input, logic, render).chain())

    // Konfiguracja setów
    .configure_set(ecs::ScheduleLabel::Update,
        ecs::configure(GameSets::Input, GameSets::Logic).chain())

    // Start
    .run();
```

`App::run()`:
1. Inicjalizuje okno raylib (z zasobu `Window`)
2. Wykonuje `startup()` — `PreStartup`, `Startup`, `PostStartup`
3. W pętli `while (!WindowShouldClose())`:
   - Aktualizuje `Time`
   - Inkrementuje `change_tick_`
   - Wykonuje kolejno: `First` → `PreUpdate` → `Update` → `PostUpdate` → `ExtractRender` → `Render` → `PostRender` → `Last`
   - Po każdym etapie wykonuje `apply_commands()`
