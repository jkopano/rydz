#pragma once

#include <raylib.h>
#include <rlgl.h>

namespace rl {

// ---- Types ----
using ::Camera2D;
using ::Camera3D;
using ::GltfScene;
using ::Image;
using ::Material;
using ::MaterialMap;
using ::Matrix;
using ::Mesh;
using ::Model;
using ::Quaternion;
using ::rlColor;
using ::rlRectangle;
using Rectangle = ::rlRectangle;
using ::RenderTexture;
using ::RenderTexture2D;
using ::Shader;
using ::Sound;
using ::Texture2D;
using ::Vector2;
using ::Vector3;
using ::Vector4;

// ---- Core: Window / Drawing ----
using ::BeginDrawing;
using ::BeginShaderMode;
using ::BeginTextureMode;
using ::ClearBackground;
using ::rlCloseWindow;
inline void CloseWindow() { ::rlCloseWindow(); }
using ::DrawFPS;
using ::EndDrawing;
using ::EndShaderMode;
using ::EndTextureMode;
using ::GetFrameTime;
using ::InitWindow;
using ::IsWindowReady;
using ::LoadFileText;
using ::SetShaderValueTexture;
using ::SetTargetFPS;
using ::SetTraceLogLevel;
using ::TraceLog;
using ::UnloadFileText;
using ::WindowShouldClose;

// ---- Input ----
using ::DisableCursor;
using ::EnableCursor;
using ::rlHideCursor;
using ::rlShowCursor;
inline void ShowCursor() { ::rlShowCursor(); }
inline void HideCursor() { ::rlHideCursor(); }
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
using ::GenMeshTangents;
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
using ::ImageText;
using ::ImageTextEx;
using ::rlLoadImage;
inline auto LoadImage(char const* fileName) -> Image {
  return ::rlLoadImage(fileName);
}
using ::LoadRenderTexture;
using ::LoadTexture;
using ::LoadTextureFromImage;
using ::SetTextureFilter;
using ::SetTextureWrap;
using ::UnloadImage;
using ::UnloadRenderTexture;
using ::UnloadTexture;

// ---- Audio ----
using ::LoadSound;

// ---- GLTF ----
using ::LoadGltfScene;

// ---- Shaders ----
using ::GetShaderLocation;
using ::GetShaderLocationAttrib;
using ::LoadShader;
using ::LoadShaderFromMemory;
using ::SetShaderValue;
using ::SetShaderValueMatrix;
using ::SetShaderValueV;

// ---- rlgl functions ----
using ::rlActiveTextureSlot;
using ::rlBegin;
using ::rlBindShaderBuffer;
using ::rlCheckRenderBatchLimit;
using ::rlColor4ub;
using ::rlColorMask;
using ::rlCompileShader;
using ::rlComputeShaderDispatch;
using ::rlDisableBackfaceCulling;
using ::rlDisableColorBlend;
using ::rlDisableDepthMask;
using ::rlDisableDepthTest;
using ::rlDisableShader;
using ::rlDisableTexture;
using ::rlDisableTextureCubemap;
using ::rlDisableVertexArray;
using ::rlDisableVertexAttribute;
using ::rlDisableVertexBuffer;
using ::rlDisableVertexBufferElement;
using ::rlDisableWireMode;
using ::rlDrawVertexArray;
using ::rlDrawVertexArrayElements;
using ::rlDrawVertexArrayElementsInstanced;
using ::rlDrawVertexArrayInstanced;
using ::rlEnableBackfaceCulling;
using ::rlEnableColorBlend;
using ::rlEnableDepthMask;
using ::rlEnableDepthTest;
using ::rlEnableShader;
using ::rlEnableTexture;
using ::rlEnableTextureCubemap;
using ::rlEnableVertexArray;
using ::rlEnableVertexAttribute;
using ::rlEnableVertexBuffer;
using ::rlEnableVertexBufferElement;
using ::rlEnableWireMode;
using ::rlGetShaderIdDefault;
using ::rlGetShaderLocsDefault;
using ::rlGetTextureIdDefault;
using ::rlLoadComputeShaderProgram;
using ::rlLoadShaderBuffer;
using ::rlLoadTextureCubemap;
using ::rlLoadVertexArray;
using ::rlLoadVertexBuffer;
using ::rlLoadVertexBufferElement;
using ::rlNormal3f;
using ::rlReadShaderBuffer;
using ::rlSetBlendMode;
using ::rlSetCullFace;
using ::rlSetShader;
using ::rlSetTexture;
using ::rlSetUniform;
using ::rlSetUniformMatrices;
using ::rlSetUniformMatrix;
using ::rlSetUniformSampler;
using ::rlSetVertexAttribute;
using ::rlSetVertexAttributeDivisor;
using ::rlTexCoord2f;
using ::rlUnloadShaderBuffer;
using ::rlUnloadShaderProgram;
using ::rlUnloadTexture;
using ::rlUnloadVertexArray;
using ::rlUnloadVertexBuffer;
using ::rlUpdateShaderBuffer;
using ::rlVertex2f;

// ---- rlgl: Matrix / Batch ----
using ::rlDrawRenderBatchActive;
using ::rlEnd;
using ::rlGetLocationAttrib;
using ::rlGetLocationUniform;
using ::rlGetMatrixModelview;
using ::rlGetMatrixProjection;
using ::rlGetMatrixTransform;
using ::rlLoadIdentity;
using ::rlMatrixMode;
using ::rlMultMatrixf;
using ::rlPopMatrix;
using ::rlPushMatrix;
using ::rlSetMatrixModelview;
using ::rlSetMatrixProjection;

} // namespace rl
