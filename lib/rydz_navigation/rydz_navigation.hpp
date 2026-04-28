#pragma once
/*
 * rydz_navigation - Navigation and pathing system
 */

#include "math.hpp"
#include "rl.hpp"
#include "rydz_ecs/asset.hpp"
#include "rydz_ecs/fwd.hpp"
#include "rydz_ecs/mod.hpp"
#include "rydz_ecs/params.hpp"
#include "rydz_ecs/query.hpp"
#include "rydz_ecs/schedule.hpp"
#include "rydz_ecs/storage.hpp"
#include "rydz_graphics/material/standard_material.hpp"
#include "rydz_graphics/mod.hpp"
#include "rydz_graphics/render_plugin.hpp"
#include "rydz_graphics/transform.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

#define MAX_PATH_NODE_CONNECTIONS 9

namespace ecs {

using namespace math;

struct RotateTag {};

struct PathNode {
  Vector3 position;
  int connections[MAX_PATH_NODE_CONNECTIONS];
  int connectionCount;
};

struct AStarNode {
  int nodeIndex;
  float estimatedCostToGoal;
  float costFromStart;
  int pathIndex;
  struct AStarNode* previousNode;
};

Mesh createPlaneMesh(brushFace face);
std::vector<brushFace> parseVertices(std::string const& filename);


void setup_navigation(Cmd cmd, Res<AssetServer> server) {
  // auto model_handle = server->load<Scene>("res/models/old_house.glb");
  cmd.spawn(
    SceneRoot{server->load<Scene>("res/levels/testLevel.glb")},
    ecs::Transform{.scale = {0.1f, 0.1f, 0.1f}}
  );
}

void createGrid(float distanceBetweenEdges, int gridResolution) {
  float distBetweenNodes = distanceBetweenEdges / gridResolution;
  float margin = distanceBetweenEdges * 0.025f;

  for (int i = 0; i < gridResolution; i++) 
  {
    for (int j = 0; j < gridResolution; j++) 
    {
        Vector3 position
    }
  }
}

void debugNodes(
}

} // namespace ecs
