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

== Diagram komponentów - rydz_graphics

```pintora
componentDiagram

package "Komponenty sceny" {
  [Transform3D] as Transform3D
  [GlobalTransform] as GlobalTransform
  [Visibility] as Visibility
  [ComputedVisibility] as ComputedVisibility
  [ViewVisibility] as ViewVisibility
  [MeshBounds] as MeshBounds
  [Model3d] as Model3d
  [Mesh3d] as Mesh3d
  [Material3d] as Material3d
  [RenderConfig] as RenderConfig
  [MeshLodGroup] as MeshLodGroup
  [Texture] as Texture
}

package "Kamera i światło" {
  [Camera3DComponent] as Camera3D
  [ActiveCamera] as ActiveCamera
  [Skybox] as Skybox
  [DirectionalLight] as DirectionalLight
  [PointLight] as PointLight
  [AmbientLight] as AmbientLight
}

package "Zasoby" {
  [Assets<rl::Model>] as ModelAssets
  [Assets<rl::Mesh>] as MeshAssets
  [Assets<rl::Texture2D>] as TextureAssets
  [Assets<rl::Sound>] as SoundAssets
  [RenderBatches] as RenderBatches
  [ClearColor] as ClearColor
  [AssetServer] as AssetServer
  [PendingModelLoads] as PendingModelLoads
  [LodConfig] as LodConfig
  [LodHistory] as LodHistory
}

package "Systemy grafiki" {
  [RenderPlugin] as RenderPlugin
  [propagate_transforms] as Propagate
  [compute_visibility] as ComputeVisibility
  [compute_mesh_bounds_system] as ComputeBounds
  [frustum_cull_system] as FrustumCull
  [auto_generate_lods_system] as AutoLod
  [build_render_batches_system] as BuildBatches
  [apply_materials_system] as ApplyMaterials
  [render_system] as RenderSystem
  [process_pending_model_loads] as ProcessLoads
}

package "Zewnętrzne" {
  [Raylib] as Raylib
  [Taskflow] as Taskflow
  [meshoptimizer] as Meshopt
}

RenderPlugin --> AssetServer
RenderPlugin --> ModelAssets
RenderPlugin --> MeshAssets
RenderPlugin --> TextureAssets
RenderPlugin --> SoundAssets
RenderPlugin --> RenderBatches
RenderPlugin --> ClearColor
RenderPlugin --> LodConfig
RenderPlugin --> LodHistory
RenderPlugin --> PendingModelLoads
RenderPlugin --> BuildBatches
RenderPlugin --> RenderSystem
RenderPlugin --> ApplyMaterials
RenderPlugin --> AutoLod
RenderPlugin --> FrustumCull
RenderPlugin --> ComputeBounds
RenderPlugin --> ComputeVisibility
RenderPlugin --> Propagate
RenderPlugin --> ProcessLoads

Transform3D --> GlobalTransform
Visibility --> ComputedVisibility
ComputedVisibility --> ViewVisibility
MeshBounds --> ViewVisibility
Camera3D --> ViewVisibility
ActiveCamera --> ViewVisibility
GlobalTransform --> ViewVisibility

Model3d --> MeshBounds
Model3d --> ModelAssets
Mesh3d --> MeshAssets
Texture --> TextureAssets
Model3d --> RenderBatches
MeshLodGroup --> RenderBatches
Material3d --> RenderBatches
RenderConfig --> RenderBatches
ViewVisibility --> RenderBatches
GlobalTransform --> RenderBatches

AssetServer --> ModelAssets
AssetServer --> TextureAssets
AssetServer --> SoundAssets
ProcessLoads --> PendingModelLoads
ProcessLoads --> ModelAssets

BuildBatches --> RenderBatches
BuildBatches --> ModelAssets

ApplyMaterials --> ModelAssets
ApplyMaterials --> TextureAssets

AutoLod --> ModelAssets
AutoLod --> LodConfig
AutoLod --> LodHistory
AutoLod --> MeshLodGroup
AutoLod --> Meshopt

ComputeVisibility --> Visibility
ComputeVisibility --> ComputedVisibility
ComputeBounds --> Model3d
ComputeBounds --> MeshBounds
ComputeBounds --> ModelAssets
FrustumCull --> MeshBounds
FrustumCull --> ViewVisibility
FrustumCull --> GlobalTransform
FrustumCull --> ComputedVisibility
FrustumCull --> Camera3D
FrustumCull --> Transform3D
FrustumCull --> ActiveCamera
FrustumCull --> Taskflow

RenderSystem --> RenderBatches
RenderSystem --> ModelAssets
RenderSystem --> TextureAssets
RenderSystem --> ClearColor
RenderSystem --> Camera3D
RenderSystem --> ActiveCamera
RenderSystem --> Transform3D
RenderSystem --> Skybox
RenderSystem --> DirectionalLight
RenderSystem --> PointLight
RenderSystem --> Texture
RenderSystem --> Raylib
```

