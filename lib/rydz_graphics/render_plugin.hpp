#pragma once
#include "frustum.hpp"
#include "light.hpp"
#include "lod.hpp"
#include "material3d.hpp"
#include "math.hpp"
#include "mesh3d.hpp"
#include "render_batches.hpp"
#include "render_config.hpp"
#include "rl.hpp"
#include "rydz_camera/camera3d.hpp"
#include "rydz_ecs/app.hpp"
#include "rydz_ecs/asset.hpp"
#include "rydz_graphics/asset_loaders.hpp"
#include "rydz_graphics/transform.hpp"
#include "rydz_graphics/visibility.hpp"
#include "skybox.hpp"
#include "types.hpp"
#include <array>
#include <cstring>
#include <print>
#include <unordered_map>
#include <vector>

namespace ecs {

using namespace math;

struct ClearColor {
  using T = Resource;
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
  std::unordered_map<RenderBatchKey, size_t> batch_index;

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
      mat_key.emissive_color = m.emissive_color;
      mat_key.texture_id = m.texture.id;
      mat_key.normal_id = m.normal_map.id;
      mat_key.metallic_id = m.metallic_map.id;
      mat_key.roughness_id = m.roughness_map.id;
      mat_key.occlusion_id = m.occlusion_map.id;
      mat_key.emissive_id = m.emissive_map.id;
      mat_key.metallic = m.metallic;
      mat_key.roughness = m.roughness;
      mat_key.normal_scale = m.normal_scale;
      mat_key.occlusion_strength = m.occlusion_strength;
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

using TextureRenderQuery = Query<const Texture, const Transform>;
using ActiveCameraRenderQuery =
    Query<const Camera3DComponent, const ActiveCamera, const GlobalTransform,
          Opt<const Skybox>>;
using DirectionalLightRenderQuery = Query<const DirectionalLight>;
using PointLightRenderQuery = Query<const PointLight, const GlobalTransform>;

namespace detail {

inline constexpr int kMaxPointLights = 16;

struct RenderSceneState {
  CameraView camera_view{
      .view = Mat4::sIdentity(),
      .proj = Mat4::sIdentity(),
      .position = Vec3(10, 10, 10),
  };
  const Skybox *active_skybox = nullptr;
  DirectionalLight dir_light{};
  bool has_directional = false;
  std::array<rl::Vector3, kMaxPointLights> point_positions{};
  std::array<rl::Vector3, kMaxPointLights> point_colors{};
  std::array<float, kMaxPointLights> point_intensities{};
  std::array<float, kMaxPointLights> point_ranges{};
  usize point_count = 0;
};

struct PreparedMaterial {
  std::array<rl::MaterialMap, 12> local_maps{};
  rl::Material material{};
};

inline rl::Material &fallback_material() {
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
}

inline bool has_texture(const rl::Texture2D &texture) { return texture.id > 0; }

inline void apply_pbr_defaults(rl::Material &material) {
  if (material.maps[MATERIAL_MAP_DIFFUSE].color.a == 0) {
    material.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
  }
  if (material.maps[MATERIAL_MAP_ROUGHNESS].value <= 0.0f &&
      !has_texture(material.maps[MATERIAL_MAP_ROUGHNESS].texture)) {
    material.maps[MATERIAL_MAP_ROUGHNESS].value = 1.0f;
  }
  if (material.maps[MATERIAL_MAP_NORMAL].value <= 0.0f) {
    material.maps[MATERIAL_MAP_NORMAL].value = 1.0f;
  }
  if (material.maps[MATERIAL_MAP_OCCLUSION].value <= 0.0f) {
    material.maps[MATERIAL_MAP_OCCLUSION].value = 1.0f;
  }
}

inline rl::Vector3 color_to_vec3(rl::Color color) {
  return {color.r / 255.0f, color.g / 255.0f, color.b / 255.0f};
}

inline RenderSceneState
collect_render_scene(ActiveCameraRenderQuery cam_query,
                     DirectionalLightRenderQuery dir_query,
                     PointLightRenderQuery point_query) {
  RenderSceneState scene{};

  cam_query.each([&](const Camera3DComponent *cam_comp, const ActiveCamera *,
                     const GlobalTransform *cam_gt, const Skybox *sky) {
    if (!cam_comp || !cam_gt)
      return;
    scene.camera_view = compute_camera_view(*cam_gt, *cam_comp);
    scene.active_skybox = sky;
  });

  dir_query.each([&](const DirectionalLight *dir) {
    if (!dir || scene.has_directional)
      return;
    scene.dir_light = *dir;
    scene.has_directional = true;
  });

  point_query.each([&](const PointLight *pl, const GlobalTransform *gt) {
    if (!pl || !gt || scene.point_count >= scene.point_positions.size())
      return;
    scene.point_positions[scene.point_count] = to_rl(gt->translation());
    scene.point_colors[scene.point_count] = color_to_vec3(pl->color);
    scene.point_intensities[scene.point_count] = pl->intensity;
    scene.point_ranges[scene.point_count] = pl->range;
    ++scene.point_count;
  });

  return scene;
}

inline void begin_world_pass(rl::Color clear_color,
                             const RenderSceneState &scene) {
  rl::BeginDrawing();
  rl::ClearBackground(clear_color);

  rl::rlDrawRenderBatchActive();
  rl::rlMatrixMode(RL_PROJECTION);
  rl::rlPushMatrix();
  rl::rlLoadIdentity();
  rl::rlSetMatrixProjection(to_rl(scene.camera_view.proj));
  rl::rlMatrixMode(RL_MODELVIEW);
  rl::rlLoadIdentity();
  rl::rlSetMatrixModelview(to_rl(scene.camera_view.view));
  rl::rlEnableDepthTest();

  if (scene.active_skybox && scene.active_skybox->loaded) {
    scene.active_skybox->draw(scene.camera_view.view, scene.camera_view.proj);
  }
}

inline void end_world_pass() {
  rl::rlDrawRenderBatchActive();
  rl::rlMatrixMode(RL_PROJECTION);
  rl::rlPopMatrix();
  rl::rlMatrixMode(RL_MODELVIEW);
  rl::rlLoadIdentity();
  rl::rlDisableDepthTest();
  rl::rlDisableBackfaceCulling();
}

inline const rl::Material *resolve_source_material(const rl::Model &model,
                                                   int material_index) {
  if (model.materials && model.materialCount > 0 && material_index >= 0 &&
      material_index < model.materialCount) {
    const rl::Material *material = &model.materials[material_index];
    if (material->maps) {
      return material;
    }
  }
  return &fallback_material();
}

inline void prepare_draw_material(const RenderBatchKey &key,
                                  const rl::Model &model,
                                  const Assets<rl::Texture2D> &tex_assets,
                                  PreparedMaterial &prepared) {
  const rl::Material *src_mat =
      resolve_source_material(model, key.material_index);
  prepared.material = *src_mat;
  std::memcpy(prepared.local_maps.data(), src_mat->maps,
              sizeof(prepared.local_maps));
  prepared.material.maps = prepared.local_maps.data();
  apply_pbr_defaults(prepared.material);

  if (key.material.shader) {
    prepared.material.shader = *key.material.shader;
  } else if (prepared.material.shader.id == 0 ||
             prepared.material.shader.locs == nullptr) {
    prepared.material.shader.id = rl::rlGetShaderIdDefault();
    prepared.material.shader.locs = rl::rlGetShaderLocsDefault();
  }

  auto multiply_color = [](rl::Color a, rl::Color b) -> rl::Color {
    return rl::Color{
        static_cast<u8>((a.r * b.r) / 255),
        static_cast<u8>((a.g * b.g) / 255),
        static_cast<u8>((a.b * b.b) / 255),
        static_cast<u8>((a.a * b.a) / 255),
    };
  };

  prepared.material.maps[MATERIAL_MAP_DIFFUSE].color =
      multiply_color(prepared.material.maps[MATERIAL_MAP_DIFFUSE].color,
                     key.material.base_color);

  if (key.material.texture_id != U32_MAX) {
    if (auto *tex =
            tex_assets.get(Handle<rl::Texture2D>{key.material.texture_id})) {
      prepared.material.maps[MATERIAL_MAP_DIFFUSE].texture = *tex;
    }
  }

  if (key.material.normal_id != U32_MAX) {
    if (auto *tex =
            tex_assets.get(Handle<rl::Texture2D>{key.material.normal_id})) {
      prepared.material.maps[MATERIAL_MAP_NORMAL].texture = *tex;
    }
  }

  if (key.material.metallic_id != U32_MAX) {
    if (auto *tex =
            tex_assets.get(Handle<rl::Texture2D>{key.material.metallic_id})) {
      prepared.material.maps[MATERIAL_MAP_METALNESS].texture = *tex;
    }
  }

  if (key.material.roughness_id != U32_MAX) {
    if (auto *tex =
            tex_assets.get(Handle<rl::Texture2D>{key.material.roughness_id})) {
      prepared.material.maps[MATERIAL_MAP_ROUGHNESS].texture = *tex;
    }
  }

  if (key.material.occlusion_id != U32_MAX) {
    if (auto *tex =
            tex_assets.get(Handle<rl::Texture2D>{key.material.occlusion_id})) {
      prepared.material.maps[MATERIAL_MAP_OCCLUSION].texture = *tex;
    }
  }

  if (key.material.emissive_id != U32_MAX) {
    if (auto *tex =
            tex_assets.get(Handle<rl::Texture2D>{key.material.emissive_id})) {
      prepared.material.maps[MATERIAL_MAP_EMISSION].texture = *tex;
    }
  }

  if (key.material.metallic >= 0.0f) {
    prepared.material.maps[MATERIAL_MAP_METALNESS].value =
        key.material.metallic;
  }
  if (key.material.roughness >= 0.0f) {
    prepared.material.maps[MATERIAL_MAP_ROUGHNESS].value =
        key.material.roughness;
  }
  if (key.material.normal_scale >= 0.0f) {
    prepared.material.maps[MATERIAL_MAP_NORMAL].value =
        key.material.normal_scale;
  }
  if (key.material.occlusion_strength >= 0.0f) {
    prepared.material.maps[MATERIAL_MAP_OCCLUSION].value =
        key.material.occlusion_strength;
  }
  if (key.material.emissive_color.a > 0) {
    prepared.material.maps[MATERIAL_MAP_EMISSION].color =
        key.material.emissive_color;
  }
}

inline void set_shader_value(rl::Shader &shader, const char *name,
                             const void *value, int type) {
  int loc = rl::GetShaderLocation(shader, name);
  if (loc >= 0) {
    rl::SetShaderValue(shader, loc, value, type);
  }
}

inline void set_shader_value_v(rl::Shader &shader, const char *name,
                               const void *value, int type, int count) {
  int loc = rl::GetShaderLocation(shader, name);
  if (loc >= 0) {
    rl::SetShaderValueV(shader, loc, value, type, count);
  }
}

inline void apply_shader_uniforms(rl::Shader &shader,
                                  const PreparedMaterial &prepared,
                                  const RenderSceneState &scene) {
  float metallic = prepared.material.maps[MATERIAL_MAP_METALNESS].value;
  float roughness = prepared.material.maps[MATERIAL_MAP_ROUGHNESS].value;
  float normal_factor = prepared.material.maps[MATERIAL_MAP_NORMAL].value;
  float occlusion_factor = prepared.material.maps[MATERIAL_MAP_OCCLUSION].value;
  rl::Vector3 emissive =
      color_to_vec3(prepared.material.maps[MATERIAL_MAP_EMISSION].color);
  rl::Vector4 tint = {1.0f, 1.0f, 1.0f, 0.0f};
  rl::Vector3 camera_pos = to_rl(scene.camera_view.position);

  if (normal_factor <= 0.0f) {
    normal_factor = 1.0f;
  }
  if (occlusion_factor <= 0.0f) {
    occlusion_factor = 1.0f;
  }

  int has_metallic =
      has_texture(prepared.material.maps[MATERIAL_MAP_METALNESS].texture) ? 1
                                                                          : 0;
  int has_roughness =
      has_texture(prepared.material.maps[MATERIAL_MAP_ROUGHNESS].texture) ? 1
                                                                          : 0;
  int has_normal =
      has_texture(prepared.material.maps[MATERIAL_MAP_NORMAL].texture) ? 1 : 0;
  int has_occlusion =
      has_texture(prepared.material.maps[MATERIAL_MAP_OCCLUSION].texture) ? 1
                                                                          : 0;
  int has_emissive =
      has_texture(prepared.material.maps[MATERIAL_MAP_EMISSION].texture) ? 1
                                                                         : 0;

  int has_directional = scene.has_directional ? 1 : 0;
  rl::Vector3 dir_color = color_to_vec3(scene.dir_light.color);
  rl::Vector3 dir_dir = to_rl(scene.dir_light.direction.Normalized());
  float dir_intensity = scene.dir_light.intensity;

  int point_count = static_cast<int>(scene.point_count);
  int spot_count = 0;

  set_shader_value(shader, "u_metallic_factor", &metallic,
                   SHADER_UNIFORM_FLOAT);
  set_shader_value(shader, "u_roughness_factor", &roughness,
                   SHADER_UNIFORM_FLOAT);
  set_shader_value(shader, "u_normal_factor", &normal_factor,
                   SHADER_UNIFORM_FLOAT);
  set_shader_value(shader, "u_occlusion_factor", &occlusion_factor,
                   SHADER_UNIFORM_FLOAT);
  set_shader_value(shader, "u_emissive_factor", &emissive, SHADER_UNIFORM_VEC3);
  set_shader_value(shader, "u_color", &tint, SHADER_UNIFORM_VEC4);
  set_shader_value(shader, "u_camera_pos", &camera_pos, SHADER_UNIFORM_VEC3);

  set_shader_value(shader, "u_has_metallic_texture", &has_metallic,
                   SHADER_UNIFORM_INT);
  set_shader_value(shader, "u_has_roughness_texture", &has_roughness,
                   SHADER_UNIFORM_INT);
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

  set_shader_value(shader, "u_point_light_count", &point_count,
                   SHADER_UNIFORM_INT);
  if (scene.point_count > 0) {
    set_shader_value_v(shader, "u_point_lights_position",
                       scene.point_positions.data(), SHADER_UNIFORM_VEC3,
                       point_count);
    set_shader_value_v(shader, "u_point_lights_intensity",
                       scene.point_intensities.data(), SHADER_UNIFORM_FLOAT,
                       point_count);
    set_shader_value_v(shader, "u_point_lights_color",
                       scene.point_colors.data(), SHADER_UNIFORM_VEC3,
                       point_count);
    set_shader_value_v(shader, "u_point_lights_range",
                       scene.point_ranges.data(), SHADER_UNIFORM_FLOAT,
                       point_count);
  }

  set_shader_value(shader, "u_spot_light_count", &spot_count,
                   SHADER_UNIFORM_INT);
}

inline bool can_draw_instanced(rl::Material &material, const rl::Mesh &mesh) {
  if (!material.shader.locs || mesh.vaoId == 0) {
    return false;
  }

  if (material.shader.locs[SHADER_LOC_VERTEX_INSTANCETRANSFORM] < 0) {
    material.shader.locs[SHADER_LOC_VERTEX_INSTANCETRANSFORM] =
        rl::GetShaderLocationAttrib(material.shader, "instanceTransform");
  }
  if (material.shader.locs[SHADER_LOC_MATRIX_MODEL] < 0) {
    material.shader.locs[SHADER_LOC_MATRIX_MODEL] =
        rl::GetShaderLocation(material.shader, "matModel");
  }

  return material.shader.locs[SHADER_LOC_VERTEX_INSTANCETRANSFORM] >= 0;
}

inline void draw_batch_instances(const rl::Mesh &mesh, rl::Material &material,
                                 const RenderBatch &batch) {
  if (can_draw_instanced(material, mesh)) {
    rl::DrawMeshInstanced(mesh, material, batch.transforms.data(),
                          static_cast<int>(batch.transforms.size()));
    return;
  }

  for (const auto &transform : batch.transforms) {
    rl::DrawMesh(mesh, material, transform);
  }
}

inline void draw_render_batch(const RenderBatch &batch,
                              const Assets<rl::Model> &model_assets,
                              const Assets<rl::Texture2D> &tex_assets,
                              const RenderSceneState &scene) {
  const auto &key = batch.key;
  const rl::Model *model = model_assets.get(key.model);
  if (!model || key.mesh_index < 0 || key.mesh_index >= model->meshCount) {
    return;
  }

  const rl::Mesh &mesh = model->meshes[key.mesh_index];
  PreparedMaterial prepared{};
  prepare_draw_material(key, *model, tex_assets, prepared);

  if (key.has_rc) {
    key.rc.apply();
  }

  apply_shader_uniforms(prepared.material.shader, prepared, scene);
  draw_batch_instances(mesh, prepared.material, batch);

  if (key.has_rc) {
    RenderConfig::restore_defaults();
  }
}

inline float texture_rotation_degrees(const Transform &tx) {
  float siny_cosp = 2.0f * (tx.rotation.GetW() * tx.rotation.GetZ() +
                            tx.rotation.GetX() * tx.rotation.GetY());
  float cosy_cosp = 1.0f - 2.0f * (tx.rotation.GetY() * tx.rotation.GetY() +
                                   tx.rotation.GetZ() * tx.rotation.GetZ());
  return atan2f(siny_cosp, cosy_cosp) * (180.0f / 3.14159265f);
}

inline void draw_screen_textures(TextureRenderQuery textures,
                                 const Assets<rl::Texture2D> &tex_assets) {
  textures.each([&](const Texture *tex, const Transform *tx) {
    if (!tex || !tx || !tex->handle.is_valid())
      return;

    const rl::Texture2D *rl_tex = tex_assets.get(tex->handle);
    if (!rl_tex)
      return;

    rl::Vector2 pos = {tx->translation.GetX(), tx->translation.GetY()};
    rl::Rectangle source = {0, 0, static_cast<float>(rl_tex->width),
                            static_cast<float>(rl_tex->height)};
    rl::Rectangle dest = {pos.x, pos.y, rl_tex->width * tx->scale.GetX(),
                          rl_tex->height * tx->scale.GetY()};
    rl::Vector2 origin = {0, 0};

    rl::DrawTexturePro(*rl_tex, source, dest, origin,
                       texture_rotation_degrees(*tx), WHITE);
  });
}

} // namespace detail

inline void
render_system(Res<ClearColor> clear_color, Res<Assets<rl::Model>> model_assets,
              Res<Assets<rl::Texture2D>> tex_assets, Res<RenderBatches> batches,
              TextureRenderQuery textures, ActiveCameraRenderQuery cam_query,
              DirectionalLightRenderQuery dir_query,
              PointLightRenderQuery point_query, NonSendMarker) {
  const auto scene =
      detail::collect_render_scene(cam_query, dir_query, point_query);

  detail::begin_world_pass(clear_color->color, scene);
  for (const auto &batch : batches->batches) {
    detail::draw_render_batch(batch, *model_assets, *tex_assets, scene);
  }
  detail::end_world_pass();

  detail::draw_screen_textures(textures, *tex_assets);
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

inline void
auto_insert_visibility(Query<Entity, With<Model3d>, Without<Visibility>> query,
                       Cmd cmd) {
  for (auto [e] : query.iter())
    cmd.entity(e).insert(Visibility::Visible);
}

inline void
auto_insert_material(Query<Entity, With<Model3d>, Without<Material3d>> query,
                     Cmd cmd) {
  for (auto [e] : query.iter())
    cmd.entity(e).insert(Material3d{StandardMaterial::from_color(WHITE)});
}

inline void auto_insert_render_config(
    Query<Entity, With<Model3d>, Without<RenderConfig>> query, Cmd cmd) {
  for (auto [e] : query.iter())
    cmd.entity(e).insert(RenderConfig{});
}

inline void render_plugin(App &app) {
  app.insert_resource(AssetServer{});
  app.insert_resource(ClearColor{});
  app.insert_resource(RenderBatches{});
  app.insert_resource(LodConfig{});
  app.insert_resource(LodHistory{});
  app.insert_resource(PendingModelLoads{});

  app.insert_resource(Assets<rl::Model>{});
  app.insert_resource(Assets<rl::Mesh>{});
  app.insert_resource(Assets<rl::Texture2D>{});

  app.world().get_resource<Assets<rl::Model>>()->set_deleter(
      [](rl::Model &m) { rl::UnloadModel(m); });

  app.world().get_resource<Assets<rl::Mesh>>()->set_deleter(
      [](rl::Mesh &m) { rl::UnloadMesh(m); });

  app.world().get_resource<Assets<rl::Texture2D>>()->set_deleter(
      [](rl::Texture2D &t) { rl::UnloadTexture(t); });

  if (auto *server = app.world().get_resource<AssetServer>()) {
    register_default_loaders(*server);
  }

  app.add_systems(ScheduleLabel::First, [](World &world) {
    if (auto *server = world.get_resource<AssetServer>()) {
      server->update(world);
    }
  });

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
