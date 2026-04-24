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

using namespace ecs;

// Test: New organized includes
TEST(GraphicsCompilation, NewOrganizedIncludes) {
  // Verify types are accessible through new include paths
  Visibility v = Visibility::Visible;
  Transform t;
  ClusteredLightingState cl;
  FrustumPlane fp;
  MaterialMap mm = MaterialMap::Albedo;

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

  // gl/mod.hpp should export shader bindings
  MaterialMap mm = MaterialMap::Albedo;

  // If we got here, all module exports are working
  SUCCEED();
}
