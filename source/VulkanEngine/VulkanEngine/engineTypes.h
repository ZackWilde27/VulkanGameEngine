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
#include <thread>
#include <threads.h>
#include "engineUtils.h"

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

enum BlendMode
{
	BM_OPAQUE,
	BM_TRANSPARENT,
	BM_ADDITIVE,
	BM_MAX
};

#define vkcheck(x, message) if (x != VK_SUCCESS) throw std::runtime_error(message)
#define check(assertion, message) if (!(assertion)) throw std::runtime_error(message)

#define VK_FLAGS_NONE 0

#define MAX(a, b) (a > b ? a : b)
#define MIN(a, b) (a < b ? a : b)
#define SIGN(x) x < 0 ? -1 : 1;
#define NEW(type) (type*)malloc(sizeof(type))
#define ZEROMEM(ptr, size) memset(ptr, 0, size);

#define ConvertVec(v, type) type(v.x, v.y, v.z)

#define LuaPCall(L, nargs, nret, message) if (lua_pcall(L, nargs, nret, 0)) { printf(message, lua_tostring(L, -1)); lua_pop(L, 1); }
#define LuaPushFuncField(func, name) lua_pushcclosure(L, func, 0); lua_setfield(L, -2, name)

#define AddLuaGlobalInt(num, name) lua_pushnumber(L, num); \
													lua_setglobal(L, name)

#define AddLuaGlobalUData(udata, name) lua_pushlightuserdata(L, udata); \
															lua_setglobal(L, name)

#define IncReadAs(x, type) *(type*)x; x += sizeof(type)

constexpr float LOOK_SENSITIVITY = 0.001f;
constexpr size_t SHADOWMAPSIZE = 1600;
constexpr size_t MESH_NAME_SIZE = 64;

typedef unsigned char BYTE;

typedef glm::mat4 float4x4;
typedef glm::mat3 float3x3;
typedef glm::vec4 float4;
typedef glm::vec3 float3;
typedef glm::vec2 float2;

class VulkanBackend;
class Thing;
class Shader;
class SpotLight;
struct Material;


typedef bool (*zThreadFunc)(void*);
int zThreadTick(void* thread);

class Thread
{
	thrd_t thread;
public:
	VkBool32 shouldClose;
	void* udata;
	zThreadFunc function;

	Thread(zThreadFunc func, void* data)
	{
		shouldClose = VK_FALSE;
		udata = data;
		function = func;

		if (thrd_create(&thread, zThreadTick, this) != thrd_success)
			throw std::runtime_error("Failed to create thread!");
	}

	~Thread()
	{
		shouldClose = VK_TRUE;
		thrd_join(thread, NULL);
	}
};

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

class VulkanMemory
{
public:
	VkBuffer buffer;
	size_t size;
	VkBool32 destroyed;
	const char* origin; // Used when debugging non-destroyed buffers

private:
	VkDeviceMemory memory;
	VkDevice logicalDevice;

public:
	// isStatic is whether or not the buffer can be updated by the CPU (static buffers are much faster for the GPU). If static, data is what's copied to the buffer at the start before it's no longer possible to update it
	VulkanMemory(VulkanBackend* backend, size_t size, VkBufferUsageFlags usage, const char* origin, bool isStatic, void* data);

	~VulkanMemory()
	{
		vkDestroyBuffer(logicalDevice, buffer, nullptr);
		vkFreeMemory(logicalDevice, memory, nullptr);
		destroyed = true;
	}

	void* Map() const
	{
		return Map(0, size);
	}

	void* Map(uint32_t offset, uint32_t size) const
	{
		return Map(offset, size, 0);
	}

	void* Map(uint32_t offset, uint32_t size, VkMemoryMapFlags flags) const
	{
		void* data;
		vkMapMemory(logicalDevice, memory, offset, size, flags, &data);
		return data;
	}

	void UnMap() const
	{
		vkUnmapMemory(logicalDevice, this->memory);
	}

	VkDescriptorBufferInfo GetBufferInfo() const
	{
		VkDescriptorBufferInfo info{};
		info.offset = 0;
		info.range = size;
		info.buffer = buffer;
		return info;
	}

	operator VkBuffer() const
	{
		return this->buffer;
	}
};