== Diagram komponentów - AudioPlugin (przykład inspirowany Bevy)

```pintora
componentDiagram

package "Komponenty audio" {
  [AudioSink] as AudioSink
  [SpatialAudioSink] as SpatialAudioSink
  [PlaybackSettings] as PlaybackSettings
  [AudioListener] as AudioListener
}

package "Zasoby" {
  [Audio] as Audio
  [AudioChannel<T>] as AudioChannel
  [Assets<AudioSource>] as AudioAssets
  [AudioOutput] as AudioOutput
  [AudioQueue] as AudioQueue
}

package "Systemy audio" {
  [AudioPlugin] as AudioPlugin
  [play_audio_system] as PlayAudio
  [update_sinks_system] as UpdateSinks
  [spatial_audio_system] as SpatialAudio
  [cleanup_finished_system] as CleanupAudio
}

package "Zewnętrzne" {
  [Backend audio (cpal/rodio)] as AudioBackend
}

AudioPlugin --> Audio
AudioPlugin --> AudioChannel
AudioPlugin --> AudioAssets
AudioPlugin --> AudioOutput
AudioPlugin --> AudioQueue
AudioPlugin --> PlayAudio
AudioPlugin --> UpdateSinks
AudioPlugin --> SpatialAudio
AudioPlugin --> CleanupAudio

Audio --> AudioAssets
Audio --> AudioChannel
Audio --> AudioQueue
AudioChannel --> AudioQueue

PlayAudio --> AudioQueue
UpdateSinks --> AudioSink
UpdateSinks --> PlaybackSettings
SpatialAudio --> SpatialAudioSink
SpatialAudio --> AudioListener
CleanupAudio --> AudioSink

AudioOutput --> AudioBackend
AudioQueue --> AudioBackend
```

== Diagram komponentów - PhysicsPlugin (przykład inspirowany Bevy + Rapier)

```pintora
componentDiagram

package "Komponenty fizyki" {
  [RigidBody] as RigidBody
  [Collider] as Collider
  [Velocity] as Velocity
  [ExternalForce] as ExternalForce
  [GravityScale] as GravityScale
  [Damping] as Damping
  [Sleeping] as Sleeping
  [LockedAxes] as LockedAxes
  [Sensor] as Sensor
  [CollisionGroups] as CollisionGroups
}

package "Zdarzenia" {
  [CollisionEvent] as CollisionEvent
  [ContactForceEvent] as ContactForceEvent
}

package "Zasoby" {
  [RapierConfiguration] as PhysicsConfig
  [IntegrationParameters] as IntegrationParams
  [PhysicsPipeline] as PhysicsPipeline
  [BroadPhase] as BroadPhase
  [NarrowPhase] as NarrowPhase
  [QueryPipeline] as QueryPipeline
}

package "Systemy fizyki" {
  [PhysicsPlugin] as PhysicsPlugin
  [step_physics_system] as StepPhysics
  [sync_from_physics] as SyncFromPhysics
  [sync_to_physics] as SyncToPhysics
  [emit_collision_events] as EmitEvents
}

package "ECS" {
  [Transform] as Transform
  [GlobalTransform] as GlobalTransform
}

package "Zewnętrzne" {
  [Rapier] as Rapier
}

PhysicsPlugin --> PhysicsConfig
PhysicsPlugin --> IntegrationParams
PhysicsPlugin --> PhysicsPipeline
PhysicsPlugin --> BroadPhase
PhysicsPlugin --> NarrowPhase
PhysicsPlugin --> QueryPipeline
PhysicsPlugin --> StepPhysics
PhysicsPlugin --> SyncFromPhysics
PhysicsPlugin --> SyncToPhysics
PhysicsPlugin --> EmitEvents

StepPhysics --> RigidBody
StepPhysics --> Collider
StepPhysics --> Velocity
StepPhysics --> ExternalForce
StepPhysics --> Rapier
StepPhysics --> PhysicsPipeline
StepPhysics --> BroadPhase
StepPhysics --> NarrowPhase
StepPhysics --> IntegrationParams
StepPhysics --> PhysicsConfig

SyncToPhysics --> Transform
SyncToPhysics --> RigidBody
SyncToPhysics --> Velocity

SyncFromPhysics --> Transform
SyncFromPhysics --> GlobalTransform
SyncFromPhysics --> RigidBody
SyncFromPhysics --> Velocity

EmitEvents --> CollisionEvent
EmitEvents --> ContactForceEvent
EmitEvents --> NarrowPhase
EmitEvents --> RigidBody
EmitEvents --> Collider

Collider --> CollisionGroups
RigidBody --> Sleeping
RigidBody --> LockedAxes
RigidBody --> GravityScale
RigidBody --> Damping
Sensor --> Collider
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
