#pragma once

#include <raylib.h>
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
using ::ClearBackground;
using ::CloseWindow;
using ::DrawFPS;
using ::EndDrawing;
using ::GetFrameTime;
using ::InitWindow;
using ::IsWindowReady;
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

// ---- Models / Meshes ----
using ::DrawMesh;
using ::DrawMeshInstanced;
using ::DrawRectangle;
using ::DrawTexture;
using ::DrawTexturePro;
using ::GenMeshCube;
using ::GenMeshCylinder;
using ::GenMeshHemiSphere;
using ::GenMeshKnot;
using ::GenMeshPlane;
using ::GenMeshSphere;
using ::GenMeshTorus;
using ::LoadModel;
using ::LoadModelFromMesh;
using ::UnloadMesh;
using ::UnloadModel;
using ::UpdateMeshBuffer;
using ::UploadMesh;

// ---- Textures / Images ----
using ::GenImageColor;
using ::ImageFormat;
using ::LoadImage;
using ::LoadTexture;
using ::LoadTextureFromImage;
using ::UnloadImage;
using ::UnloadTexture;

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

// ---- rlgl: Matrix / Batch ----
using ::rlDrawRenderBatchActive;
using ::rlLoadIdentity;
using ::rlMatrixMode;
using ::rlMultMatrixf;
using ::rlPopMatrix;
using ::rlPushMatrix;
using ::rlSetMatrixModelview;
using ::rlSetMatrixProjection;

} // namespace rl