struct Texture
{
	const char* filename;
	VkBool32 freeFilename;
	VkImage Image;
	VkImageAspectFlags Aspect;
	uint32_t Width, Height;
	uint32_t mipLevels;
	VkDeviceMemory Memory;
	VkImageView View;
	VkSampler Sampler;
	VkImageLayout theoreticalLayout; // This is used when reading the render stage from engine.lua, to anticipate layout errors before they happen
	VkFormat format;
	size_t textureIndex; // Index into the allTextures array
};

// Just like a 'pixel' is a 'picture-element', and a 'voxel' is a 'volume-element', a 'mexel' is a 'mesh-element'
// It is the individual mesh pieces with separate materials that make up a mesh
struct Mexel
{
	const char* Filename;
	size_t startingVertex;
	size_t startingIndex;
	uint32_t IndexBufferLength;
	uint32_t mexelIndex;

	float3 boundingBoxMin;
	float3 boundingBoxMax;
	float3 boundingBoxCentre;
};

struct Mesh
{
	char name[MESH_NAME_SIZE];
	std::vector<Mexel*> mexels;
	float3 boundingBoxMin;
	float3 boundingBoxMax;
	float3 boundingBoxCentre;
};


// In order to make recording the command buffer as efficient as possible, it groups objects by their pipeline (so it only binds the pipeline once and draws everything that uses it)
// The pipeline stores groups of meshes (so that it only binds the buffers once and then draws every instance)
// and the meshGroups store all the data for each instance of that mesh

struct RenderStageMeshGroup
{
	Mexel* mexel;
	uint32_t numInstances;
	std::vector<float4x4> matrices;
	std::vector<float4> shadowMapOffsets;
	VulkanMemory* matrixMem;
	VulkanMemory* shadowMapOffsetsMem;
	VkDescriptorSet descriptorSet;
	float3 boundingBoxMin;
	float3 boundingBoxMax;
	float3 boundingBoxCentre;
	VkBool32 isStatic;
};

struct RenderStageMaterialGroup
{
	Material* material;
	std::vector<RenderStageMeshGroup*> meshGroups;
};

struct RenderStageShaderGroup
{
	Shader* shader;
	std::vector<RenderStageMaterialGroup*> materialGroups;
};

struct RenderPassFromToLayout
{
	VkImageLayout from;
	VkImageLayout to;
};

struct RenderPass
{
	VkRenderPass renderPass;
	std::vector<RenderPassFromToLayout> layouts;
};

struct FullSampler
{
	VkSampler sampler;
	VkFilter magFilter, minFilter;
	VkSamplerAddressMode addressMode;
	int mipLevels;
};

struct Shader
{
	VkPipeline pipeline;
	VkPipelineLayout pipelineLayout;

	const char* zlslFile;
	std::filesystem::file_time_type mtime;
	const char* vertexShader;
	const char* pixelShader;
	int shaderType;
	VkCullModeFlags cullMode;
	VkPolygonMode polygonMode;
	VkRenderPass renderPass;
	VkSampleCountFlagBits sampleCount;
	BlendMode alphaBlend;
	bool depthTest;
	bool depthWrite;
	bool freeVertexShader;
	bool freePixelShader;
	bool masked;
	bool isStatic;
	int numTextures;
	std::vector<VkDescriptorSetLayout> setLayouts;
	VkPushConstantRange pushConstantRange;
	uint32_t numAttachments;
	uint32_t stencilWriteMask;
	VkCompareOp stencilCompareOp;
	uint32_t stencilTestValue;
	float depthBias;
};

struct Material
{
	std::vector<Texture*> textures;
	Shader* shader;
	VkDescriptorSet descriptorSets[2]; // The first one contains all textures, the second one only has the colour texture, for alpha testing on shadow maps
	bool masked; // Whether or not to alpha test on shadow maps, whether or not the shader does discarding does not affect it
	float roughness; // This will act as a multiplier for the roughness texture, so you can make a material rougher or shinier
};

struct FullSetLayout
{
	size_t numVBuffers, numPBuffers, numSamplers, numStorageBuffers;
	VkDescriptorSetLayout setLayout;
};


class Thing
{
public:
	float3 position;
	float3 rotation;
	float3 scale;

	std::vector<RenderStageMeshGroup*> meshGroups; // Pointer to its mesh group for movable objects to update their matrix
	std::vector<uint32_t> matrixIndices; // Index into the matrix array in the meshGroup for updating the matrix

	Texture*& shadowMap;
	Mesh* mesh;
	std::vector<Material*> materials;

