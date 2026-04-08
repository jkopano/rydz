#pragma once
/*
 * rydz_camera - Isometric camera system for rydz ECS engine
 *
 * Provides IsometricCamera component, controller system, and setup helper.
 * Single header for top-down games and RPGs.
 */

#include "math.hpp"
#include "rl.hpp"
#include "rydz_ecs/fwd.hpp"
#include "rydz_ecs/params.hpp"
#include "rydz_ecs/query.hpp"
#include "rydz_ecs/rydz_ecs.hpp"
#include "rydz_ecs/schedule.hpp"
#include "rydz_ecs/storage.hpp"
#include "rydz_graphics/render_plugin.hpp"
#include "rydz_graphics/rydz_graphics.hpp"
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

    rl::Mesh createPlaneMesh(brushFace face);
    std::vector<brushFace> parseVertices(const std::string& filename);



    inline void load_level(Cmd cmd, ResMut<Assets<rl::Mesh>> meshes,
        ResMut<Assets<rl::Texture2D>> textures,
        NonSendMarker) {


        std::vector<brushFace> brushFaces = parseVertices("res/levels/testLevel.vmf");



        //Spawn each face
        for (auto& face : brushFaces)
        {
            //rl::TraceLog(LOG_DEBUG, "RENDERING NEW FACE");
            //rl::TraceLog(LOG_DEBUG, ("X: " + std::to_string(face.v0.x) + " Y: " + std::to_string(face.v0.z) + " Z: " + std::to_string(face.v0.y)).c_str());
            //rl::TraceLog(LOG_DEBUG, ("X: " + std::to_string(face.v1.x) + " Y: " + std::to_string(face.v1.z) + " Z: " + std::to_string(face.v1.y)).c_str());
            //rl::TraceLog(LOG_DEBUG, ("X: " + std::to_string(face.v2.x) + " Y: " + std::to_string(face.v2.z) + " Z: " + std::to_string(face.v2.y)).c_str());
            //rl::TraceLog(LOG_DEBUG, ("X: " + std::to_string(face.v3.x) + " Y: " + std::to_string(face.v3.z) + " Z: " + std::to_string(face.v3.y)).c_str());

            //OLD MODEL LOADING
            //rl::Mesh cube_mesh = createPlaneMesh(face);
            //rl::Model plane_model = rl::LoadModelFromMesh(cube_mesh);
            //auto plane_h = models->add(std::move(plane_model));

            rl::Mesh cube_mesh = createPlaneMesh(face);
            auto plane_h = meshes->add(cube_mesh);

            std::ifstream f(("res/textures/" + face.texturePath + ".png").c_str());
            if (!f.good())
            {
                face.texturePath = "empty";
            }
            cmd.spawn(Mesh3d{ plane_h },
                MeshMaterial3d{ StandardMaterial::from_texture(
                    textures->add(rl::LoadTexture(("res/textures/" + face.texturePath + ".png").c_str()))) },

                Transform{ });
            //Transform{ (JPH::Vec3) { 5.0f, 0.0f, 10.0f } }
        }

        //rl::Mesh cube_mesh = rl::GenMeshCube(10.0f, 10.0f, 10.0f);
        //rl::Model plane_model = rl::LoadModelFromMesh(cube_mesh);
        //auto plane_h = models->add(std::move(plane_model));

        //cmd.spawn(Model3d{ plane_h },
        //    Material3d{ StandardMaterial::from_texture(
        //        textures->add(rl::LoadTexture("res/textures/texture_04_2.png"))) },
        //    Transform{ (JPH::Vec3) { 0.0f, 5.0f, -10.0f } });



    }


    std::vector<brushFace> parseVertices(const std::string& filename) {
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
                    std::string fullPath = line.substr(firstQuote + 1, lastQuote - firstQuote - 1);

                    //lastFoundTexture = "empty";
                    size_t lastSlash = fullPath.find_last_of("/\\");
                    if (lastSlash != std::string::npos) {
                        lastFoundTexture = fullPath.substr(lastSlash + 1);
                    }
                    else {
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
                    std::string coordsStr = line.substr(firstQuote + 1, lastQuote - firstQuote - 1);

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

                rl::TraceLog(LOG_DEBUG, (("TEXTURE TO APPLY: " + lastFoundTexture).c_str()));
                //face.texturePath = lastFoundTexture;

                rl::TraceLog(LOG_DEBUG, "LOADING NEW FACE");
                rl::TraceLog(LOG_DEBUG, ("X: " + std::to_string(face.v0.x) + " Y: " + std::to_string(face.v0.z) + " Z: " + std::to_string(face.v0.y)).c_str());
                rl::TraceLog(LOG_DEBUG, ("X: " + std::to_string(face.v1.x) + " Y: " + std::to_string(face.v1.z) + " Z: " + std::to_string(face.v1.y)).c_str());
                rl::TraceLog(LOG_DEBUG, ("X: " + std::to_string(face.v2.x) + " Y: " + std::to_string(face.v2.z) + " Z: " + std::to_string(face.v2.y)).c_str());
                rl::TraceLog(LOG_DEBUG, ("X: " + std::to_string(face.v3.x) + " Y: " + std::to_string(face.v3.z) + " Z: " + std::to_string(face.v3.y)).c_str());
                allFaces.push_back(face);

                tempVertices.clear();
                lastFoundTexture = "";
            }
        }

        file.close();
        return allFaces;
    }

    // Asset Server - ³adowanie modeli GLTF z plików
    void load_obj_model(Cmd cmd, Res<AssetServer> asset_server, NonSendMarker) {
        // load() zwraca Handle<Scene> od razu — scena ³aduje siê w tle
        // jak plik nie istnieje to nic siê nie renderuje ale bez crasha
        auto model_handle = asset_server->load<Scene>("res/models/old_house.glb");

        cmd.spawn(SceneRoot{ model_handle },
            ecs::Transform{ .translation = Vec3(0, -10.0f, 0),
                           .scale = Vec3::sReplicate(1.0f) });
    }

    void load_gltf_model(Cmd cmd, Res<AssetServer> asset_server, NonSendMarker) {
        auto model_handle = asset_server->load<Scene>("res/models/old_house.glb");

        cmd.spawn(SceneRoot{ model_handle },
            ecs::Transform{ });
    }

    // NonSendMarker musi byæ gdy funkcja musi byæ odpalona na g³ównym w¹tku
    // (inn¹ opcj¹ jest dodanie do funkcji World world)
    void setupScene(Cmd cmd, ResMut<Assets<rl::Mesh>> meshes, NonSendMarker) {

        // cube - czerwona
        auto cube_h = meshes->add(mesh::cube(2, 2, 2));
        cmd.spawn(Mesh3d{ cube_h }, MeshMaterial3d<>{StandardMaterial::from_color(RED)},
            ecs::Transform::from_xyz(-4, 1, 0), RotateTag{});

        // kula - zielona
        auto sphere_h = meshes->add(mesh::sphere(1.0f));
        cmd.spawn(
            Mesh3d{ sphere_h }, MeshMaterial3d<>{StandardMaterial::from_color(GREEN)},
            ecs::Transform::from_xyz(0, 1, 0));

        // cylinder - niebieski
        auto cyl_h = meshes->add(mesh::cylinder(0.8f, 2.0f));
        cmd.spawn(Mesh3d{ cyl_h },
            MeshMaterial3d<>{StandardMaterial::from_color(BLUE)},
            ecs::Transform::from_xyz(4, 1, 0));

        // floor
        auto floor_h = meshes->add(mesh::plane(20, 20));
        cmd.spawn(Mesh3d{ floor_h },
            MeshMaterial3d<>{StandardMaterial::from_color(DARKGRAY)},
            ecs::Transform::from_xyz(0, 0, 0));

        // torus
        auto torus_h = meshes->add(mesh::torus(1.0f, 0.3f));
        cmd.spawn(
            Mesh3d{ torus_h }, MeshMaterial3d<>{StandardMaterial::from_color(PURPLE)},
            ecs::Transform::from_xyz(25, 3, -4), RotateTag{});

        // œwiat³o ¿eby coœ by³o widaæ
        cmd.spawn(DirectionalLight{
            .color = RED,
            .direction = Vec3(-0.3f, -1.0f, -0.5f),
            .intensity = 0.5f,
            });
    }

    //TODO:
    //Zoptymalizowaæ liczenie trójk¹tów poprzez indeksowanie (chwilowo duplikuj¹ siê 2 vertexy per plane)

    rl::Mesh createPlaneMesh(brushFace face) {
        Mesh mesh = { 0 };

        rl::TraceLog(LOG_DEBUG, ("X: " + std::to_string(face.v0.x) + " Y: " + std::to_string(face.v0.y) + " Z: " + std::to_string(face.v0.z)).c_str());

        //Raylib robi tutaj kalkulacje z rozdzielczoœci (do subdivision), ale my chyba nie potrzebujemy
        int vertexCount = 6;

        // Vertices definition
        Vector3* vertices = (Vector3*)RL_MALLOC(vertexCount * sizeof(Vector3));
        vertices[0] = face.v0;
        vertices[1] = face.v1;
        vertices[2] = face.v2;
        vertices[3] = face.v3;
        vertices[4] = vertices[0];
        vertices[5] = vertices[2];

        // Normals definition
        Vector3* normals = (Vector3*)RL_MALLOC(vertexCount * sizeof(Vector3));
        //for (int n = 0; n < vertexCount; n++) normals[n] = (Vector3){ 0.0f, 1.0f, 0.0f };   // Vector3.up;
        for (int n = 0; n < vertexCount; n++) normals[n] = Vector3CrossProduct((vertices[1] - vertices[0]), (vertices[2] - vertices[0]));   // Vector3.up;

        // TexCoords definition
        Vector2* texcoords = (Vector2*)RL_MALLOC(vertexCount * sizeof(Vector2));
        texcoords[0] = { -1.0f, 1.0f };
        texcoords[1] = { 1.0f, 1.0f };
        texcoords[2] = { 1.0f, -1.0f };
        texcoords[3] = { -1.0f, -1.0f };
        texcoords[4] = texcoords[0];
        texcoords[5] = texcoords[2];

        mesh.vertexCount = vertexCount;
        mesh.triangleCount = 2;
        mesh.vertices = (float*)RL_MALLOC(mesh.vertexCount * 3 * sizeof(float));
        mesh.texcoords = (float*)RL_MALLOC(mesh.vertexCount * 2 * sizeof(float));
        mesh.normals = (float*)RL_MALLOC(mesh.vertexCount * 3 * sizeof(float));
        //mesh.indices = (unsigned short*)RL_MALLOC(mesh.triangleCount * 3 * sizeof(unsigned short));

        // Mesh vertices position array
        for (int i = 0; i < mesh.vertexCount; i++)
        {
            mesh.vertices[3 * i] = vertices[i].x;
            mesh.vertices[3 * i + 1] = vertices[i].y;
            mesh.vertices[3 * i + 2] = vertices[i].z;
        }

        // Mesh texcoords array
        for (int i = 0; i < mesh.vertexCount; i++)
        {
            mesh.texcoords[2 * i] = texcoords[i].x;
            mesh.texcoords[2 * i + 1] = texcoords[i].y;
        }

        // Mesh normals array
        for (int i = 0; i < mesh.vertexCount; i++)
        {
            mesh.normals[3 * i] = normals[i].x;
            mesh.normals[3 * i + 1] = normals[i].y;
            mesh.normals[3 * i + 2] = normals[i].z;
        }

        // Mesh indices array initialization
        //for (int i = 0; i < mesh.triangleCount * 3; i++) mesh.indices[i] = triangles[i];

        RL_FREE(vertices);
        RL_FREE(normals);
        RL_FREE(texcoords);
        //RL_FREE(triangles);

        // Upload vertex data to GPU (static mesh)
        UploadMesh(&mesh, false);

        return mesh;
    }


} // namespace ecs
