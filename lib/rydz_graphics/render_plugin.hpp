#pragma once
#include "camera3d.hpp"
#include "frustum.hpp"
#include "light.hpp"
#include "lod.hpp"
#include "material3d.hpp"
#include "math.hpp"
#include "mesh3d.hpp"
#include "render_batches.hpp"
#include "render_config.hpp"
#include "rl.hpp"
#include "rydz_ecs/app.hpp"
#include "rydz_ecs/asset.hpp"
#include "rydz_graphics/asset_loaders.hpp"
#include "rydz_graphics/transform.hpp"
#include "rydz_graphics/visibility.hpp"
#include "skybox.hpp"
#include "types.hpp"
#include <absl/container/flat_hash_map.h>
#include <array>
#include <cstring>
#include <print>
#include <vector>

namespace ecs {

using namespace math;

struct ClearColor {
  using Type = ResourceType;
  rl::Color color = {30, 30, 40, 255};
};

inline void apply_materials_system(Query<const Model3d, const Material3d> query,
                                   ResMut<Assets<rl::Model>> model_assets,
                                   ResMut<Assets<rl::Texture2D>> tex_assets) {
  for (auto [m3d, mat] : query.iter()) {
    rl::Model *model = model_assets->get(m3d->model);
    if (!model)
      continue;
    mat->material.apply(*model, tex_assets.ptr);
  }
}

inline void build_render_batches_system(
    Query<const Model3d, const GlobalTransform, Opt<const Material3d>,
          Opt<const RenderConfig>, Opt<const ViewVisibility>,
          Opt<const MeshLodGroup>>
        query,
    ResMut<Assets<rl::Model>> model_assets, ResMut<RenderBatches> batches,
    Res<LodConfig> lod_config, ResMut<LodHistory> lod_history, NonSendMarker) {
  batches->clear();
  absl::flat_hash_map<RenderBatchKey, size_t> batch_index;

  query.each([&](const Model3d *model3d, const GlobalTransform *global,
                 const Material3d *mat, const RenderConfig *rc,
                 const ViewVisibility *vv, const MeshLodGroup *lod_group) {
    if (!model3d || !model3d->model.is_valid())
      return;
    if (vv && !vv->visible)
      return;
    Handle<rl::Model> model_handle = model3d->model;
    if (lod_group && lod_group->level_count > 1) {
      model_handle = lod_group->levels[0];
    }

    rl::Model *model = model_assets->get(model_handle);
    if (!model || model->meshCount <= 0 || model->meshes == nullptr)
      return;

    if (model->meshes[0].vaoId == 0) {
      for (int i = 0; i < model->meshCount; ++i) {
        rl::UploadMesh(&model->meshes[i], false);
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
        batch.transforms.push_back(to_rl(global->matrix));
        batches->batches.push_back(std::move(batch));
        batch_index.emplace(batches->batches.back().key, idx);
      } else {
        batches->batches[it->second].transforms.push_back(
            to_rl(global->matrix));
      }
    }
  });
}

struct Texture {
  Handle<rl::Texture2D> handle;
};

inline void
render_system(Res<ClearColor> clear_color, Res<Assets<rl::Model>> model_assets,
              Res<Assets<rl::Texture2D>> tex_assets, Res<RenderBatches> batches,
              Query<const Texture, const Transform> textures,
              Query<const Camera3DComponent, const ActiveCamera,
                    const Transform, Opt<const Skybox>>
                  cam_query,
              Query<const DirectionalLight> dir_query,
              Query<const PointLight, const GlobalTransform> point_query,
              NonSendMarker) {
  rl::Color bg = clear_color->color;

  CameraView cam_view{
      .view = Mat4::sIdentity(),
      .proj = Mat4::sIdentity(),
      .position = Vec3(10, 10, 10),
  };
  const Skybox *active_skybox = nullptr;

  cam_query.each([&](const Camera3DComponent *cam_comp, const ActiveCamera *,
                     const Transform *cam_tx, const Skybox *sky) {
    if (cam_comp && cam_tx) {
      cam_view = compute_camera_view(*cam_tx, *cam_comp);
      active_skybox = sky;
    }
  });

  auto fallback_material = []() -> rl::Material & {
    static rl::Material fallback = {};
    static bool init = false;
    if (!init) {
      fallback.shader.id = rl::rlGetShaderIdDefault();
      fallback.shader.locs = rl::rlGetShaderLocsDefault();
      fallback.maps = (rl::MaterialMap *)RL_CALLOC(12, sizeof(rl::MaterialMap));
      fallback.maps[MATERIAL_MAP_DIFFUSE].texture.id =
          rl::rlGetTextureIdDefault();
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

  auto mul_color = [](rl::Color a, rl::Color b) -> rl::Color {
    return rl::Color{
        (unsigned char)((a.r * b.r) / 255),
        (unsigned char)((a.g * b.g) / 255),
        (unsigned char)((a.b * b.b) / 255),
        (unsigned char)((a.a * b.a) / 255),
    };
  };

  auto color_to_vec3 = [](rl::Color c) -> rl::Vector3 {
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
  std::array<rl::Vector3, kMaxPointLights> point_positions{};
  std::array<rl::Vector3, kMaxPointLights> point_colors{};
  std::array<float, kMaxPointLights> point_intensities{};
  std::array<float, kMaxPointLights> point_ranges{};
  usize point_count = 0;

  point_query.each([&](const PointLight *pl, const GlobalTransform *gt) {
    if (!pl || !gt || point_count >= kMaxPointLights)
      return;
    point_positions[point_count] = to_rl(gt->translation());
    point_colors[point_count] = color_to_vec3(pl->color);
    point_intensities[point_count] = pl->intensity;
    point_ranges[point_count] = pl->range;
    ++point_count;
  });

  rl::BeginDrawing();
  rl::ClearBackground(bg);

  rl::rlDrawRenderBatchActive();
  rl::rlMatrixMode(RL_PROJECTION);
  rl::rlPushMatrix();
  rl::rlLoadIdentity();
  rl::rlSetMatrixProjection(to_rl(cam_view.proj));
  rl::rlMatrixMode(RL_MODELVIEW);
  rl::rlLoadIdentity();
  rl::rlSetMatrixModelview(to_rl(cam_view.view));
  rl::rlEnableDepthTest();

  if (active_skybox && active_skybox->loaded) {
    active_skybox->draw(cam_view.view, cam_view.proj);
  }

  for (const auto &batch : batches->batches) {
    const auto &key = batch.key;
    const rl::Model *model = model_assets->get(key.model);
    if (!model || key.mesh_index < 0 || key.mesh_index >= model->meshCount)
      continue;

    const rl::Mesh &mesh = model->meshes[key.mesh_index];
    const rl::Material *src_mat = nullptr;
    if (model->materials && model->materialCount > 0 &&
        key.material_index >= 0 && key.material_index < model->materialCount) {
      src_mat = &model->materials[key.material_index];
    }
    if (!src_mat || !src_mat->maps) {
      src_mat = &fallback_material();
    }

    std::array<rl::MaterialMap, 12> local_maps;
    rl::Material draw_mat = *src_mat;
    memcpy(local_maps.data(), src_mat->maps, sizeof(local_maps));
    draw_mat.maps = local_maps.data();

    if (key.material.shader) {
      draw_mat.shader = *key.material.shader;
    } else if (draw_mat.shader.id == 0 || draw_mat.shader.locs == nullptr) {
      draw_mat.shader.id = rl::rlGetShaderIdDefault();
      draw_mat.shader.locs = rl::rlGetShaderLocsDefault();
    }

    draw_mat.maps[MATERIAL_MAP_DIFFUSE].color = mul_color(
        draw_mat.maps[MATERIAL_MAP_DIFFUSE].color, key.material.base_color);

    if (key.material.texture_id != UINT32_MAX) {
      if (auto *tex =
              tex_assets->get(Handle<rl::Texture2D>{key.material.texture_id})) {
        draw_mat.maps[MATERIAL_MAP_DIFFUSE].texture = *tex;
      }
    }
    if (key.material.normal_id != UINT32_MAX) {
      if (auto *tex =
              tex_assets->get(Handle<rl::Texture2D>{key.material.normal_id})) {
        draw_mat.maps[MATERIAL_MAP_NORMAL].texture = *tex;
      }
    }

    if (key.has_rc) {
      key.rc.apply();
    }

    auto set_shader_value = [](rl::Shader &shader, const char *name,
                               const void *value, int type) {
      int loc = rl::GetShaderLocation(shader, name);
      if (loc >= 0) {
        rl::SetShaderValue(shader, loc, value, type);
      }
    };

    auto set_shader_value_v = [](rl::Shader &shader, const char *name,
                                 const void *value, int type, int count) {
      int loc = rl::GetShaderLocation(shader, name);
      if (loc >= 0) {
        rl::SetShaderValueV(shader, loc, value, type, count);
      }
    };

    rl::Shader &shader = draw_mat.shader;
    float metallic = key.material.metallic;
    float roughness = key.material.roughness;
    float normal_factor = 1.0f;
    float occlusion_factor = 1.0f;
    rl::Vector3 emissive = {0.0f, 0.0f, 0.0f};
    rl::Vector4 tint = {1.0f, 1.0f, 1.0f, 0.0f};
    rl::Vector3 camera_pos = to_rl(cam_view.position);

    int has_normal = 0;
    if (key.material.normal_id != UINT32_MAX) {
      if (tex_assets->get(Handle<rl::Texture2D>{key.material.normal_id})) {
        has_normal = 1;
      }
    }

    int has_roughness_metallic = 0;
    int has_occlusion = 0;
    int has_emissive = 0;

    int has_directional = has_dir ? 1 : 0;
    rl::Vector3 dir_color = color_to_vec3(dir_light.color);
    rl::Vector3 dir_dir = to_rl(dir_light.direction.Normalized());
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
      if (draw_mat.shader.locs[SHADER_LOC_VERTEX_INSTANCETRANSFORM] < 0) {
        draw_mat.shader.locs[SHADER_LOC_VERTEX_INSTANCETRANSFORM] =
            rl::GetShaderLocationAttrib(draw_mat.shader, "instanceTransform");
      }
      if (draw_mat.shader.locs[SHADER_LOC_MATRIX_MODEL] < 0) {
        draw_mat.shader.locs[SHADER_LOC_MATRIX_MODEL] =
            rl::GetShaderLocation(draw_mat.shader, "matModel");
      }
      can_instance =
          draw_mat.shader.locs[SHADER_LOC_VERTEX_INSTANCETRANSFORM] >= 0;
    }

    if (can_instance) {
      rl::DrawMeshInstanced(mesh, draw_mat, batch.transforms.data(),
                            static_cast<int>(batch.transforms.size()));
    } else {
      for (const auto &tx : batch.transforms) {
        rl::DrawMesh(mesh, draw_mat, tx);
      }
    }

    if (key.has_rc) {
      RenderConfig::restore_defaults();
    }
  }

  rl::rlDrawRenderBatchActive();
  rl::rlMatrixMode(RL_PROJECTION);
  rl::rlPopMatrix();
  rl::rlMatrixMode(RL_MODELVIEW);
  rl::rlLoadIdentity();
  rl::rlDisableDepthTest();
  rl::rlDisableBackfaceCulling();

  textures.each([&](const Texture *tex, const Transform *tx) {
    if (!tex || !tex->handle.is_valid())
      return;
    const rl::Texture2D *rl_tex = tex_assets->get(tex->handle);
    if (!rl_tex)
      return;

    rl::Vector2 pos = {tx->translation.GetX(), tx->translation.GetY()};
    float rotation_deg = 0.0f;
    float siny_cosp = 2.0f * (tx->rotation.GetW() * tx->rotation.GetZ() +
                              tx->rotation.GetX() * tx->rotation.GetY());
    float cosy_cosp = 1.0f - 2.0f * (tx->rotation.GetY() * tx->rotation.GetY() +
                                     tx->rotation.GetZ() * tx->rotation.GetZ());
    rotation_deg = atan2f(siny_cosp, cosy_cosp) * (180.0f / 3.14159265f);

    float scale_x = tx->scale.GetX();
    float scale_y = tx->scale.GetY();

    rl::Rectangle source = {0, 0, (float)rl_tex->width, (float)rl_tex->height};
    rl::Rectangle dest = {pos.x, pos.y, rl_tex->width * scale_x,
                          rl_tex->height * scale_y};
    rl::Vector2 origin = {0, 0};

    rl::DrawTexturePro(*rl_tex, source, dest, origin, rotation_deg, WHITE);
  });

  rl::DrawFPS(10, 10);
  rl::EndDrawing();
}

inline void apply_model_transform(Query<Model3d, Mut<GlobalTransform>> query,
                                  Res<Assets<rl::Model>> model_assets) {
  for (auto [m3d, gt] : query.iter()) {
    auto *model = model_assets->get(m3d->model);
    if (!model)
      continue;

    gt->matrix = gt->matrix * from_rl(model->transform);
  }
}

inline void auto_insert_global_transform(
    Query<Entity, With<Transform>, Without<GlobalTransform>> query, Cmd cmd) {
  for (auto [e] : query.iter())
    cmd.entity(e).insert(GlobalTransform{});
}

inline void auto_insert_visibility(
    Query<Entity, With<Model3d>, Without<Visibility>> query, Cmd cmd) {
  for (auto [e] : query.iter())
    cmd.entity(e).insert(Visibility::Visible);
}

inline void auto_insert_material(
    Query<Entity, With<Model3d>, Without<Material3d>> query, Cmd cmd) {
  for (auto [e] : query.iter())
    cmd.entity(e).insert(Material3d{StandardMaterial::from_color(WHITE)});
}

inline void auto_insert_render_config(
    Query<Entity, With<Model3d>, Without<RenderConfig>> query, Cmd cmd) {
  for (auto [e] : query.iter())
    cmd.entity(e).insert(RenderConfig{});
}

inline void render_plugin(App &app) {
  if (!app.world().has_resource<Assets<rl::Model>>()) {
    app.insert_resource(Assets<rl::Model>{});
  }
  if (!app.world().has_resource<Assets<rl::Mesh>>()) {
    app.insert_resource(Assets<rl::Mesh>{});
  }
  if (!app.world().has_resource<Assets<rl::Texture2D>>()) {
    app.insert_resource(Assets<rl::Texture2D>{});
  }
  if (!app.world().has_resource<ClearColor>()) {
    app.insert_resource(ClearColor{});
  }
  if (!app.world().has_resource<RenderBatches>()) {
    app.insert_resource(RenderBatches{});
  }
  if (!app.world().has_resource<AssetServer>()) {
    app.insert_resource(AssetServer{});
  }
  {
    auto *server = app.world().get_resource<AssetServer>();
    if (server) {
      register_default_loaders(*server);
    }
  }
  if (!app.world().has_resource<LodConfig>()) {
    app.insert_resource(LodConfig{});
  }
  if (!app.world().has_resource<LodHistory>()) {
    app.insert_resource(LodHistory{});
  }

  if (!app.world().has_resource<PendingModelLoads>()) {
    app.insert_resource(PendingModelLoads{});
  }
  app.add_systems(
      ScheduleLabel::First,
      [](ResMut<AssetServer> server, World &world) { server->update(world); });
  app.add_systems(ScheduleLabel::First, process_pending_model_loads);

  app.add_systems(ScheduleLabel::First, auto_insert_global_transform);
  app.add_systems(ScheduleLabel::First, auto_insert_visibility);
  app.add_systems(ScheduleLabel::First, auto_insert_material);

  app.add_systems(ScheduleLabel::PostUpdate, propagate_transforms);
  app.add_systems(ScheduleLabel::PostUpdate, apply_model_transform);
  app.add_systems(ScheduleLabel::PostUpdate, compute_visibility);
  app.add_systems(ScheduleLabel::PostUpdate, compute_mesh_bounds_system);
  app.add_systems(ScheduleLabel::PostUpdate, frustum_cull_system);
  app.add_systems(ScheduleLabel::PostUpdate, auto_generate_lods_system);

  app.add_systems(ScheduleLabel::ExtractRender, build_render_batches_system);

  app.add_systems(ScheduleLabel::Render, render_system);
}

} // namespace ecs
