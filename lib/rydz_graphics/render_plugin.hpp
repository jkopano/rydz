#pragma once
#include "camera3d.hpp"
#include "frustum.hpp"
#include "light.hpp"
#include "lod.hpp"
#include "material3d.hpp"
#include "mesh3d.hpp"
#include "render_batches.hpp"
#include "render_config.hpp"
#include "rydz_ecs/app.hpp"
#include "rydz_ecs/asset.hpp"
#include "rydz_ecs/transform.hpp"
#include "rydz_ecs/types.hpp"
#include "rydz_ecs/visibility.hpp"
#include "skybox.hpp"
#include <absl/container/flat_hash_map.h>
#include <array>
#include <cstring>
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <vector>

namespace ecs {

struct ClearColor {
  Color color = {30, 30, 40, 255};
};

inline void apply_materials_system(World &world) {
  auto *mesh_storage = world.get_storage<Mesh3d>();
  auto *mat_storage = world.get_storage<Material3d>();
  auto *model_assets = world.get_resource<Assets<Model>>();
  auto *tex_assets = world.get_resource<Assets<Texture2D>>();
  if (!mesh_storage || !mat_storage || !model_assets)
    return;

  auto entities = mat_storage->entity_indices();
  for (auto e : entities) {
    auto *mesh_comp = mesh_storage->get(e);
    auto *mat_comp = mat_storage->get(e);
    if (!mesh_comp || !mat_comp)
      continue;

    Model *model = model_assets->get(mesh_comp->model);
    if (!model)
      continue;

    mat_comp->material.apply(*model, tex_assets);
  }
}

inline void build_render_batches_system(
    Query<const Mesh3d, const GlobalTransform, Opt<const Material3d>,
          Opt<const RenderConfig>, Opt<const ViewVisibility>,
          Opt<const MeshLodGroup>>
        query,
    ResMut<Assets<Model>> model_assets, ResMut<RenderBatches> batches,
    Res<LodConfig> lod_config, ResMut<LodHistory> lod_history, NonSendMarker) {
  batches->clear();
  absl::flat_hash_map<RenderBatchKey, size_t> batch_index;

  query.each([&](const Mesh3d *mesh3d, const GlobalTransform *global,
                 const Material3d *mat, const RenderConfig *rc,
                 const ViewVisibility *vv, const MeshLodGroup *lod_group) {
    if (!mesh3d || !mesh3d->model.is_valid())
      return;
    if (vv && !vv->visible)
      return;
    Handle<Model> model_handle = mesh3d->model;
    if (lod_group && lod_group->level_count > 1) {
      model_handle = lod_group->levels[0];
    }

    Model *model = model_assets->get(model_handle);
    if (!model || model->meshCount <= 0 || model->meshes == nullptr)
      return;

    if (model->meshes[0].vaoId == 0) {
      for (int i = 0; i < model->meshCount; ++i) {
        UploadMesh(&model->meshes[i], false);
      }
    }

    MaterialKey mat_key{};
    if (mat) {
      const auto &m = mat->material;
      mat_key.base_color = m.base_color;
      mat_key.texture_id = m.texture.id;
      mat_key.normal_id = m.normal_map.id;
      mat_key.metallic = m.metallic;
      mat_key.roughness = m.roughness;
      mat_key.shader = m.shader();
    }

    RenderConfig rc_value{};
    bool has_rc = false;
    if (rc) {
      rc_value = *rc;
      has_rc = true;
    }

    for (int i = 0; i < model->meshCount; ++i) {
      int mat_index = 0;
      if (model->meshMaterial && model->meshMaterial[i] >= 0 &&
          model->meshMaterial[i] < model->materialCount) {
        mat_index = model->meshMaterial[i];
      }

      RenderBatchKey key{};
      key.model = model_handle;
      key.mesh_index = i;
      key.material_index = mat_index;
      key.material = mat_key;
      key.rc = rc_value;
      key.has_rc = has_rc;

      auto it = batch_index.find(key);
      if (it == batch_index.end()) {
        size_t idx = batches->batches.size();
        RenderBatch batch{};
        batch.key = key;
        batch.transforms.push_back(global->matrix);
        batches->batches.push_back(std::move(batch));
        batch_index.emplace(batches->batches.back().key, idx);
      } else {
        batches->batches[it->second].transforms.push_back(global->matrix);
      }
    }
  });
}

inline void
render_system(Res<ClearColor> clear_color, Res<Assets<Model>> model_assets,
              Res<Assets<Texture2D>> tex_assets, Res<RenderBatches> batches,
              Query<const Camera3DComponent, const ActiveCamera,
                    const Transform3D, Opt<const Skybox>>
                  cam_query,
              Query<const DirectionalLight> dir_query,
              Query<const PointLight, const GlobalTransform> point_query,
              NonSendMarker) {
  Color bg = clear_color->color;

  Camera3D cam = {};
  cam.position = {10, 10, 10};
  cam.target = {0, 0, 0};
  cam.up = {0, 1, 0};
  cam.fovy = 45.0f;
  cam.projection = CAMERA_PERSPECTIVE;

  const Skybox *active_skybox = nullptr;

  cam_query.each([&](const Camera3DComponent *cam_comp, const ActiveCamera *,
                     const Transform3D *cam_tx, const Skybox *sky) {
    if (cam_comp && cam_tx) {
      cam = build_camera(cam_tx->translation, cam_tx->forward(), cam_tx->up(),
                         *cam_comp);
      active_skybox = sky;
    }
  });

  auto fallback_material = []() -> Material & {
    static Material fallback = {};
    static bool init = false;
    if (!init) {
      fallback.shader.id = rlGetShaderIdDefault();
      fallback.shader.locs = rlGetShaderLocsDefault();
      fallback.maps = (MaterialMap *)RL_CALLOC(12, sizeof(MaterialMap));
      fallback.maps[MATERIAL_MAP_DIFFUSE].texture.id = rlGetTextureIdDefault();
      fallback.maps[MATERIAL_MAP_DIFFUSE].texture.width = 1;
      fallback.maps[MATERIAL_MAP_DIFFUSE].texture.height = 1;
      fallback.maps[MATERIAL_MAP_DIFFUSE].texture.mipmaps = 1;
      fallback.maps[MATERIAL_MAP_DIFFUSE].texture.format =
          PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
      fallback.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
      init = true;
    }
    return fallback;
  };

  auto mul_color = [](Color a, Color b) -> Color {
    return Color{
        (unsigned char)((a.r * b.r) / 255),
        (unsigned char)((a.g * b.g) / 255),
        (unsigned char)((a.b * b.b) / 255),
        (unsigned char)((a.a * b.a) / 255),
    };
  };

  auto color_to_vec3 = [](Color c) -> Vector3 {
    return {c.r / 255.0f, c.g / 255.0f, c.b / 255.0f};
  };

  DirectionalLight dir_light{};
  bool has_dir = false;
  dir_query.each([&](const DirectionalLight *dir) {
    if (!dir || has_dir)
      return;
    dir_light = *dir;
    has_dir = true;
  });

  constexpr int kMaxPointLights = 16;
  std::array<Vector3, kMaxPointLights> point_positions{};
  std::array<Vector3, kMaxPointLights> point_colors{};
  std::array<float, kMaxPointLights> point_intensities{};
  std::array<float, kMaxPointLights> point_ranges{};
  usize point_count = 0;

  point_query.each([&](const PointLight *pl, const GlobalTransform *gt) {
    if (!pl || !gt || point_count >= kMaxPointLights)
      return;
    point_positions[point_count] = gt->translation();
    point_colors[point_count] = color_to_vec3(pl->color);
    point_intensities[point_count] = pl->intensity;
    point_ranges[point_count] = pl->range;
    ++point_count;
  });

  BeginDrawing();
  ClearBackground(bg);
  BeginMode3D(cam);

  if (active_skybox && active_skybox->loaded) {
    active_skybox->draw(cam);
  }

  for (const auto &batch : batches->batches) {
    const auto &key = batch.key;
    const Model *model = model_assets->get(key.model);
    if (!model || key.mesh_index < 0 || key.mesh_index >= model->meshCount)
      continue;

    const Mesh &mesh = model->meshes[key.mesh_index];
    const Material *src_mat = nullptr;
    if (model->materials && model->materialCount > 0 &&
        key.material_index >= 0 && key.material_index < model->materialCount) {
      src_mat = &model->materials[key.material_index];
    }
    if (!src_mat || !src_mat->maps) {
      src_mat = &fallback_material();
    }

    std::array<MaterialMap, 12> local_maps;
    Material draw_mat = *src_mat;
    memcpy(local_maps.data(), src_mat->maps, sizeof(local_maps));
    draw_mat.maps = local_maps.data();

    if (key.material.shader) {
      draw_mat.shader = *key.material.shader;
    } else if (draw_mat.shader.id == 0 || draw_mat.shader.locs == nullptr) {
      draw_mat.shader.id = rlGetShaderIdDefault();
      draw_mat.shader.locs = rlGetShaderLocsDefault();
    }

    draw_mat.maps[MATERIAL_MAP_DIFFUSE].color = mul_color(
        draw_mat.maps[MATERIAL_MAP_DIFFUSE].color, key.material.base_color);

    if (key.material.texture_id != UINT32_MAX) {
      if (auto *tex =
              tex_assets->get(Handle<Texture2D>{key.material.texture_id})) {
        draw_mat.maps[MATERIAL_MAP_DIFFUSE].texture = *tex;
      }
    }
    if (key.material.normal_id != UINT32_MAX) {
      if (auto *tex =
              tex_assets->get(Handle<Texture2D>{key.material.normal_id})) {
        draw_mat.maps[MATERIAL_MAP_NORMAL].texture = *tex;
      }
    }

    if (key.has_rc) {
      key.rc.apply();
    }

    auto set_shader_value = [](Shader &shader, const char *name,
                               const void *value, int type) {
      int loc = GetShaderLocation(shader, name);
      if (loc >= 0) {
        SetShaderValue(shader, loc, value, type);
      }
    };

    auto set_shader_value_v = [](Shader &shader, const char *name,
                                 const void *value, int type, int count) {
      int loc = GetShaderLocation(shader, name);
      if (loc >= 0) {
        SetShaderValueV(shader, loc, value, type, count);
      }
    };

    Shader &shader = draw_mat.shader;
    float metallic = key.material.metallic;
    float roughness = key.material.roughness;
    float normal_factor = 1.0f;
    float occlusion_factor = 1.0f;
    Vector3 emissive = {0.0f, 0.0f, 0.0f};
    Vector4 tint = {1.0f, 1.0f, 1.0f, 0.0f};
    Vector3 camera_pos = cam.position;

    int has_normal = 0;
    if (key.material.normal_id != UINT32_MAX) {
      if (tex_assets->get(Handle<Texture2D>{key.material.normal_id})) {
        has_normal = 1;
      }
    }

    int has_roughness_metallic = 0;
    int has_occlusion = 0;
    int has_emissive = 0;

    int has_directional = has_dir ? 1 : 0;
    Vector3 dir_color = color_to_vec3(dir_light.color);
    Vector3 dir_dir = Vector3Normalize(dir_light.direction);
    float dir_intensity = dir_light.intensity;

    usize point_count_i = point_count;
    int spot_count_i = 0;

    set_shader_value(shader, "u_metallic_factor", &metallic,
                     SHADER_UNIFORM_FLOAT);
    set_shader_value(shader, "u_roughness_factor", &roughness,
                     SHADER_UNIFORM_FLOAT);
    set_shader_value(shader, "u_normal_factor", &normal_factor,
                     SHADER_UNIFORM_FLOAT);
    set_shader_value(shader, "u_occlusion_factor", &occlusion_factor,
                     SHADER_UNIFORM_FLOAT);
    set_shader_value(shader, "u_emissive_factor", &emissive,
                     SHADER_UNIFORM_VEC3);
    set_shader_value(shader, "u_color", &tint, SHADER_UNIFORM_VEC4);
    set_shader_value(shader, "u_camera_pos", &camera_pos, SHADER_UNIFORM_VEC3);

    set_shader_value(shader, "u_has_roughness_metallic_texture",
                     &has_roughness_metallic, SHADER_UNIFORM_INT);
    set_shader_value(shader, "u_has_normal_texture", &has_normal,
                     SHADER_UNIFORM_INT);
    set_shader_value(shader, "u_has_occlusion_texture", &has_occlusion,
                     SHADER_UNIFORM_INT);
    set_shader_value(shader, "u_has_emissive_texture", &has_emissive,
                     SHADER_UNIFORM_INT);

    set_shader_value(shader, "u_has_directional", &has_directional,
                     SHADER_UNIFORM_INT);
    set_shader_value(shader, "u_dir_light_direction", &dir_dir,
                     SHADER_UNIFORM_VEC3);
    set_shader_value(shader, "u_dir_light_intensity", &dir_intensity,
                     SHADER_UNIFORM_FLOAT);
    set_shader_value(shader, "u_dir_light_color", &dir_color,
                     SHADER_UNIFORM_VEC3);

    set_shader_value(shader, "u_point_light_count", &point_count_i,
                     SHADER_UNIFORM_INT);
    if (point_count > 0) {
      set_shader_value_v(shader, "u_point_lights_position",
                         point_positions.data(), SHADER_UNIFORM_VEC3,
                         static_cast<int>(point_count));
      set_shader_value_v(shader, "u_point_lights_intensity",
                         point_intensities.data(), SHADER_UNIFORM_FLOAT,
                         static_cast<int>(point_count));
      set_shader_value_v(shader, "u_point_lights_color", point_colors.data(),
                         SHADER_UNIFORM_VEC3, static_cast<int>(point_count));
      set_shader_value_v(shader, "u_point_lights_range", point_ranges.data(),
                         SHADER_UNIFORM_FLOAT, static_cast<int>(point_count));
    }

    set_shader_value(shader, "u_spot_light_count", &spot_count_i,
                     SHADER_UNIFORM_INT);

    bool can_instance = false;
    if (draw_mat.shader.locs && mesh.vaoId != 0) {
      if (draw_mat.shader.locs[SHADER_LOC_MATRIX_MODEL] < 0) {
        draw_mat.shader.locs[SHADER_LOC_MATRIX_MODEL] =
            GetShaderLocationAttrib(draw_mat.shader, "instanceTransform");
      }
      can_instance = draw_mat.shader.locs[SHADER_LOC_MATRIX_MODEL] >= 0;
    }

    if (can_instance) {
      ::DrawMeshInstanced(mesh, draw_mat, batch.transforms.data(),
                          static_cast<int>(batch.transforms.size()));
    } else {
      for (const auto &tx : batch.transforms) {
        ::DrawMesh(mesh, draw_mat, tx);
      }
    }

    if (key.has_rc) {
      RenderConfig::restore_defaults();
    }
  }

  EndMode3D();
  DrawFPS(10, 10);
  EndDrawing();
}

inline void auto_insert_global_transform(World &world) {
  auto *transform_storage = world.get_storage<Transform3D>();
  auto *global_storage = world.get_storage<GlobalTransform>();
  if (!transform_storage)
    return;

  auto entities = transform_storage->entity_indices();
  for (auto e : entities) {
    if (!global_storage || !global_storage->get(e)) {
      world.insert_component(e, GlobalTransform{});
      global_storage = world.get_storage<GlobalTransform>();
    }
  }
}

inline void auto_insert_visibility(World &world) {
  auto *mesh_storage = world.get_storage<Mesh3d>();
  auto *vis_storage = world.get_storage<Visibility>();
  if (!mesh_storage)
    return;

  auto entities = mesh_storage->entity_indices();
  for (auto e : entities) {
    if (!vis_storage || !vis_storage->get(e)) {
      world.insert_component(e, Visibility::Visible);
      vis_storage = world.get_storage<Visibility>();
    }
  }
}

inline void auto_insert_material(World &world) {
  auto *mesh_storage = world.get_storage<Mesh3d>();
  auto *mat_storage = world.get_storage<Material3d>();
  if (!mesh_storage)
    return;

  auto entities = mesh_storage->entity_indices();
  for (auto e : entities) {
    if (!mat_storage || !mat_storage->get(e)) {
      world.insert_component(e,
                             Material3d{StandardMaterial::from_color(WHITE)});
      mat_storage = world.get_storage<Material3d>();
    }
  }
}

inline void auto_insert_render_config(World &world) {
  auto *mesh_storage = world.get_storage<Mesh3d>();
  auto *rc_storage = world.get_storage<RenderConfig>();
  if (!mesh_storage)
    return;

  auto entities = mesh_storage->entity_indices();
  for (auto e : entities) {
    if (!rc_storage || !rc_storage->get(e)) {
      world.insert_component(e, RenderConfig{});
      rc_storage = world.get_storage<RenderConfig>();
    }
  }
}

inline void render_plugin(App &app) {
  if (!app.world().contains_resource<Assets<Model>>()) {
    app.insert_resource(Assets<Model>{});
  }
  if (!app.world().contains_resource<Assets<Texture2D>>()) {
    app.insert_resource(Assets<Texture2D>{});
  }
  if (!app.world().contains_resource<ClearColor>()) {
    app.insert_resource(ClearColor{});
  }
  if (!app.world().contains_resource<RenderBatches>()) {
    app.insert_resource(RenderBatches{});
  }
  if (!app.world().contains_resource<AssetServer>()) {
    app.insert_resource(AssetServer{});
  }
  if (!app.world().contains_resource<LodConfig>()) {
    app.insert_resource(LodConfig{});
  }
  if (!app.world().contains_resource<LodHistory>()) {
    app.insert_resource(LodHistory{});
  }
  app.add_systems(ScheduleLabel::First, auto_insert_global_transform);
  app.add_systems(ScheduleLabel::First, auto_insert_visibility);
  app.add_systems(ScheduleLabel::First, auto_insert_material);

  app.add_systems(ScheduleLabel::PostUpdate, propagate_transforms);
  app.add_systems(ScheduleLabel::PostUpdate, compute_visibility);

  app.add_systems(ScheduleLabel::PostUpdate, compute_mesh_bounds_system);
  app.add_systems(ScheduleLabel::PostUpdate, frustum_cull_system);

  app.add_systems(ScheduleLabel::PostUpdate, auto_generate_lods_system);

  app.add_systems(ScheduleLabel::PostUpdate, build_render_batches_system);

  app.add_systems(ScheduleLabel::Last, render_system);
}

} // namespace ecs
