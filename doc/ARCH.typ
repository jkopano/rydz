#import "@preview/pintorita:0.1.4"

#set page(height: auto, width: auto, fill: white, margin: 2em)
#set text(size: 11pt)

#show raw.where(lang: "pintora"): it => pintorita.render(it.text)

= rydz_ecs - Architektura

== Diagram komponentów

```pintora
componentDiagram

package "Rdzeń ECS" {
  [Entity] as Entity
  [Storage] as Storage
  [World] as World
  [Resource] as Resource
  [Ticks] as Ticks
}

package "System zapytań" {
  [Query] as Query
  [Filter] as Filter
  [Access] as Access
}

package "Wykonanie" {
  [System] as System
  [Schedule] as Schedule
  [Condition] as Condition
  [Command] as Command
}

package "Aplikacja" {
  [App] as App
  [Plugin] as Plugin
  [State] as State
  [Event] as Event
}

package "Domena" {
  [Input] as Input
  [Asset] as Asset
  [Hierarchy] as Hierarchy
  [Time and Name] as CoreTypes
}

package "Zewnętrzne" {
  [Raylib] as Raylib
  [Taskflow] as Taskflow
}

World --> Entity
World --> Storage
World --> Resource
Storage --> Ticks
Storage --> Entity

Query --> Storage
Query --> Filter
Query --> Access
Query --> World
Filter --> Ticks

System --> Query
System --> Access
System --> World
Command --> World
Command --> System

Schedule --> System
Schedule --> Condition
Schedule --> Access
Schedule --> Taskflow

App --> Schedule
App --> World
App --> Plugin
App --> Command
App --> Event
App --> State

State --> Schedule
Event --> System

Input --> App
Input --> Raylib
Asset --> World
Asset --> System
Hierarchy --> Entity
Hierarchy --> World
CoreTypes --> World
```

== Diagram sekwencji - aktualizacja App

```pintora
sequenceDiagram
  participant App
  participant Schedules
  participant Schedule
  participant System
  participant World
  participant CommandQueue

  App ->> App: uruchomienie PreStartup, Startup, PostStartup

  loop main loop
    App ->> App: update
    App ->> World: aktualizacja zasobu Time
    App ->> World: inkrementacja change tick

    loop dla każdego ScheduleLabel
      App ->> Schedules: run label, world
      Schedules ->> Schedule: run world
      Schedule ->> Schedule: budowa grafu + grupowanie systemów

      loop dla każdej grupy przez Taskflow
        Schedule ->> System: run world
        System ->> World: zapytanie o komponenty
        World -->> System: wyniki
        System ->> CommandQueue: push cmd
      end

      Schedule -->> Schedules: zakończono
      App ->> CommandQueue: apply world
      CommandQueue ->> World: spawn, despawn, insert
    end
  end
```

== Diagram komponentów - Storage

```pintora
componentDiagram

[IStorage - interfejs] as IStorage
[SparseSetStorage] as Sparse
[HashMapStorage] as HashMap
[storage_trait] as Trait
[Entity - index, generation] as Entity
[ComponentTicks] as Ticks
[Dane komponentu - gęsty wektor] as Data

Sparse --> IStorage
HashMap --> IStorage
Trait --> Sparse
Trait --> HashMap
Sparse --> Entity
Sparse --> Ticks
Sparse --> Data
HashMap --> Entity
HashMap --> Ticks
```

== Diagram aktywności - Wstrzykiwanie zależności systemu

```pintora
activityDiagram

start
:Rejestracja funkcji systemu;
:Ekstrakcja typów parametrów przez function traits;

if (Zawiera Query?) then (tak)
  :Rozwiąż storage z World;
  :Zapisz dostęp odczyt/zapis komponentów;
else (nie)
endif

if (Zawiera Res lub ResMut?) then (tak)
  :Wyszukaj zasób w World;
  :Zapisz dostęp odczyt/zapis zasobu;
else (nie)
endif

if (Zawiera Cmd?) then (tak)
  :Wstrzyknij referencję do CommandQueue;
else (nie)
endif

if (Zawiera World?) then (tak)
  :Oznacz jako ekskluzywny - brak równoległości;
else (nie)
endif

:Zbuduj SystemAccess;
:Zarejestruj w Schedule;
end
```
