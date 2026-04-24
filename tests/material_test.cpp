#include <gtest/gtest.h>

#include "rydz_ecs/query.hpp"
#include "rydz_ecs/world.hpp"
#include "rydz_graphics/material/material3d.hpp"
#include "rydz_graphics/material/postprocess_material.hpp"
#include "rydz_graphics/material/render_material.hpp"
#include "rydz_graphics/material/standard_material.hpp"
#include "rydz_graphics/pipeline/pass_context.hpp"
#include "rydz_graphics/pipeline/phase.hpp"

namespace {

struct HasWind {
  using DependsOn = std::tuple<ecs::HasCamera>;
};

struct FancyMaterial : ecs::MaterialTrait<HasWind, ecs::HasPBR> {
  static ecs::ShaderRef vertex_shader() { return "vertex.glsl"; }
  static ecs::ShaderRef fragment_shader() { return "fragment.glsl"; }
};

struct DuplicateSlotMaterial
    : ecs::MaterialTrait<ecs::HasCamera, ecs::HasPBR, ecs::HasCamera> {};

struct ReservedUniformMaterial : ecs::MaterialTrait<ecs::HasCamera> {
  void bind(ecs::MaterialBuilder& builder) const {
    builder.uniform("u_camera_pos", ecs::Uniform{1.0f});
  }
};

struct TemporaryUniformNameMaterial : ecs::MaterialTrait<> {
  void bind(ecs::MaterialBuilder& builder) const {
    builder.uniform(std::string{"u_temporary_name"}, ecs::Uniform{2.0f});
  }
};

struct QueryMaterial : ecs::MaterialTrait<> {
  int value = 0;

  static ecs::ShaderRef vertex_shader() { return "query.vert"; }
  static ecs::ShaderRef fragment_shader() { return "query.frag"; }
  [[nodiscard]] auto render_method() const -> ecs::RenderMethod {
    return ecs::RenderMethod::Opaque;
  }
  [[nodiscard]] auto enable_shadows() const -> bool { return true; }
  auto bind(ecs::MaterialBuilder&) const -> void {}
};

struct OtherQueryMaterial : ecs::MaterialTrait<> {
  static ecs::ShaderRef vertex_shader() { return "other.vert"; }
  static ecs::ShaderRef fragment_shader() { return "other.frag"; }
  [[nodiscard]] auto render_method() const -> ecs::RenderMethod {
    return ecs::RenderMethod::Opaque;
  }
  [[nodiscard]] auto enable_shadows() const -> bool { return true; }
  auto bind(ecs::MaterialBuilder&) const -> void {}
};

} // namespace

TEST(MaterialTest, TraitMaterialExpandsDependenciesAndCanonicalizesSlots) {
  ecs::Material material = FancyMaterial{};

  ASSERT_EQ(material.compiled.slots.size(), 3u);
  EXPECT_EQ(material.compiled.slots[0].type, std::type_index(typeid(ecs::HasCamera)));
  EXPECT_LE(material.compiled.slots[1].debug_name, material.compiled.slots[2].debug_name);
  EXPECT_EQ(material.compiled.slots[1].type, std::type_index(typeid(HasWind)));
  EXPECT_EQ(material.compiled.slots[2].type, std::type_index(typeid(ecs::HasPBR)));
  EXPECT_EQ(material.compiled.shader.vertex_path, "vertex.glsl");
  EXPECT_EQ(material.compiled.shader.fragment_path, "fragment.glsl");
  EXPECT_FALSE(material.compiled.material_type_name.empty());
}

TEST(MaterialTest, TraitMaterialDeduplicatesExpandedSlots) {
  ecs::Material material = DuplicateSlotMaterial{};

  ASSERT_EQ(material.compiled.slots.size(), 2u);
  EXPECT_EQ(material.compiled.slots[0].type, std::type_index(typeid(ecs::HasCamera)));
  EXPECT_EQ(material.compiled.slots[1].type, std::type_index(typeid(ecs::HasPBR)));
}

