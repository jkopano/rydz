#include <gtest/gtest.h>

#include "rydz_graphics/material/material3d.hpp"

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
  void bind(ecs::MaterialBuilder &builder) const {
    builder.uniform("u_camera_pos", ecs::Uniform{1.0f});
  }
};

} // namespace

TEST(MaterialTest, TraitMaterialExpandsDependenciesAndCanonicalizesSlots) {
  ecs::Material material = FancyMaterial{};

  ASSERT_EQ(material.compiled.slots.size(), 3u);
  EXPECT_EQ(material.compiled.slots[0].type,
            std::type_index(typeid(ecs::HasCamera)));
  EXPECT_LE(material.compiled.slots[1].debug_name,
            material.compiled.slots[2].debug_name);
  EXPECT_EQ(material.compiled.slots[1].type,
            std::type_index(typeid(HasWind)));
  EXPECT_EQ(material.compiled.slots[2].type,
            std::type_index(typeid(ecs::HasPBR)));
  EXPECT_EQ(material.compiled.shader.vertex_path, "vertex.glsl");
  EXPECT_EQ(material.compiled.shader.fragment_path, "fragment.glsl");
  EXPECT_FALSE(material.compiled.material_type_name.empty());
}

TEST(MaterialTest, TraitMaterialDeduplicatesExpandedSlots) {
  ecs::Material material = DuplicateSlotMaterial{};

  ASSERT_EQ(material.compiled.slots.size(), 2u);
  EXPECT_EQ(material.compiled.slots[0].type,
            std::type_index(typeid(ecs::HasCamera)));
  EXPECT_EQ(material.compiled.slots[1].type,
            std::type_index(typeid(ecs::HasPBR)));
}

TEST(MaterialTest, ReservedUniformFailsCompilation) {
  EXPECT_THROW((void)ecs::Material{ReservedUniformMaterial{}},
               std::runtime_error);
}
