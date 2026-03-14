#pragma once

#include <raylib.h>
#include <raymath.h>
#include <rcamera.h>
#include <rlgl.h>

namespace rl {

// ---- Types ----
using ::Vector2;
using ::Vector3;
using ::Vector4;
using ::Matrix;
using ::Quaternion;
using ::Color;
using ::Rectangle;
using ::Image;
using ::Texture2D;
using ::Shader;
using ::Camera3D;
using ::Camera2D;
using ::Mesh;
using ::Model;
using ::Material;
using ::MaterialMap;
using ::Sound;
using ::GltfScene;

// ---- Core: Window / Drawing ----
using ::InitWindow;
using ::CloseWindow;
using ::WindowShouldClose;
using ::SetTargetFPS;
using ::BeginDrawing;
using ::EndDrawing;
using ::BeginMode3D;
using ::EndMode3D;
using ::ClearBackground;
using ::DrawFPS;
using ::SetTraceLogLevel;
using ::TraceLog;
using ::GetFrameTime;

// ---- Input ----
using ::IsKeyDown;
using ::IsKeyUp;
using ::IsKeyPressed;
using ::GetKeyPressed;
using ::GetMouseDelta;
using ::DisableCursor;
using ::EnableCursor;

// ---- Screen ----
using ::GetScreenWidth;
using ::GetScreenHeight;

// ---- Camera ----
using ::GetCameraViewMatrix;
using ::GetCameraProjectionMatrix;

// ---- Models / Meshes ----
using ::LoadModel;
using ::LoadModelFromMesh;
using ::DrawMesh;
using ::DrawMeshInstanced;
using ::UploadMesh;
using ::GenMeshCube;
using ::GenMeshSphere;
using ::GenMeshPlane;
using ::GenMeshCylinder;
using ::GenMeshTorus;
using ::GenMeshHemiSphere;
using ::GenMeshKnot;

// ---- Textures / Images ----
using ::LoadTexture;
using ::LoadImage;
using ::UnloadImage;
using ::ImageFormat;

// ---- Audio ----
using ::LoadSound;

// ---- GLTF ----
using ::LoadGltfScene;

// ---- Shaders ----
using ::LoadShader;
using ::GetShaderLocation;
using ::GetShaderLocationAttrib;
using ::SetShaderValue;
using ::SetShaderValueV;
using ::SetShaderValueMatrix;

// ---- Math: Matrix ----
using ::MatrixIdentity;
using ::MatrixScale;
using ::MatrixTranslate;
using ::MatrixMultiply;
using ::MatrixInvert;
using ::MatrixLookAt;
using ::MatrixPerspective;

// ---- Math: Vector3 ----
using ::Vector3Add;
using ::Vector3Subtract;
using ::Vector3Scale;
using ::Vector3Normalize;
using ::Vector3Length;
using ::Vector3LengthSqr;
using ::Vector3Transform;
using ::Vector3RotateByQuaternion;

// ---- Math: Quaternion ----
using ::QuaternionIdentity;
using ::QuaternionFromEuler;
using ::QuaternionFromMatrix;
using ::QuaternionNormalize;
using ::QuaternionToMatrix;

// ---- rlgl functions ----
using ::rlGetShaderIdDefault;
using ::rlGetShaderLocsDefault;
using ::rlGetTextureIdDefault;
using ::rlEnableDepthTest;
using ::rlDisableDepthTest;
using ::rlEnableDepthMask;
using ::rlDisableDepthMask;
using ::rlEnableBackfaceCulling;
using ::rlDisableBackfaceCulling;
using ::rlSetCullFace;
using ::rlEnableWireMode;
using ::rlDisableWireMode;
using ::rlSetBlendMode;
using ::rlEnableShader;
using ::rlDisableShader;
using ::rlEnableTextureCubemap;
using ::rlDisableTextureCubemap;
using ::rlEnableVertexArray;
using ::rlDisableVertexArray;
using ::rlLoadVertexArray;
using ::rlLoadVertexBuffer;
using ::rlUnloadVertexArray;
using ::rlUnloadVertexBuffer;
using ::rlSetVertexAttribute;
using ::rlEnableVertexAttribute;
using ::rlLoadTextureCubemap;
using ::rlUnloadTexture;
using ::rlActiveTextureSlot;
using ::rlDrawVertexArray;

} // namespace rl