TEST(MaterialTest, MaterialMapDefaultsFillMissingUniforms) {
  ecs::Material material = FancyMaterial{};

  auto const& uniforms = material.compiled.uniforms;

  EXPECT_EQ(uniforms.at("u_color"), ecs::Uniform(1.0f, 1.0f, 1.0f, 0.0f));
  EXPECT_EQ(uniforms.at("u_metallic_factor"), ecs::Uniform{0.0f});
  EXPECT_EQ(uniforms.at("u_normal_factor"), ecs::Uniform{1.0f});
  EXPECT_EQ(uniforms.at("u_roughness_factor"), ecs::Uniform{1.0f});
  EXPECT_EQ(uniforms.at("u_occlusion_factor"), ecs::Uniform{1.0f});
  EXPECT_EQ(uniforms.at("u_emissive_factor"), ecs::Uniform(0.0f, 0.0f, 0.0f));
}

TEST(MaterialTest, AuthoredMaterialMapUniformOverridesDefault) {
  ecs::StandardMaterial material_def;
  material_def.roughness = 0.25f;
  material_def.occlusion_strength = 0.5f;
  material_def.emissive_color = {255, 128, 0, 255};

  ecs::Material material = material_def;
  auto const& uniforms = material.compiled.uniforms;

  EXPECT_EQ(uniforms.at("u_roughness_factor"), ecs::Uniform{0.25f});
  EXPECT_EQ(uniforms.at("u_occlusion_factor"), ecs::Uniform{0.5f});
  EXPECT_EQ(
    uniforms.at("u_emissive_factor"),
    ecs::Uniform(1.0f, 128.0f / 255.0f, 0.0f)
  );
  EXPECT_EQ(uniforms.at("u_metallic_factor"), ecs::Uniform{0.0f});
}

TEST(MaterialTest, PrepareMaterialAppliesFallbackTexturesForDefaultedMaps) {
  ecs::CompiledMaterial material;
  ecs::Assets<ecs::Texture> textures;
  ecs::ShaderCache shader_cache;
  ecs::SlotProviderRegistry registry;
  ecs::PreparedMaterial prepared;
  gl::RenderState render_state;
  ecs::ExtractedView view{};
  ecs::ExtractedLights lights{};
  ecs::ExtractedShadows shadows{};
  ecs::Time time{};
  ecs::ExtractedMeshes extracted_meshes{};
  ecs::Assets<ecs::Mesh> mesh_assets;
  ecs::OpaquePhase opaque_phase{};
  ecs::TransparentPhase transparent_phase{};
  ecs::ShadowPhase shadow_phase{};
  ecs::UiPhase ui_phase{};
  gl::ClusterConfig cluster_config{};
  ecs::ShadowSettings shadow_settings{};
  ecs::ShadowResources shadow_resources{};
  gl::ClusteredLightingState cluster_state{};
  ecs::ViewUniformState view_uniforms{};
  ecs::ShadowUniformState shadow_uniforms{};
  ecs::PassContext ctx{
    .marker = ecs::NonSendMarker{},
    .render_state = render_state,
    .framebuffer = {},
    .view = view,
    .lights = lights,
    .shadows = shadows,
    .time = time,
    .extracted_meshes = extracted_meshes,
    .mesh_assets = mesh_assets,
    .texture_assets = textures,
    .shader_cache = shader_cache,
    .view_uniforms = view_uniforms,
    .shadow_uniforms = shadow_uniforms,
    .slot_registry = registry,
    .opaque_phase = opaque_phase,
    .transparent_phase = transparent_phase,
    .shadow_phase = shadow_phase,
    .ui_phase = ui_phase,
    .cluster_config = cluster_config,
    .shadow_settings = shadow_settings,
    .cluster_state = cluster_state,
    .shadow_resources = shadow_resources,
  };
  ecs::MaterialContext material_ctx{.frame_data = &ctx};

  ecs::prepare_material(
    material_ctx,
    material,
    ecs::ShaderSpec{},
    prepared
  );

  auto const* metallic = ecs::fallback_texture(ecs::MaterialMap::Metalness);
  auto const* normal = ecs::fallback_texture(ecs::MaterialMap::Normal);
  auto const* roughness = ecs::fallback_texture(ecs::MaterialMap::Roughness);
  auto const* occlusion = ecs::fallback_texture(ecs::MaterialMap::Occlusion);
  auto const* emission = ecs::fallback_texture(ecs::MaterialMap::Emission);

  ASSERT_NE(metallic, nullptr);
  ASSERT_NE(normal, nullptr);
  ASSERT_NE(roughness, nullptr);
  ASSERT_NE(occlusion, nullptr);
  ASSERT_NE(emission, nullptr);

  EXPECT_EQ(
    prepared.material.maps[ecs::material_map_index(ecs::MaterialMap::Metalness)]
      .texture.id,
    metallic->id
  );
  EXPECT_EQ(
    prepared.material.maps[ecs::material_map_index(ecs::MaterialMap::Normal)]
      .texture.id,
    normal->id
  );
  EXPECT_EQ(
    prepared.material.maps[ecs::material_map_index(ecs::MaterialMap::Roughness)]
      .texture.id,
    roughness->id
  );
  EXPECT_EQ(
    prepared.material.maps[ecs::material_map_index(ecs::MaterialMap::Occlusion)]
      .texture.id,
    occlusion->id
  );
  EXPECT_EQ(
    prepared.material.maps[ecs::material_map_index(ecs::MaterialMap::Emission)]
      .texture.id,
    emission->id
  );
}

