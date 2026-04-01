#pragma once
#include "frustum.hpp"
#include "light.hpp"
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

struct Texture {
  Handle<rl::Texture2D> handle;
};

struct RenderPlugin {
  static void install(App &app) {
    app.init_resource<Assets<rl::Model>>(
           [](rl::Model &m) { rl::UnloadModel(m); })
        .init_resource<Assets<rl::Mesh>>([](rl::Mesh &m) { rl::UnloadMesh(m); })
        .init_resource<Assets<rl::Texture2D>>(
            [](rl::Texture2D &t) { rl::UnloadTexture(t); })
        .init_resource<ClearColor>()
        .init_resource<RenderBatches>()
        .init_resource<AssetServer>()
        .init_resource<PendingModelLoads>();

    if (auto *server = app.world().get_resource<AssetServer>()) {
      register_default_loaders(*server);
    }

    app.add_systems(ScheduleLabel::First, [](World &world) {
      if (auto *server = world.get_resource<AssetServer>()) {
        server->update(world);
      }
    });

    app.add_systems(ScheduleLabel::First,
                    group(process_pending_model_loads,
                          auto_insert_global_transform,
                          auto_insert_render_config, auto_insert_visibility,
                          auto_insert_material))

        .add_systems(ScheduleLabel::PostUpdate,
                     group(propagate_transforms, apply_model_transform,
                           compute_visibility, compute_mesh_bounds_system,
                           frustum_cull_system))

        .add_systems(ScheduleLabel::ExtractRender,
                     group(apply_materials_system, build_render_batches_system))

        .add_systems(ScheduleLabel::Render, render_system);
  }

private:
  using TextureRenderQuery = Query<Texture, Transform>;

  using ActiveCameraRenderQuery =
      Query<Camera3DComponent, ActiveCamera, GlobalTransform, Opt<Skybox>,
            Opt<RenderConfig>>;

  using DirectionalLightRenderQuery = Query<DirectionalLight>;

  using PointLightRenderQuery = Query<PointLight, GlobalTransform>;

  static void
  apply_materials_system(Query<const Model3d, const Material3d> query,
                         ResMut<Assets<rl::Model>> model_assets,
                         ResMut<Assets<rl::Texture2D>> tex_assets) {
    for (auto [m3d, mat] : query.iter()) {
      rl::Model *model = model_assets->get(m3d->model);
      if (!model)
        continue;
      mat->material.apply(*model, tex_assets.ptr);
    }
  }