	float2 shadowMapOffset;
	float shadowMapScale;

	BYTE id;
	bool isStatic;
	bool castShadow;

	Thing(float3 position, float3 rotation, float3 scale, Mesh* mesh, Texture*& shadowmap, float texScale, bool isStatic, bool castShadow, BYTE id, const char* scriptFilename);

	void UpdateMatrix(float4x4* overrideMatrix) const;
};

class Camera
{
public:
	float3 position;
	float3 target;
	float4x4 matrix;
	float4x4 viewMatrix;
	float3 velocityVec;
	float oldpitch, oldyaw;

	Camera()
	{
		position = float3(0);
		target = float3(1, 0, 0);
		matrix = {};
		viewMatrix = {};
		velocityVec = float3(0);
		oldpitch = 0;
		oldyaw = 0;
	}

	void TargetFromRotation(float pitch, float yaw)
	{
		float4x4 m = glm::rotate(float4x4(1.0f), pitch, float3(0.0f, 1.0f, 0.0f));
		m = glm::rotate(m, yaw, float3(0.0f, 0.0f, 1.0f));
		target = float4(1.0f, 0.0f, 0.0f, 0.0f) * m;
		target += position;

		velocityVec.y = (pitch - oldpitch) * 0.3f;
		velocityVec.x = (oldyaw - yaw) * 0.3f;
		velocityVec.y *= velocityVec.y * SIGN(velocityVec.y);
		velocityVec.x *= velocityVec.x * SIGN(velocityVec.x);

		oldpitch = pitch;
		oldyaw = yaw;
	}

	void UpdateMatrix(float4x4* perspectiveMatrix)//mat4* perspectiveMatrix)
	{
		viewMatrix = glm::lookAt(position, target, float3(0.0f, 0.0f, 1.0f));
		matrix = (*perspectiveMatrix) * viewMatrix;
	}
};



constexpr int NUMCASCADES = 4;

class Light
{
public:
	std::vector<VulkanMemory*> viewProjBuffer;
	std::vector<VkDescriptorSet> descriptorSetPS;

	virtual void RecordCommandBuffer(Camera* activeCamera, uint32_t imageIndex) {};
	virtual void UpdateMatrix(Camera* activeCamera, uint32_t imageIndex) {}
};

class SunLight : public Light
{
public:
	float3 dir;
	float3 offset;
	float4 orthoParams;
	Texture renderTargets[NUMCASCADES];
	VkFramebuffer frameBuffers[NUMCASCADES];
	std::vector<std::array<VkCommandBuffer, NUMCASCADES>> commandBuffers;
	std::vector<std::array<VkDescriptorSet, NUMCASCADES>> descriptorSetVS;
	float4x4 projectionMatrices[NUMCASCADES];
	float cascadeDistances[NUMCASCADES];

public:
	SunLight(float3 dir, uint32_t width, uint32_t height, VulkanBackend* backend);
	~SunLight();

	struct SunUniformBuffer
	{
		float4x4 viewProjs[NUMCASCADES];
		float3 dir;
	};

	void UpdateProjection()
	{
		for (uint32_t i = 0; i < NUMCASCADES; i++)
			projectionMatrices[i] = glm::ortho(-cascadeDistances[i], cascadeDistances[i], cascadeDistances[i], -cascadeDistances[i], 0.1f, 5000.f);
	}

	void UpdateMatrix(Camera* playerCamera, uint32_t imageIndex) override
	{
		float4x4 viewMatrix = glm::lookAt(playerCamera->position + offset, playerCamera->position, float3(0.0f, 0.0f, 1.0f));

		SunUniformBuffer* sub = (SunUniformBuffer*)this->viewProjBuffer[imageIndex]->Map();

		for (uint32_t i = 0; i < NUMCASCADES; i++)
			sub->viewProjs[i] = projectionMatrices[i] * viewMatrix;

		sub->dir = dir;

		this->viewProjBuffer[imageIndex]->UnMap();
	}
};

struct SpotLightThread
{
	Thread* thread;
	volatile bool go;
	volatile bool done;
	SpotLight* light;
	VulkanBackend* backend;
	VkRenderPassBeginInfo passInfo;
};