TEST(MaterialTest, PrepareMaterialPreservesAuthoredTextureOverFallback) {
  ecs::Assets<ecs::Texture> textures;
  ecs::Texture texture;
  texture.id = 42;
  texture.width = 1;
  texture.height = 1;
  texture.mipmaps = 1;
  texture.format = gl::PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
  auto const roughness_texture = textures.add(texture);

  ecs::CompiledMaterial material;
  material.maps.push_back(
    ecs::MaterialMapBinding::texture_binding(
      ecs::MaterialMap::Roughness, roughness_texture
    )
  );

  ecs::ShaderCache shader_cache;
  ecs::SlotProviderRegistry registry;
  ecs::PreparedMaterial prepared;
  gl::RenderState render_state;
  ecs::ExtractedView view{};
  ecs::ExtractedLights lights{};
  ecs::ExtractedShadows shadows{};
  ecs::Time time{};
  ecs::ExtractedMeshes extracted_meshes{};
  ecs::Assets<ecs::Mesh> mesh_assets;
  ecs::OpaquePhase opaque_phase{};
  ecs::TransparentPhase transparent_phase{};
  ecs::ShadowPhase shadow_phase{};
  ecs::UiPhase ui_phase{};
  gl::ClusterConfig cluster_config{};
  ecs::ShadowSettings shadow_settings{};
  ecs::ShadowResources shadow_resources{};
  gl::ClusteredLightingState cluster_state{};
  ecs::ViewUniformState view_uniforms{};
  ecs::ShadowUniformState shadow_uniforms{};
  ecs::PassContext ctx{
    .marker = ecs::NonSendMarker{},
    .render_state = render_state,
    .framebuffer = {},
    .view = view,
    .lights = lights,
    .shadows = shadows,
    .time = time,
    .extracted_meshes = extracted_meshes,
    .mesh_assets = mesh_assets,
    .texture_assets = textures,
    .shader_cache = shader_cache,
    .view_uniforms = view_uniforms,
    .shadow_uniforms = shadow_uniforms,
    .slot_registry = registry,
    .opaque_phase = opaque_phase,
    .transparent_phase = transparent_phase,
    .shadow_phase = shadow_phase,
    .ui_phase = ui_phase,
    .cluster_config = cluster_config,
    .shadow_settings = shadow_settings,
    .cluster_state = cluster_state,
    .shadow_resources = shadow_resources,
  };
  ecs::MaterialContext material_ctx{.frame_data = &ctx};

  ecs::prepare_material(
    material_ctx,
    material,
    ecs::ShaderSpec{},
    prepared
  );

  EXPECT_EQ(
    prepared.material.maps[ecs::material_map_index(ecs::MaterialMap::Roughness)]
      .texture.id,
    42u
  );
}

