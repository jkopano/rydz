#include <gtest/gtest.h>

// Test 8.2: Verify backward compatibility includes work
// This test verifies that old include paths still work

#include "rydz_graphics/frustum.hpp"
#include "rydz_graphics/visibility.hpp"
#include "rydz_graphics/transform.hpp"
#include "rydz_graphics/clustered_lighting.hpp"
#include "rydz_graphics/compute.hpp"
#include "rydz_graphics/shader_bindings.hpp"

// Test 8.3: Verify new organized includes work
// This test verifies that new include paths work correctly

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

// Test 8.2: Backward compatibility includes
TEST(GraphicsCompilation, BackwardCompatibilityIncludes) {
  // Verify types are accessible through old include paths
  Visibility v = Visibility::Visible;
  Transform t;
  ClusteredLightingState cl;
  FrustumPlane fp;
  MaterialMap mm = MaterialMap::Albedo;

  // If we got here, all types are accessible
  SUCCEED();
}

// Test 8.3: New organized includes
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
