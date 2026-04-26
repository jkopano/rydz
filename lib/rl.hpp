#pragma once

#include <raylib.h>
#define RL_MATRIX_TYPE
#include <external/glad.h>
#include <rlgl.h>

namespace rl {

// ---- Types ----
using ::AudioStream;
using ::Camera2D;
using ::Camera3D;
using ::Font;
using ::GltfScene;
using ::Image;
using ::Matrix;
using ::Model;
using ::Quaternion;
using ::RenderTexture;
using ::RenderTexture2D;
using ::rlColor;
using ::rlMaterial;
using ::rlMaterialMap;
using ::rlMesh;
using ::rlRectangle;
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
using ::DrawFPS;
using ::EndDrawing;
using ::EndShaderMode;
using ::EndTextureMode;
using ::GetFrameTime;
using ::InitWindow;
using ::IsWindowFullscreen;
using ::IsWindowReady;
using ::LoadFileText;
using ::rlCloseWindow;
using ::SetConfigFlags;
using ::SetShaderValueTexture;
using ::SetTargetFPS;
using ::SetTraceLogLevel;
using ::ToggleFullscreen;
using ::TraceLog;
using ::UnloadFileText;
using ::WindowShouldClose;

inline constexpr unsigned int FLAG_FULLSCREEN_MODE = ::FLAG_FULLSCREEN_MODE;

using ::ColorFromHSV;

// ---- Input ----
using ::DisableCursor;
using ::EnableCursor;
using ::GetKeyPressed;
using ::GetMouseDelta;
using ::IsKeyDown;
using ::IsKeyPressed;
using ::IsKeyUp;
using ::rlHideCursor;
using ::rlShowCursor;

// ---- Screen ----
using ::GetScreenHeight;
using ::GetScreenWidth;
using ::SetConfigFlags;

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
using ::LoadRenderTexture;
using ::LoadTexture;
using ::LoadTextureFromImage;
using ::rlLoadImage;
using ::SetTextureFilter;
using ::SetTextureWrap;
using ::UnloadImage;
using ::UnloadRenderTexture;
using ::UnloadTexture;

// ---- Font ----
using ::GetFontDefault;
using ::ImageDrawTextEx;
using ::LoadFont;
using ::MeasureTextEx;

// ---- Audio ----
using ::CloseAudioDevice;
using ::InitAudioDevice;
using ::IsAudioDeviceReady;
using ::IsSoundPlaying;
using ::LoadSound;
using ::PauseSound;
using ::PlaySound;
using ::ResumeSound;
using ::SetSoundPan;
using ::SetSoundPitch;
using ::SetSoundVolume;
using ::StopSound;
using ::UnloadSound;

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
using ::rlBindFramebuffer;
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
using ::rlFramebufferAttach;
using ::rlFramebufferComplete;
using ::rlGetShaderIdDefault;
using ::rlGetShaderLocsDefault;
using ::rlGetTextureIdDefault;
using ::rlLoadComputeShaderProgram;
using ::rlLoadFramebuffer;
using ::rlLoadShaderBuffer;
using ::rlLoadTexture;
using ::rlLoadTextureCubemap;
using ::rlLoadTextureDepth;
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

// ---- OpenGL Wrapper ----
inline auto SetTextureParameteri(unsigned int texture_id, unsigned int param, int value)
  -> void {
#if defined(GRAPHICS_API_OPENGL_33) || defined(GRAPHICS_API_OPENGL_ES2)
  unsigned int prev_texture = rlGetTextureIdDefault();
  rlEnableTexture(texture_id);
  glTexParameteri(GL_TEXTURE_2D, param, value);
  rlEnableTexture(prev_texture);
#endif
}

inline auto SetTextureParameterf(unsigned int texture_id, unsigned int param, float value)
  -> void {
#if defined(GRAPHICS_API_OPENGL_33) || defined(GRAPHICS_API_OPENGL_ES2)
  unsigned int prev_texture = rlGetTextureIdDefault();
  rlEnableTexture(texture_id);
  glTexParameterf(GL_TEXTURE_2D, param, value);
  rlEnableTexture(prev_texture);
#endif
}

inline auto GetUniformBlockIndex(unsigned int program_id, char const* name)
  -> unsigned int {
#if defined(GRAPHICS_API_OPENGL_33) || defined(GRAPHICS_API_OPENGL_ES2)
  return glGetUniformBlockIndex(program_id, name);
#else
  return 0;
#endif
}

inline auto UniformBlockBinding(
  unsigned int program_id, unsigned int block_index, unsigned int binding
) -> void {
#if defined(GRAPHICS_API_OPENGL_33) || defined(GRAPHICS_API_OPENGL_ES2)
  glUniformBlockBinding(program_id, block_index, binding);
#endif
}

inline auto CheckFramebufferStatus(unsigned int target) -> unsigned int {
#if defined(GRAPHICS_API_OPENGL_33) || defined(GRAPHICS_API_OPENGL_ES2)
  return glCheckFramebufferStatus(target);
#else
  return 0;
#endif
}

inline auto BindFramebuffer(unsigned int target, unsigned int framebuffer) -> void {
#if defined(GRAPHICS_API_OPENGL_33) || defined(GRAPHICS_API_OPENGL_ES2)
  glBindFramebuffer(target, framebuffer);
#endif
}

inline auto GenBuffers(int count, unsigned int* buffers) -> void {
#if defined(GRAPHICS_API_OPENGL_33) || defined(GRAPHICS_API_OPENGL_ES2)
  glGenBuffers(count, buffers);
#endif
}

inline auto DeleteBuffers(int count, unsigned int const* buffers) -> void {
#if defined(GRAPHICS_API_OPENGL_33) || defined(GRAPHICS_API_OPENGL_ES2)
  glDeleteBuffers(count, buffers);
#endif
}

inline auto BindBuffer(unsigned int target, unsigned int buffer) -> void {
#if defined(GRAPHICS_API_OPENGL_33) || defined(GRAPHICS_API_OPENGL_ES2)
  glBindBuffer(target, buffer);
#endif
}

inline auto BufferData(
  unsigned int target, long size, void const* data, unsigned int usage
) -> void {
#if defined(GRAPHICS_API_OPENGL_33) || defined(GRAPHICS_API_OPENGL_ES2)
  glBufferData(target, size, data, usage);
#endif
}

inline auto BufferSubData(unsigned int target, long offset, long size, void const* data)
  -> void {
#if defined(GRAPHICS_API_OPENGL_33) || defined(GRAPHICS_API_OPENGL_ES2)
  glBufferSubData(target, offset, size, data);
#endif
}

inline auto BindBufferBase(unsigned int target, unsigned int index, unsigned int buffer)
  -> void {
#if defined(GRAPHICS_API_OPENGL_33) || defined(GRAPHICS_API_OPENGL_ES2)
  glBindBufferBase(target, index, buffer);
#endif
}

inline auto GenTextureMipmaps(unsigned int texture_id) -> void {
#if defined(GRAPHICS_API_OPENGL_33) || defined(GRAPHICS_API_OPENGL_ES2)
  unsigned int prev_texture = rlGetTextureIdDefault();
  rlEnableTexture(texture_id);
  glGenerateMipmap(GL_TEXTURE_2D);
  rlEnableTexture(prev_texture);
#endif
}

} // namespace rl
