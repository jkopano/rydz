#pragma once

#include <raylib.h>
#include <raymath.h>
#include <rcamera.h>
#include <rlgl.h>

namespace rl {

// ---- Types ----
using ::Camera2D;
using ::Camera3D;
using ::Color;
using ::GltfScene;
using ::Image;
using ::Material;
using ::MaterialMap;
using ::Matrix;
using ::Mesh;
using ::Model;
using ::Quaternion;
using ::Rectangle;
using ::Shader;
using ::Sound;
using ::Texture2D;
using ::Vector2;
using ::Vector3;
using ::Vector4;

// ---- Core: Window / Drawing ----
using ::BeginDrawing;
using ::BeginMode3D;
using ::ClearBackground;
using ::CloseWindow;
using ::DrawFPS;
using ::EndDrawing;
using ::EndMode3D;
using ::GetFrameTime;
using ::InitWindow;
using ::SetTargetFPS;
using ::SetTraceLogLevel;
using ::TraceLog;
using ::WindowShouldClose;

// ---- Input ----
using ::DisableCursor;
using ::EnableCursor;
using ::GetKeyPressed;
using ::GetMouseDelta;
using ::IsKeyDown;
using ::IsKeyPressed;
using ::IsKeyUp;

// ---- Screen ----
using ::GetScreenHeight;
using ::GetScreenWidth;

// ---- Camera ----
using ::GetCameraProjectionMatrix;
using ::GetCameraViewMatrix;

// ---- Models / Meshes ----
using ::DrawMesh;
using ::DrawMeshInstanced;
using ::GenMeshCube;
using ::GenMeshCylinder;
using ::GenMeshHemiSphere;
using ::GenMeshKnot;
using ::GenMeshPlane;
using ::GenMeshSphere;
using ::GenMeshTorus;
using ::LoadModel;
using ::LoadModelFromMesh;
using ::UpdateMeshBuffer;
using ::UploadMesh;

// ---- Textures / Images ----
using ::ImageFormat;
using ::LoadImage;
using ::LoadTexture;
using ::UnloadImage;

// ---- Audio ----
using ::LoadSound;

// ---- GLTF ----
using ::LoadGltfScene;

// ---- Shaders ----
using ::GetShaderLocation;
using ::GetShaderLocationAttrib;
using ::LoadShader;
using ::SetShaderValue;
using ::SetShaderValueMatrix;
using ::SetShaderValueV;

// ---- Math: Matrix ----
using ::MatrixIdentity;
using ::MatrixInvert;
using ::MatrixLookAt;
using ::MatrixMultiply;
using ::MatrixPerspective;
using ::MatrixScale;
using ::MatrixTranslate;

// ---- Math: Vector3 ----
using ::Vector3Add;
using ::Vector3DotProduct;
using ::Vector3Length;
using ::Vector3LengthSqr;
using ::Vector3Normalize;
using ::Vector3RotateByQuaternion;
using ::Vector3Scale;
using ::Vector3Subtract;
using ::Vector3Transform;

// ---- Math: Quaternion ----
using ::QuaternionFromEuler;
using ::QuaternionFromMatrix;
using ::QuaternionIdentity;
using ::QuaternionNormalize;
using ::QuaternionToMatrix;

// ---- rlgl functions ----
using ::rlActiveTextureSlot;
using ::rlDisableBackfaceCulling;
using ::rlDisableDepthMask;
using ::rlDisableDepthTest;
using ::rlDisableShader;
using ::rlDisableTextureCubemap;
using ::rlDisableVertexArray;
using ::rlDisableWireMode;
using ::rlDrawVertexArray;
using ::rlEnableBackfaceCulling;
using ::rlEnableDepthMask;
using ::rlEnableDepthTest;
using ::rlEnableShader;
using ::rlEnableTextureCubemap;
using ::rlEnableVertexArray;
using ::rlEnableVertexAttribute;
using ::rlEnableWireMode;
using ::rlGetShaderIdDefault;
using ::rlGetShaderLocsDefault;
using ::rlGetTextureIdDefault;
using ::rlLoadTextureCubemap;
using ::rlLoadVertexArray;
using ::rlLoadVertexBuffer;
using ::rlSetBlendMode;
using ::rlSetCullFace;
using ::rlSetVertexAttribute;
using ::rlUnloadTexture;
using ::rlUnloadVertexArray;
using ::rlUnloadVertexBuffer;

} // namespace rl
