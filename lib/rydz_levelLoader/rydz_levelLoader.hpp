#pragma once
/*
 * rydz_camera - Isometric camera system for rydz ECS engine
 *
 * Provides IsometricCamera component, controller system, and setup helper.
 * Single header for top-down games and RPGs.
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

namespace ecs {

using namespace math;

struct RotateTag {};

struct brushFace {
  Vector3 v0;
  Vector3 v1;
  Vector3 v2;
  Vector3 v3;
  std::string texturePath;
};

struct entity_prop_static {
  Vec3 position = {0, 0, 0};
  std::string modelPath = "models/sun.glb";
};

Mesh createPlaneMesh(brushFace face);
std::vector<brushFace> parseVertices(std::string const& filename);

// Function to extract content between double quotes
std::string extractValue(std::string const& line) {
  size_t first = line.find('"');
  size_t second = line.find('"', first + 1);
  size_t third = line.find('"', second + 1);
  size_t fourth = line.find('"', third + 1);

  // We want the content of the second quoted string (the value)
  if (third != std::string::npos && fourth != std::string::npos) {
    return line.substr(third + 1, fourth - third - 1);
  }
  return "";
}

// Function to extract the key (the first quoted string)
std::string extractKey(std::string const& line) {
  size_t first = line.find('"');
  size_t second = line.find('"', first + 1);
  if (first != std::string::npos && second != std::string::npos) {
    return line.substr(first + 1, second - first - 1);
  }
  return "";
}

std::vector<entity_prop_static> parseMapForProps(std::string const& filePath) {
  std::ifstream file(filePath);
  std::vector<entity_prop_static> props;
  std::string line;

  while (std::getline(file, line)) {
    if (line.find('{') != std::string::npos) {
      entity_prop_static currentProp;
      bool isTargetEntity = false;
      bool insideBrush = false;

      std::string originStr;

      while (std::getline(file, line) && line.find('}') == std::string::npos) {
        // Skip brush blocks within entities
        if (line.find('{') != std::string::npos) {
          insideBrush = true;
          continue;
        }
        if (insideBrush) {
          if (line.find('}') != std::string::npos)
            insideBrush = false;
          continue;
        }

        std::string key = extractKey(line);
        std::string value = extractValue(line);

        if (key == "classname" && value == "prop_static") {
          isTargetEntity = true;
        } else if (key == "origin") {
          originStr = value; // Store the full "x y z" string
        } else if (key == "modelpath") {
          currentProp.modelPath = value;
        }
      }

      if (isTargetEntity) {
        // Now parse the full origin string into the Vec3
        std::stringstream ss(originStr);
        float x;
        float y;
        float z;
        ss >> x >> y >> z;
        currentProp.position = {x, z, -y};
        props.push_back(currentProp);
      }
    }
  }
  return props;
}

std::vector<brushFace> parseVertices(std::string const& filename) {
  std::vector<brushFace> allFaces;
  std::ifstream file(filename);

  if (!file.is_open()) {
    std::cerr << "Cannot read file: " << filename << std::endl;
    return allFaces;
  }

  std::string line;
  std::vector<Vector3> tempVertices;
  std::string lastFoundTexture = "";

  while (std::getline(file, line)) {

    size_t matPos = line.find("\"material\"");
    if (matPos != std::string::npos) {
      size_t firstQuote = line.find("\"", matPos + 10);
      size_t lastQuote = line.find("\"", firstQuote + 1);

      if (firstQuote != std::string::npos && lastQuote != std::string::npos) {
        std::string fullPath =
          line.substr(firstQuote + 1, lastQuote - firstQuote - 1);

        // lastFoundTexture = "empty";
        size_t lastSlash = fullPath.find_last_of("/\\");
        if (lastSlash != std::string::npos) {
          lastFoundTexture = fullPath.substr(lastSlash + 1);
        } else {
          lastFoundTexture = fullPath;
        }

        if (!allFaces.empty() && allFaces.back().texturePath.empty()) {
          allFaces.back().texturePath = lastFoundTexture;
        }
      }
    }

    size_t vPos = line.find("\"v\"");
    if (vPos != std::string::npos) {
      size_t firstQuote = line.find("\"", vPos + 3);
      size_t lastQuote = line.find("\"", firstQuote + 1);

      if (firstQuote != std::string::npos && lastQuote != std::string::npos) {
        std::string coordsStr =
          line.substr(firstQuote + 1, lastQuote - firstQuote - 1);

        std::stringstream ss(coordsStr);
        Vector3 v;
        if (ss >> v.x >> v.z >> v.y) {
          tempVertices.push_back(v);
        }
      }
    }

    if (tempVertices.size() == 4) {
      brushFace face;
      face.v0 = tempVertices[0];
      face.v1 = tempVertices[1];
      face.v2 = tempVertices[2];
      face.v3 = tempVertices[3];

      rl::TraceLog(
        LOG_DEBUG, (("TEXTURE TO APPLY: " + lastFoundTexture).c_str())
      );
      // face.texturePath = lastFoundTexture;

      rl::TraceLog(LOG_DEBUG, "LOADING NEW FACE");
      rl::TraceLog(
        LOG_DEBUG,
        ("X: " + std::to_string(face.v0.x) + " Y: " +
         std::to_string(face.v0.z) + " Z: " + std::to_string(face.v0.y))
          .c_str()
      );
      rl::TraceLog(
        LOG_DEBUG,
        ("X: " + std::to_string(face.v1.x) + " Y: " +
         std::to_string(face.v1.z) + " Z: " + std::to_string(face.v1.y))
          .c_str()
      );
      rl::TraceLog(
        LOG_DEBUG,
        ("X: " + std::to_string(face.v2.x) + " Y: " +
         std::to_string(face.v2.z) + " Z: " + std::to_string(face.v2.y))
          .c_str()
      );
      rl::TraceLog(
        LOG_DEBUG,
        ("X: " + std::to_string(face.v3.x) + " Y: " +
         std::to_string(face.v3.z) + " Z: " + std::to_string(face.v3.y))
          .c_str()
      );
      allFaces.push_back(face);

      tempVertices.clear();
      lastFoundTexture = "";
    }
  }

  file.close();
  return allFaces;
}

void spawn_model(Cmd cmd, Res<AssetServer> server) {
  // auto model_handle = server->load<Scene>("res/models/old_house.glb");
  cmd.spawn(
    SceneRoot{server->load<Scene>("res/levels/testLevel.glb")},
    ecs::Transform{.scale = {0.2f, 0.2f, 0.2f}}
  );
}

void spawn_entity_models(Cmd cmd, Res<AssetServer> server) {
  // auto model_handle = server->load<Scene>("res/models/old_house.glb");
  // entity_prop_static testProp;
  // testProp.modelPath = "sun.glb";
  // testProp.position = { 0.0f, 0.0f, 0.0f };

  std::vector<entity_prop_static> props_static =
    parseMapForProps("res/levels/testLevel.map");

  for (auto& prop_static : props_static) {
    cmd.spawn(
      SceneRoot{server->load<Scene>("res/" + prop_static.modelPath)},
      ecs::Transform{
        .translation = prop_static.position * 0.2f, .scale = {0.2f, 0.2f, 0.2f}
      }
    );
  }
}

// NonSendMarker musi byæ gdy funkcja musi byæ odpalona na g³ównym w¹tku
// (inn¹ opcj¹ jest dodanie do funkcji World world)
void setupScene(
  Cmd cmd,
  ResMut<Assets<Mesh>> meshes,
  NonSendMarker,
  ResMut<Assets<Material>> materials
) {

  // cube - czerwona
  auto cube_h = meshes->add(mesh::cube(16, 5, 16));
  auto material = materials->add(StandardMaterial::from_color(Color::RED));
  cmd.spawn(
    Mesh3d{cube_h},
    MeshMaterial3d{material},
    ecs::Transform::from_xyz(-24, -20, -24),
    RotateTag{}
  );
}

} // namespace ecs