class SpotLight : public Light
{
public:
	float4 position; // W component is attenuation
	float4 dir; // W component is FOV
	Texture renderTarget;
	VkFramebuffer frameBuffer;
	std::vector<VkDescriptorSet> descriptorSetVS;
	std::vector<VkCommandBuffer> commandBuffers;
	float4x4 viewProj;
	VkCommandPool commandPool;
	VkDevice device;
	SpotLightThread thread;

	struct SpotLightBuffer
	{
		float4x4 viewProj;
		float4 dir;
		float4 pos;
	};

public:
	SpotLight(float3 position, float3 dir, float fov, float attenuation, uint32_t width, uint32_t height, VulkanBackend* backend);

	~SpotLight()
	{
		delete thread.thread;

		vkFreeCommandBuffers(device, commandPool, commandBuffers.size(), commandBuffers.data());
		vkDestroyCommandPool(device, commandPool, VK_NULL_HANDLE);

		vkDestroyFramebuffer(device, frameBuffer, VK_NULL_HANDLE);

		vkDestroyImage(device, renderTarget.Image, VK_NULL_HANDLE);
		vkDestroyImageView(device, renderTarget.View, VK_NULL_HANDLE);
		vkFreeMemory(device, renderTarget.Memory, VK_NULL_HANDLE);

		for (size_t i = 0; i < viewProjBuffer.size(); i++)
			delete viewProjBuffer[i];
	}

	void UpdateMatrix(Camera* activeCamera, uint32_t imageIndex) override
	{
		float4x4 viewMatrix = glm::lookAt((float3)this->position, (float3)this->position + (float3)this->dir, float3(0.0f, 0.0f, 1.0f));

		SpotLightBuffer* block = (SpotLightBuffer*)this->viewProjBuffer[imageIndex]->Map();

		block->viewProj = glm::perspective(this->dir.a, 1.0f, 0.1f, this->position.a);
		block->viewProj[1][1] *= -1;
		block->viewProj *= viewMatrix;
		block->dir = this->dir;
		block->pos = this->position;

		this->viewProjBuffer[imageIndex]->UnMap();
	}
};

class ComputeShader
{
	bool freeFilename;

	VkDevice device;
	VkPipeline pipeline;
	VkPipelineLayout pipelineLayout;
	uint32_t workGroupsX, workGroupsY, workGroupsZ;


	void GetInfoFromComp()
	{
		auto data = readFile(filename);

		const char* groupNames[] = {
			"local_size_x",
			"local_size_y",
			"local_size_z"
		};

		uint32_t* groups[] = {
			&workGroupsX, &workGroupsY, &workGroupsZ
		};

		char* ptr = data.data();
		char* endPtr = data.data() + data.size();
		uint32_t depth = 0;
		while (ptr < endPtr)
		{
			if (*ptr == '(' || *ptr == '{' || *ptr == '[')
				depth++;

			if (*ptr == ')' || *ptr == '}' || *ptr == ']')
				depth--;

			if (!depth)
			{
				if (*ptr == 'l')
				{
					if (StringCompare(ptr, "layout"))
					{
						while (*ptr++ != '(');

						while (*ptr != ')')
						{
							for (uint32_t i = 0; i < 3; i++)
							{
								if (StringCompare(ptr, groupNames[i]))
								{
									ptr += 13;
									while (*ptr++ != '=');
									*groups[i] = atoi(ptr);
									while (*ptr++ != ',');
								}
							}
							ptr++;
						}

					}
				}
			}
			ptr++;
		}

		for (uint32_t i = 0; i < 3; i++)
			if (!*groups[i]) *groups[i] = 1;

		printf("Groups: %u, %u, %u\n", workGroupsX, workGroupsY, workGroupsZ);
	}

	void ConvertFilename()
	{
		size_t len = strlen(filename);
		len += 5;
		spvFilename = (char*)malloc(len);
		if (!spvFilename)
			throw std::runtime_error("Failed to allocate memory in ComputeShader::ConvertFilename()!");

		ZEROMEM(spvFilename, len);

		char* outPtr = spvFilename;
		char* ptr = (char*)filename;
		while (*ptr != '.')
			*outPtr++ = *ptr++;

		StringConcatSafe(spvFilename, len, "_comp.spv");
	}

public:
	VkDescriptorSetLayout setLayout;
	const char* filename;
	char* spvFilename;
	size_t numUniformBuffers, numStorageBuffers, numStorageImages, numSamplers;
	std::filesystem::file_time_type lastModified;
	std::vector<VulkanMemory*> uniformBuffers;
	std::vector<VkDescriptorSet> descriptorSets;

