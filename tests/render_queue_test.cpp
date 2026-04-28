#include <gtest/gtest.h>

#include "rydz_graphics/extract/data.hpp"
#include "rydz_graphics/extract/systems.hpp"

using namespace ecs;

namespace {

auto make_material_key(u32 id) -> RenderMaterialKey {
  return RenderMaterialKey{
    .asset_type = std::type_index(typeid(Material)),
    .id = id,
  };
}

auto make_compiled_material(char const* vertex, char const* fragment) -> CompiledMaterial {
  CompiledMaterial material;
  material.shader = ShaderSpec::from(vertex, fragment);
  return material;
}

} // namespace

TEST(RenderQueueTest, OpaqueBatchesInstancesByMeshAndMaterial) {
  ExtractedMeshes meshes;
  OpaquePhase phase;
  auto const material_key = make_material_key(7);

  meshes.materials.push_back(
    ExtractedMeshes::MaterialItem{
      .key = material_key,
      .material = make_compiled_material("mesh.vert", "mesh.frag"),
      .transparent = false,
    }
  );
  meshes.items.push_back(
    ExtractedMeshes::Item{
      .mesh = Handle<Mesh>{1},
      .material = material_key,
      .material_index = 0,
      .distance_sq_to_camera = 5.0F,
    }
  );
  meshes.items.push_back(
    ExtractedMeshes::Item{
      .mesh = Handle<Mesh>{1},
      .material = material_key,
      .material_index = 0,
      .distance_sq_to_camera = 2.0F,
    }
  );

  Queue::opaque(Res<ExtractedMeshes>{&meshes}, ResMut<OpaquePhase>{&phase});

  ASSERT_EQ(phase.commands.size(), 1u);
  EXPECT_EQ(phase.commands[0].mesh.id, 1u);
  EXPECT_EQ(phase.commands[0].material_index, 0u);
  EXPECT_EQ(meshes.materials[phase.commands[0].material_index].key.id, 7u);
  EXPECT_EQ(phase.commands[0].instances.size(), 2u);
}

TEST(RenderQueueTest, OpaqueSortsByShaderThenMaterialThenMesh) {
  ExtractedMeshes meshes;
  OpaquePhase phase;

  meshes.materials.push_back(
    ExtractedMeshes::MaterialItem{
      .key = make_material_key(2),
      .material = make_compiled_material("z.vert", "z.frag"),
      .transparent = false,
    }
  );
  meshes.materials.push_back(
    ExtractedMeshes::MaterialItem{
      .key = make_material_key(1),
      .material = make_compiled_material("a.vert", "a.frag"),
      .transparent = false,
    }
  );
  meshes.materials.push_back(
    ExtractedMeshes::MaterialItem{
      .key = make_material_key(3),
      .material = make_compiled_material("a.vert", "a.frag"),
      .transparent = false,
    }
  );

  meshes.items.push_back(
    ExtractedMeshes::Item{
      .mesh = Handle<Mesh>{30},
      .material = meshes.materials[0].key,
      .material_index = 0,
    }
  );
  meshes.items.push_back(
    ExtractedMeshes::Item{
      .mesh = Handle<Mesh>{20},
      .material = meshes.materials[2].key,
      .material_index = 2,
    }
  );
  meshes.items.push_back(
    ExtractedMeshes::Item{
      .mesh = Handle<Mesh>{10},
      .material = meshes.materials[1].key,
      .material_index = 1,
    }
  );

  Queue::opaque(Res<ExtractedMeshes>{&meshes}, ResMut<OpaquePhase>{&phase});

  ASSERT_EQ(phase.commands.size(), 3u);
  EXPECT_EQ(
    meshes.materials[phase.commands[0].material_index].material.shader.vertex_path,
    "a.vert"
  );
  EXPECT_EQ(meshes.materials[phase.commands[0].material_index].key.id, 1u);
  EXPECT_EQ(phase.commands[0].mesh.id, 10u);
  EXPECT_EQ(
    meshes.materials[phase.commands[1].material_index].material.shader.vertex_path,
    "a.vert"
  );
  EXPECT_EQ(meshes.materials[phase.commands[1].material_index].key.id, 3u);
  EXPECT_EQ(
    meshes.materials[phase.commands[2].material_index].material.shader.vertex_path,
    "z.vert"
  );
  EXPECT_EQ(meshes.materials[phase.commands[2].material_index].key.id, 2u);
}

TEST(RenderQueueTest, TransparentKeepsDistanceSortingAndDoesNotBatch) {
  ExtractedMeshes meshes;
  TransparentPhase phase;
  auto const material_key = make_material_key(5);

  meshes.materials.push_back(
    ExtractedMeshes::MaterialItem{
      .key = material_key,
      .material = make_compiled_material("transparent.vert", "transparent.frag"),
      .transparent = true,
    }
  );
  meshes.items.push_back(
    ExtractedMeshes::Item{
      .mesh = Handle<Mesh>{1},
      .material = material_key,
      .material_index = 0,
      .distance_sq_to_camera = 1.0F,
    }
  );
  meshes.items.push_back(
    ExtractedMeshes::Item{
      .mesh = Handle<Mesh>{1},
      .material = material_key,
      .material_index = 0,
      .distance_sq_to_camera = 9.0F,
    }
  );

  Queue::transparent(Res<ExtractedMeshes>{&meshes}, ResMut<TransparentPhase>{&phase});

  ASSERT_EQ(phase.commands.size(), 2u);
  EXPECT_EQ(phase.commands[0].sort_key, 9.0F);
  EXPECT_EQ(phase.commands[1].sort_key, 1.0F);
}
