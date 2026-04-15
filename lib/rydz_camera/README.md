# rydz_camera

Isometric camera system for top-down games and RPGs. Single header, ~70 lines.

## Quick Start

```cpp
#include "rydz_camera/mod.hpp"

// In your startup system:
setup_isometric_camera(cmd, target, offset);

// In your app plugin:
app.add_systems(Update, isometric_camera_system);
```

## API

#### `IsometricCamera` Component
```cpp
Vec3 target;              // Follow target position
Vec3 offset;              // Camera offset from target (e.g., (10, 10, 10))
f32 follow_speed = 5.0f;  // Smooth follow lerp speed
bool smooth_follow = true; // Enable/disable smooth following
```

#### `setup_isometric_camera()` Helper
```cpp
void setup_isometric_camera(
  Cmd cmd,
  const Vec3& target = Vec3::sZero(),
  const Vec3& offset = Vec3(10.0f, 10.0f, 10.0f),
  f32 ortho_height = 20.0f,
  f32 follow_speed = 5.0f);
```

#### `isometric_camera_system()` 
Add to Update schedule to drive camera movement.

## Example

```cpp
inline void setup_scene(Cmd cmd, ResMut<Assets<rl::Mesh>> meshes, NonSendMarker) {
  // Spawn player entity
  auto cube = meshes->add(mesh::cube(1.0f, 1.0f, 1.0f));
  cmd.spawn(Mesh3d{cube},
            MeshMaterial3d<>{StandardMaterial::from_color(WHITE)},
            Transform::from_xyz(0, 0.5f, 0), Player{});
  
  // Spawn isometric camera following origin with offset
  setup_isometric_camera(cmd, Vec3::sZero(), Vec3(10, 10, 10), 20.0f);
}

inline void update_camera(Query<Mut<IsometricCamera>> cam_query, 
                          Query<Transform, Player> player_query) {
  for (auto [pt, _] : player_query.iter()) {
    for (auto [cam] : cam_query.iter()) {
      cam->target = pt->translation;  // Follow player
    }
    break;
  }
}
```

## Notes

- Original `Camera3DComponent` in rydz_graphics/camera3d.hpp is untouched
- System uses orthographic projection (configurable height)
- Smooth follow uses Lerp with framerate-independent delta time