TEST(MaterialTest, ReservedUniformFailsCompilation) {
  EXPECT_THROW((void) ecs::Material{ReservedUniformMaterial{}}, std::runtime_error);
}

TEST(MaterialTest, UniformNamesAreOwnedAfterCompilation) {
  ecs::Material material = TemporaryUniformNameMaterial{};

  auto it = material.compiled.uniforms.find("u_temporary_name");
  ASSERT_NE(it, material.compiled.uniforms.end());
  EXPECT_EQ(it->second, ecs::Uniform{2.0f});
}

TEST(MaterialTest, MeshMaterial3dQueryFiltersByMaterialType) {
  ecs::World world;
  ecs::Assets<QueryMaterial> materials;
  auto material = materials.add(QueryMaterial{.value = 42});
  world.insert_resource(std::move(materials));

  auto matching_entity = world.spawn();
  auto other_entity = world.spawn();
  world.insert_component(matching_entity, ecs::MeshMaterial3d<QueryMaterial>{material});
  world.insert_component(
    other_entity,
    ecs::MeshMaterial3d<OtherQueryMaterial>{ecs::Handle<OtherQueryMaterial>{0}}
  );

  int count = 0;
  ecs::Query<ecs::Entity, ecs::MeshMaterial3d<QueryMaterial>> query(
    world, ecs::Tick{0}, world.read_change_tick()
  );
  for (auto [entity, material_handle] : query.iter()) {
    ++count;
    EXPECT_EQ(entity, matching_entity);
    EXPECT_EQ(material_handle->material, material);
  }

  EXPECT_EQ(count, 1);

  auto const* stored_materials = world.get_resource<ecs::Assets<QueryMaterial>>();
  ASSERT_NE(stored_materials, nullptr);
  auto const* stored_material = stored_materials->get(material);
  ASSERT_NE(stored_material, nullptr);
  EXPECT_EQ(stored_material->value, 42);
}

TEST(MaterialTest, PostProcessUniformNamesAreOwned) {
  ecs::PostProcessDescriptor descriptor;
  descriptor._uniforms.insert_or_assign(
    std::string{"u_temporary_post"}, ecs::Uniform{3.0f}
  );

  auto it = descriptor._uniforms.find("u_temporary_post");
  ASSERT_NE(it, descriptor._uniforms.end());
  EXPECT_EQ(it->second, ecs::Uniform{3.0f});
}

TEST(MaterialTest, DoubleSidedMaterialDisablesCulling) {
  ecs::CompiledMaterial material;

  EXPECT_EQ(material.cull_state(), gl::Cull::back());

  material.double_sided = true;
  EXPECT_EQ(material.cull_state(), gl::Cull::none());
}

TEST(MaterialTest, RenderConfigsBuildExpectedStateValues) {
  auto default_config = ecs::RenderConfig::get_default();
  EXPECT_EQ(default_config.blend, gl::Blend::alpha());

  auto opaque = ecs::RenderConfig::opaque();
  EXPECT_EQ(opaque.blend, gl::Blend::disabled());
  EXPECT_EQ(opaque.depth, (gl::Depth{.test = true, .write = true}));
  EXPECT_EQ(opaque.cull, gl::Cull::back());

  auto transparent = ecs::RenderConfig::transparent();
  EXPECT_EQ(transparent.blend, gl::Blend::alpha());
  EXPECT_EQ(transparent.depth, (gl::Depth{.test = true, .write = false}));

  auto depth_prepass = ecs::RenderConfig::depth_prepass();
  EXPECT_EQ(
    depth_prepass.color_mask,
    (gl::ColorMask{.r = false, .g = false, .b = false, .a = false})
  );
}