  static void build_render_batches_system(
      Query<Model3d, GlobalTransform, Opt<Material3d>, Opt<ViewVisibility>>
          query,
      ResMut<Assets<rl::Model>> model_assets, ResMut<RenderBatches> batches,
      NonSendMarker) {
    batches->clear();
    std::unordered_map<RenderBatchKey, usize> batch_index;

    for (auto [model3d, global, mat, vv] : query.iter()) {
      if (!model3d || !model3d->model.is_valid())
        continue;
      if (vv && !vv->visible)
        continue;

      rl::Model *model = model_assets->get(model3d->model);
      if (!model || model->meshCount <= 0 || model->meshes == nullptr)
        continue;

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

      for (int i = 0; i < model->meshCount; ++i) {
        int mat_index = 0;
        if (model->meshMaterial && model->meshMaterial[i] >= 0 &&
            model->meshMaterial[i] < model->materialCount) {
          mat_index = model->meshMaterial[i];
        }

        RenderBatchKey key{};
        key.model = model3d->model;
        key.mesh_index = i;
        key.material_index = mat_index;
        key.material = mat_key;

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
    }
  }

  static void render_system(
      Res<ClearColor> clear_color, Res<Assets<rl::Model>> model_assets,
      Res<Assets<rl::Texture2D>> tex_assets, Res<RenderBatches> batches,
      TextureRenderQuery textures, ActiveCameraRenderQuery cam_query,
      DirectionalLightRenderQuery dir_query, PointLightRenderQuery point_query,
      NonSendMarker marker) {
    const auto scene = collect_render_scene(cam_query, dir_query, point_query);

    begin_world_pass(marker, clear_color->color, scene);

    for (const auto &batch : batches->batches) {
      draw_render_batch(marker, batch, *model_assets, *tex_assets, scene);
    }

    if (scene.has_render_config) {
      RenderConfig::restore_defaults();
    }

    end_world_pass(marker);
    draw_screen_textures(textures, *tex_assets);
    rl::DrawFPS(10, 10);
    rl::EndDrawing();
  }

  static void apply_model_transform(Query<Model3d, Mut<GlobalTransform>> query,
                                    Res<Assets<rl::Model>> model_assets) {
    for (auto [m3d, gt] : query.iter()) {
      auto *model = model_assets->get(m3d->model);
      if (!model)
        continue;

      gt->matrix = gt->matrix * from_rl(model->transform);
    }
  }

  static void auto_insert_global_transform(
      Query<Entity, Added<Transform>, Without<GlobalTransform>> query,
      Cmd cmd) {
    for (auto [e] : query.iter())
      cmd.entity(e).insert(GlobalTransform{});
  }

  static void auto_insert_visibility(
      Query<Entity, With<Model3d>, Without<Visibility>> query, Cmd cmd) {
    for (auto [e] : query.iter())
      cmd.entity(e).insert(Visibility::Visible);
  }

  static void
  auto_insert_material(Query<Entity, With<Model3d>, Without<Material3d>> query,
                       Cmd cmd) {
    for (auto [e] : query.iter())
      cmd.entity(e).insert(Material3d{StandardMaterial::from_color(WHITE)});
  }

  static void auto_insert_render_config(
      Query<Entity, With<Camera3DComponent>, Without<RenderConfig>> query,
      Cmd cmd) {
    for (auto [e] : query.iter())
      cmd.entity(e).insert(RenderConfig{});
  }
  static constexpr int kMaxPointLights = 16;

  struct RenderSceneState {
    CameraView camera_view{
        .view = Mat4::sIdentity(),
        .proj = Mat4::sIdentity(),
        .position = Vec3(10, 10, 10),
    };
    const Skybox *active_skybox = nullptr;
    RenderConfig render_config{};
    bool has_render_config = false;
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

  struct PbrFallbackTextures {
    rl::Texture2D metallic_black{};
    rl::Texture2D roughness_white{};
    rl::Texture2D normal_flat{};
    rl::Texture2D occlusion_white{};
    rl::Texture2D emission_black{};
  };

  static rl::Material &fallback_material(NonSendMarker) {
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

  static bool has_texture(const rl::Texture2D &texture) {
    return texture.id > 0;
  }

  static PbrFallbackTextures &pbr_fallback_textures() {
    static PbrFallbackTextures textures = [] {
      auto make_texture = [](rl::Color color) {
        rl::Image image = rl::GenImageColor(1, 1, color);
        rl::Texture2D texture = rl::LoadTextureFromImage(image);
        rl::UnloadImage(image);
        return texture;
      };

      return PbrFallbackTextures{
          .metallic_black = make_texture({0, 0, 0, 255}),
          .roughness_white = make_texture(WHITE),
          .normal_flat = make_texture({128, 128, 255, 255}),
          .occlusion_white = make_texture(WHITE),
          .emission_black = make_texture({0, 0, 0, 255}),
      };
    }();
    return textures;
  }

  static void apply_pbr_defaults(rl::Material &material) {
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

  static void apply_pbr_fallback_textures(rl::Material &material) {
    auto &fallbacks = pbr_fallback_textures();

    if (!has_texture(material.maps[MATERIAL_MAP_METALNESS].texture)) {
      material.maps[MATERIAL_MAP_METALNESS].texture = fallbacks.metallic_black;
    }
    if (!has_texture(material.maps[MATERIAL_MAP_ROUGHNESS].texture)) {
      material.maps[MATERIAL_MAP_ROUGHNESS].texture = fallbacks.roughness_white;
    }
    if (!has_texture(material.maps[MATERIAL_MAP_NORMAL].texture)) {
      material.maps[MATERIAL_MAP_NORMAL].texture = fallbacks.normal_flat;
    }
    if (!has_texture(material.maps[MATERIAL_MAP_OCCLUSION].texture)) {
      material.maps[MATERIAL_MAP_OCCLUSION].texture = fallbacks.occlusion_white;
    }
    if (!has_texture(material.maps[MATERIAL_MAP_EMISSION].texture)) {
      material.maps[MATERIAL_MAP_EMISSION].texture = fallbacks.emission_black;
    }
  }

  static rl::Vector3 color_to_vec3(rl::Color color) {
    return {color.r / 255.0f, color.g / 255.0f, color.b / 255.0f};
  }

  static RenderSceneState
  collect_render_scene(ActiveCameraRenderQuery cam_query,
                       DirectionalLightRenderQuery dir_query,
                       PointLightRenderQuery point_query) {
    RenderSceneState scene{};

    cam_query.each([&](const Camera3DComponent *cam_comp, const ActiveCamera *,
                       const GlobalTransform *cam_gt, const Skybox *sky,
                       const RenderConfig *rc) {
      if (!cam_comp || !cam_gt)
        return;
      scene.camera_view = compute_camera_view(*cam_gt, *cam_comp);
      scene.active_skybox = sky;
      if (rc) {
        scene.render_config = *rc;
        scene.has_render_config = true;
      }
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

  static void begin_world_pass(NonSendMarker, rl::Color clear_color,
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

    if (scene.has_render_config) {
      scene.render_config.apply();
    }

    if (scene.active_skybox && scene.active_skybox->loaded) {
      scene.active_skybox->draw(scene.camera_view.view, scene.camera_view.proj);
    }
  }

  static void end_world_pass(NonSendMarker) {
    rl::rlDrawRenderBatchActive();
    rl::rlMatrixMode(RL_PROJECTION);
    rl::rlPopMatrix();
    rl::rlMatrixMode(RL_MODELVIEW);
    rl::rlLoadIdentity();
    rl::rlDisableDepthTest();
    rl::rlDisableBackfaceCulling();
  }

  static const rl::Material *resolve_source_material(NonSendMarker marker,
                                                     const rl::Model &model,
                                                     int material_index) {
    if (model.materials && model.materialCount > 0 && material_index >= 0 &&
        material_index < model.materialCount) {
      const rl::Material *material = &model.materials[material_index];
      if (material->maps) {
        return material;
      }
    }
    return &fallback_material(marker);
  }

  static void prepare_draw_material(NonSendMarker marker,
                                    const RenderBatchKey &key,
                                    const rl::Model &model,
                                    const Assets<rl::Texture2D> &tex_assets,
                                    PreparedMaterial &prepared) {
    const rl::Material *src_mat =
        resolve_source_material(marker, model, key.material_index);
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
      if (auto *tex = tex_assets.get(
              Handle<rl::Texture2D>{key.material.roughness_id})) {
        prepared.material.maps[MATERIAL_MAP_ROUGHNESS].texture = *tex;
      }
    }

    if (key.material.occlusion_id != U32_MAX) {
      if (auto *tex = tex_assets.get(
              Handle<rl::Texture2D>{key.material.occlusion_id})) {
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

    apply_pbr_fallback_textures(prepared.material);
  }

  static void set_shader_value(NonSendMarker, rl::Shader &shader,
                               const char *name, const void *value, int type) {
    int loc = rl::GetShaderLocation(shader, name);
    if (loc >= 0) {
      rl::SetShaderValue(shader, loc, value, type);
    }
  }

  static void set_shader_value_v(NonSendMarker, rl::Shader &shader,
                                 const char *name, const void *value, int type,
                                 int count) {
    int loc = rl::GetShaderLocation(shader, name);
    if (loc >= 0) {
      rl::SetShaderValueV(shader, loc, value, type, count);
    }
  }

  static void apply_shader_uniforms(NonSendMarker marker, rl::Shader &shader,
                                    const PreparedMaterial &prepared,
                                    const RenderSceneState &scene) {
    float metallic = prepared.material.maps[MATERIAL_MAP_METALNESS].value;
    float roughness = prepared.material.maps[MATERIAL_MAP_ROUGHNESS].value;
    float normal_factor = prepared.material.maps[MATERIAL_MAP_NORMAL].value;
    float occlusion_factor =
        prepared.material.maps[MATERIAL_MAP_OCCLUSION].value;
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

    int has_directional = scene.has_directional ? 1 : 0;
    rl::Vector3 dir_color = color_to_vec3(scene.dir_light.color);
    rl::Vector3 dir_dir = to_rl(scene.dir_light.direction.Normalized());
    float dir_intensity = scene.dir_light.intensity;

    int point_count = static_cast<int>(scene.point_count);
    int spot_count = 0;

    set_shader_value(marker, shader, "u_metallic_factor", &metallic,
                     SHADER_UNIFORM_FLOAT);
    set_shader_value(marker, shader, "u_roughness_factor", &roughness,
                     SHADER_UNIFORM_FLOAT);
    set_shader_value(marker, shader, "u_normal_factor", &normal_factor,
                     SHADER_UNIFORM_FLOAT);
    set_shader_value(marker, shader, "u_occlusion_factor", &occlusion_factor,
                     SHADER_UNIFORM_FLOAT);
    set_shader_value(marker, shader, "u_emissive_factor", &emissive,
                     SHADER_UNIFORM_VEC3);
    set_shader_value(marker, shader, "u_color", &tint, SHADER_UNIFORM_VEC4);
    set_shader_value(marker, shader, "u_camera_pos", &camera_pos,
                     SHADER_UNIFORM_VEC3);

    set_shader_value(marker, shader, "u_has_directional", &has_directional,
                     SHADER_UNIFORM_INT);
    set_shader_value(marker, shader, "u_dir_light_direction", &dir_dir,
                     SHADER_UNIFORM_VEC3);
    set_shader_value(marker, shader, "u_dir_light_intensity", &dir_intensity,
                     SHADER_UNIFORM_FLOAT);
    set_shader_value(marker, shader, "u_dir_light_color", &dir_color,
                     SHADER_UNIFORM_VEC3);

    set_shader_value(marker, shader, "u_point_light_count", &point_count,
                     SHADER_UNIFORM_INT);
    if (scene.point_count > 0) {
      set_shader_value_v(marker, shader, "u_point_lights_position",
                         scene.point_positions.data(), SHADER_UNIFORM_VEC3,
                         point_count);
      set_shader_value_v(marker, shader, "u_point_lights_intensity",
                         scene.point_intensities.data(), SHADER_UNIFORM_FLOAT,
                         point_count);
      set_shader_value_v(marker, shader, "u_point_lights_color",
                         scene.point_colors.data(), SHADER_UNIFORM_VEC3,
                         point_count);
      set_shader_value_v(marker, shader, "u_point_lights_range",
                         scene.point_ranges.data(), SHADER_UNIFORM_FLOAT,
                         point_count);
    }

    set_shader_value(marker, shader, "u_spot_light_count", &spot_count,
                     SHADER_UNIFORM_INT);
  }

  static bool can_draw_instanced(rl::Material &material, const rl::Mesh &mesh) {
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

  static void draw_batch_instances(const rl::Mesh &mesh, rl::Material &material,
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

  static void draw_render_batch(NonSendMarker marker, const RenderBatch &batch,
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
    prepare_draw_material(marker, key, *model, tex_assets, prepared);

    apply_shader_uniforms(marker, prepared.material.shader, prepared, scene);

    draw_batch_instances(mesh, prepared.material, batch);
  }

  static float texture_rotation_degrees(const Transform &tx) {
    float siny_cosp = 2.0f * (tx.rotation.GetW() * tx.rotation.GetZ() +
                              tx.rotation.GetX() * tx.rotation.GetY());
    float cosy_cosp = 1.0f - 2.0f * (tx.rotation.GetY() * tx.rotation.GetY() +
                                     tx.rotation.GetZ() * tx.rotation.GetZ());
    return atan2f(siny_cosp, cosy_cosp) * (180.0f / 3.14159265f);
  }

  static void draw_screen_textures(TextureRenderQuery textures,
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
};

} // namespace ecs
