#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define NOMINMAX

#ifdef _WIN32
	#define VK_USE_PLATFORM_WIN32_KHR
	#define GLFW_EXPOSE_NATIVE_WIN32
#else
	#define GLFW_EXPOSE_NATIVE_WAYLAND
	#define GLFW_EXPOSE_NATIVE_XTERM
#endif

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <lua.hpp>
}

#include <filesystem>
#include "engineUtils.h"
#include "engineSettings.h"
#include "zstring.h"

#ifndef _WIN32
#include <cstring>
#endif

// These are bits instead of an index, so a shader can have more than 1 type
enum ShaderFlags
{
	SF_DEFAULT,
	SF_ALPHA, // Things that need transparency, like glass or water, these will be separated from the other meshes so that they are always drawn last.
	SF_POSTPROCESS, // Shaders for post processing
	SF_SKYBOX, // Shaders that only need a cubemap texture
	SF_SHADOW, // Shadow Map shader
	SF_SUNSHADOWPASS,
	SF_SPOTSHADOWPASS
};

enum RenderStageType
{
	RST_DEFAULT,
	RST_POSTPROCESS,
	RST_BLIT,
	RST_SHADOW
};



// The first 3 bits indicate the type, the rest are flags
// CT_NONE - the Thing is not added to the collision lists
// CT_BBOX - TraceRay will only check against the bounding box and return that result
// CT_PERTRI - TraceRay will first check against the bounding box, and if that hits, then it will go through each triangle checking for collisions there
// CT_COLLISION_ONLY - the Thing is added to the collision lists, but not the render stages, so it won't be drawn but still have collision. This can make custom collision or everyone's favourite invisible barriers
// COLLISION_ONLY is a flag, so it can be or'd with the other ones to pick which type of hitbox
enum CollisionType
{
	CT_NONE,
	CT_BOX,
	CT_PERTRI,
	CT_COLLISION_ONLY = 8
};



typedef int Bool;
#define True 1
#define False 0

#define vkcheck(x, message) if (x != VK_SUCCESS) throw std::runtime_error(message)
#define check(assertion, message) if (!(assertion)) throw std::runtime_error(message)

#define MAX(a, b) (a > b ? a : b)
#define MIN(a, b) (a < b ? a : b)
#define SIGN(x) x < 0 ? -1 : 1;
#define NEW(type) (type*)malloc(sizeof(type))
#define ZEROMEM(ptr, size) memset(ptr, 0, size);
#define MAKESHORT(string) string[0] << 8 | string[1]

#define ConvertVec(v, type) type(v.x, v.y, v.z)

#define LuaPCall(L, nargs, nret, message) if (lua_pcall(L, nargs, nret, 0)) { printf(message, lua_tostring(L, -1)); lua_pop(L, 1); }
#define LuaPushFuncField(func, name) lua_pushcclosure(L, func, 0); lua_setfield(L, -2, name)

#define AddLuaGlobalInt(num, name) lua_pushnumber(L, num); \
														lua_setglobal(L, name)

#define AddLuaGlobalEnum(val)	lua_pushnumber(L, val); \
												lua_setglobal(L, #val)

#define AddLuaGlobalUData(udata, name) lua_pushlightuserdata(L, udata); \
															lua_setglobal(L, name)

#define IncReadAs(x, type) *(type*)x; x += sizeof(type)


constexpr float LOOK_SENSITIVITY = 0.001f;

typedef unsigned char BYTE;

typedef glm::mat4 float4x4;
typedef glm::mat3 float3x3;
typedef glm::vec4 float4;
typedef glm::vec3 float3;
typedef glm::vec2 float2;
typedef glm::ivec4 int4;
typedef glm::ivec3 int3;
typedef glm::ivec2 int2;
typedef glm::uvec4 uint4;
typedef glm::uvec3 uint3;
typedef glm::uvec2 uint2;
typedef glm::bvec4 bool4;
typedef glm::bvec3 bool3;
typedef glm::bvec2 bool2;

class Camera;
class DescriptorSet;
struct Material;
class Mesh;
struct Mexel;
class Shader;
class SpotLight;
class Texture;
class Thing;
class Thread;
class VulkanBackend;
class GPUMemory;
struct RenderPass;

struct Rect
{
	uint32_t x, y, width, height;
};

// Buffer sent to the GPU once per-frame
struct UniformBufferObject {
	float4x4 viewProj;
	float3 CAMERA;
	float time;
};

// Buffer sent to the GPU once per-frame, for post processing
struct PostBuffer {
	float4x4 viewProj;
	float4x4 viewMatrix;
	float4 camPos;
	float2 velocity;
};

class SDF
{
public:
	Texture* texture;
	VkDevice device;

	SDF(Mesh* mesh, VulkanBackend* backend);
	~SDF();
};

struct SamplerSettings
{
	VkFilter magFilter, minFilter;
	VkSamplerAddressMode addressMode;
	int mipLevels;

	bool operator==(SamplerSettings& other)
	{
		return this->magFilter == other.magFilter &&
			this->minFilter == other.minFilter &&
			this->addressMode == other.addressMode &&
			this->mipLevels == other.mipLevels;
	}
};

struct SunPassThreadInfo
{
	uint32_t cascade;
	VkRenderPassBeginInfo passInfo;
	Shader* opaqueShader, *maskedShader;
};

#ifdef LGE_BACKWARDS_COMPATIBILITY

typedef Thing MeshObject;
typedef RenderStage FullRenderPass;
typedef RenderStageShaderGroup RenderPassPipelineGroup;
typedef RenderStageMaterialGroup RenderPassMaterialGroup;
typedef RenderStageMeshGroup RenderPassMeshGroup;

enum FullRenderPassType
{
	RPT_DEFAULT,
	RPT_POSTPROCESS,
	RPT_BLIT,
	RPT_SHADOW
};

#define AddObject AddThing
#define SetupObjects SetupThings
#define SortObjects SortThings
#define NewPipeline_Separate NewShader_Separate
#define allPipelines allShaders
#define numPipelines numShaders

#define AddObjectToPipelineGroup AddThingToShaderGroup
#define AddObjectToRenderingProcess AddThingToRenderStage
#define AddObjectToExistingRenderProcess AddThingToExistingRenderStage

#endif