#include <gtest/gtest.h>

// Test: Verify new organized includes work

#include "rydz_graphics/spatial/frustum.hpp"
#include "rydz_graphics/spatial/visibility.hpp"
#include "rydz_graphics/spatial/transform.hpp"
#include "rydz_graphics/spatial/mod.hpp"

#include "rydz_graphics/lighting/clustered_lighting.hpp"
#include "rydz_graphics/lighting/compute.hpp"
#include "rydz_graphics/lighting/mod.hpp"

#include "rydz_graphics/gl/shader_bindings.hpp"
#include "rydz_graphics/gl/mod.hpp"
#include "rydz_graphics/components/environment.hpp"

using namespace ecs;

// Test: New organized includes
TEST(GraphicsCompilation, NewOrganizedIncludes) {
  // Verify types are accessible through new include paths
  Visibility v = Visibility::Visible;
  Transform t;
  ClusteredLightingState cl;
  FrustumPlane fp;
  MaterialMap mm = MaterialMap::Albedo;
  SpotLight sl;
  Environment env;

  // If we got here, all types are accessible
  SUCCEED();
}

// Test that module exports work correctly
TEST(GraphicsCompilation, ModuleExports) {
  // spatial/mod.hpp should export all spatial utilities
  Visibility v = Visibility::Visible;
  Transform t;
  FrustumPlane fp;

  // lighting/mod.hpp should export all lighting utilities
  ClusteredLightingState cl;
  SpotLight sl;
  Environment env;

  // gl/mod.hpp should export shader bindings
  MaterialMap mm = MaterialMap::Albedo;

  // If we got here, all module exports are working
  SUCCEED();
}

TEST(GraphicsCompilation, EnvironmentLightingBuilderWorksOnTemporaries) {
  auto env = Environment::from_color(Color::BLACK)
               .with_ambient_light(AmbientLight{
                 .color = Color::WHITE,
                 .intensity = 0.5f,
               })
               .with_directional_light(DirectionalLight{
                 .color = Color::WHITE,
                 .direction = Vec3(-1.0f, -1.0f, -1.0f).normalized(),
                 .intensity = 1.0f,
                 .casts_shadows = true,
               });

  EXPECT_FLOAT_EQ(env.ambient_light.intensity, 0.5f);
  EXPECT_FLOAT_EQ(env.directional_light.intensity, 1.0f);
}

TEST(GraphicsCompilation, EnvironmentDefaultsToNoAmbientContribution) {
  auto env = Environment::from_color(Color::BLACK);

  EXPECT_FLOAT_EQ(env.ambient_light.intensity, 0.1f);
}
