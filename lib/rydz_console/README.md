Rydz Console 
===============

Wbudowana konsola deweloperska (In-Game Console) dla silnika `rydz_ecs`. Oparta na maszynie wirtualnej **Lua** (przez bibliotekę `sol2`), pozwala na wywoływanie systemów ECS i modyfikowanie stanu gry w czasie rzeczywistym.

Główne funkcje
----------------

-   **Pełna integracja z ECS:** Komendy to w rzeczywistości pełnoprawne systemy `rydz_ecs`.

-   **Przekazywanie argumentów:** Bezpieczne typowanie z Lua do C++ (z wykorzystaniem `sol::optional` do łapania błędów wprowadzania).


-   **Historia komend:** Przewijanie strzałkami góra/dół (jak w terminalu Linux/Windows).

-   **Scrollowanie logów:** Obsługa rolki myszy oraz klawiszy Page Up / Page Down.

-   **Ochrona przed wyciekiem wejścia:** Klawisz tyldy (``` / `~`) nie wpisuje się do pola tekstowego podczas otwierania konsoli.

* * * * *

Inicjalizacja
-----------------------------------------

Aby konsola działała, musisz dodać jej pluginy do głównej pętli `App` w swoim pliku `main.cpp`.

C++

```
#include "rydz_console/scripting.hpp"
#include "rydz_console/console.hpp"
#include "rydz_console/command_api.hpp"

int main() {
    ecs::App app;
    app.add_plugin(window_plugin({800, 600, "Moja Gra", 60}))
       // ... inne pluginy ...

       // 1. Dodaj pluginy konsoli i skryptów
       .add_plugin(engine::scripting_plugin)
       .add_plugin(engine::console_plugin)

       // 2. Dodaj system rejestrujący Twoje komendy na starcie gry
       .add_systems(ecs::Startup, bind_lua_commands)

       .run();
}

```

* * * * *

Obsługa w grze
--------------------

-   `[ ~ ]` / `[` ]` / `[ F1 ]` -- Otwieranie / zamykanie konsoli. `F1` jest zapasowym skrótem, jeśli środowisko pulpitu przechwytuje klawisz tyldy.

-   `[ Enter ]` -- Wykonanie komendy. (Uwaga: Konsola automatycznie dodaje nawiasy `()` jeśli wpiszesz samo słowo, np. `kill_all` nie dotyczy to funkcji  którym przekazujemy parametry wtedy nawiasy są konieczne np. `spawn(5)`) .

-   `[ Strzałka w górę / w dół ]` -- Przewijanie historii wpisanych komend.

-   `[ Rolka Myszki / Page Up / Page Down ]` -- Przewijanie historii logów i błędów.

* * * * *

Rejestrowanie komend (API)
---------------------------------

Moduł udostępnia uniwersalne API w pliku `command_api.hpp`. Funkcje bindujące przyjmują referencję do Świata (`World&`) i automatycznie wstrzykują zależności (`Cmd`, `ResMut`, `Query`) do Twoich systemów.

Typ A: Systemy bez parametrów (np. `kill_all`)
------------------------------------------------

Idealne do wywoływania statycznych zdarzeń, które nie potrzebują dodatkowych danych od gracza.

**Krok 1: Napisz czysty system ECS**

C++

```
void kill_all_enemies(Query<Mut<Health>, With<EnemyTag>> query) {
    for (auto [hp] : query.iter()) {
        hp->value = 0;
    }
}

```

**Krok 2: Zarejestruj używając `bind_system`**

C++

```
void bind_lua_commands(ecs::World& world) {
    engine::ConsoleAPI::bind_system(world, "kill_all", kill_all_enemies);
}

```

Typ B: Komendy z parametrami z Lua (np. `spawn(10)`)
----------------------------------------------------

Kiedy chcesz przekazać zmienną wpisaną przez gracza w konsoli do wnętrza systemu ECS.

**Krok 1: Napisz funkcję z pożądanym argumentem**

C++

```
void spawn_enemies(Cmd cmd, ResMut<EnemyCount> count, int amount) {
    for (int i = 0; i < amount; ++i) {
        cmd.spawn(EnemyBundle{...});
        count->count++;
    }
}

```

**Krok 2: Zarejestruj używając `BindCommand<Typ>::to`** Musisz zadeklarować w nawiasach ostrych `< >`, jakich typów z Lua oczekujesz. System automatycznie sprawdzi poprawność wpisanych danych. Zwróć uwagę na użycie `std::move(cmd)`!

C++

```
void bind_lua_commands(ecs::World& world) {
    
    // Oczekujemy jednego inta <int>
    engine::BindCommand<int>::to(world, "spawn", [](int amount) {
        
        // Zwracamy lambdę, która jest poprawnym systemem ECS
        return [amount](Cmd cmd, ResMut<EnemyCount> count) {
            spawn_enemies(std::move(cmd), count, amount);
        };
        
    });
}
```