	ComputeShader(VulkanBackend* backend, const char* filename, size_t numUniformBuffers, size_t numStorageBuffers, size_t numStorageImages, size_t numSamplers);

	static uint32_t InstanceFromGroup(uint32_t instances, uint32_t groups)
	{
		// The actual number of instances is divided among the group, rounding up.
		return ceil((float)instances / groups);
	}

	void Go(VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t instancesX, uint32_t instancesY, uint32_t instancesZ) const
	{
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSets[imageIndex], 0, VK_NULL_HANDLE);
		vkCmdDispatch(commandBuffer, InstanceFromGroup(instancesX, workGroupsX), InstanceFromGroup(instancesY, workGroupsY), InstanceFromGroup(instancesZ, workGroupsZ));
	}

	void UpdateDescriptorSets(VkDescriptorBufferInfo* uniformBufferInfos, VkDescriptorBufferInfo* storageBufferInfos, VkDescriptorImageInfo* storageImageInfos, VkDescriptorImageInfo* samplerInfos, uint32_t imageIndex)
	{
		uint32_t numDescriptors = numUniformBuffers + numStorageBuffers + numStorageImages + numSamplers;

		std::vector<VkWriteDescriptorSet> writes(numDescriptors);

		for (uint32_t i = 0; i < numDescriptors; i++)
		{
			writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[i].descriptorCount = 1;
			writes[i].dstSet = descriptorSets[imageIndex];
			writes[i].dstArrayElement = 0;
			writes[i].dstBinding = i;
			writes[i].pNext = VK_NULL_HANDLE;
		}

		auto builtInUniformBufferInfo = uniformBuffers[imageIndex]->GetBufferInfo();

		writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writes[0].pBufferInfo = &builtInUniformBufferInfo;

		uint32_t writeDex = 1;
		for (size_t i = 0; i < numUniformBuffers - 1; i++)
		{
			writes[writeDex].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			writes[writeDex++].pBufferInfo = &uniformBufferInfos[i];
		}

		for (size_t i = 0; i < numStorageBuffers; i++)
		{
			writes[writeDex].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			writes[writeDex++].pBufferInfo = &storageBufferInfos[i];
		}

		for (size_t i = 0; i < numSamplers; i++)
		{
			writes[writeDex].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writes[writeDex++].pImageInfo = &samplerInfos[i];
		}

		for (size_t i = 0; i < numStorageImages; i++)
		{
			writes[writeDex].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			writes[writeDex++].pImageInfo = &storageImageInfos[i];
		}

		vkUpdateDescriptorSets(device, numDescriptors, writes.data(), 0, VK_NULL_HANDLE);
	}

	~ComputeShader()
	{
		vkDestroyPipeline(device, pipeline, VK_NULL_HANDLE);
		vkDestroyPipelineLayout(device, pipelineLayout, VK_NULL_HANDLE);
		if (freeFilename)
			free((void*)filename);

		free((void*)spvFilename);
	}
};



struct RenderStage
{
	VkRenderPass renderPass;
	VkFramebuffer frameBuffer;
	std::vector<VkClearValue> clearValues;
	VkExtent2D extent;

	RenderStageType stageType;

	std::vector<RenderStageShaderGroup> shaderGroups;
	std::vector<int> meshIDs;

	Shader* shader; // It's basically an override. If NULL, it uses whatever shader the objects have

	std::vector<VkDescriptorSet> descriptorSet;

	// RPT_BLIT parameters
	Texture* srcImage;
	VkImageLayout srcLayout;
	VkImageAspectFlags srcAspect;
	uint32_t srcX, srcY;
	Texture* dstImage;
	VkImageAspectFlags dstAspect;
	uint32_t dstX, dstY;
	VkFilter blitFilter;
	VkImageLayout transitionSrc, transitionDst;
};

struct SunPassThreadInfo
{
	uint32_t cascade;
	VkRenderPassBeginInfo passInfo;
	Shader* opaqueShader, *maskedShader;
};

struct Timing
{
	float frametime;
	int triangle_count;
	int drawcall_count;
	int bound_pipelines;
	int bound_buffers;
	int api_calls;
	int blits;
	int passes;
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
#define AddObjectToExistingRenderProcess AddThingToExistingRenderProcess

#endif