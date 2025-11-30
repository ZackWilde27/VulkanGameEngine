#pragma once
#include "../engineTypes.h"

enum CullMode
{
	CULL_MODE_NONE = VK_CULL_MODE_NONE,
	CULL_MODE_BACK = VK_CULL_MODE_BACK_BIT,
	CULL_MODE_FRONT = VK_CULL_MODE_FRONT_BIT,
	CULL_MODE_BOTH = VK_CULL_MODE_FRONT_AND_BACK
};

enum PolygonMode
{
	POLYGON_MODE_FILL = VK_POLYGON_MODE_FILL,
	POLYGON_MODE_LINE = VK_POLYGON_MODE_LINE,
	POLYGON_MODE_POINT = VK_POLYGON_MODE_POINT
};

enum ImageLayout
{
	IMAGE_LAYOUT_UNDEFINED = VK_IMAGE_LAYOUT_UNDEFINED,
	IMAGE_LAYOUT_READ_ONLY = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
	IMAGE_LAYOUT_GENERAL = VK_IMAGE_LAYOUT_GENERAL,
	IMAGE_LAYOUT_TRANSFER_SRC = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
	IMAGE_LAYOUT_TRANSFER_DST = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
	IMAGE_LAYOUT_RENDER_TARGET = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
	IMAGE_LAYOUT_DEPTH_TARGET = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
};

enum Filter
{
	FILTER_LINEAR = VK_FILTER_LINEAR,
	FILTER_NEAREST = VK_FILTER_NEAREST
};

struct RenderPassFromToLayout
{
	ImageLayout from;
	ImageLayout to;
};

struct RenderPass
{
	VkRenderPass renderPass;
	std::vector<RenderPassFromToLayout> layouts;
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
	GPUMemory* matrixMem;
	GPUMemory* shadowMapOffsetsMem;
	DescriptorSet* descriptorSet;
	float3 boundingBoxMin;
	float3 boundingBoxMax;
	float3 boundingBoxCentre;
	Bool isStatic;
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
	Texture** srcImage;
	ImageLayout srcLayout;
	VkImageAspectFlags srcAspect;
	uint32_t srcX, srcY;
	Texture** dstImage;
	VkImageAspectFlags dstAspect;
	uint32_t dstX, dstY;
	VkFilter blitFilter;
	ImageLayout transitionSrc, transitionDst;
};

struct Material
{
	std::vector<Texture*> textures;
	Shader* shader;
	DescriptorSet* descriptorSets[2]; // The first one contains all textures, the second one only has the colour texture, for alpha testing on shadow maps
	bool masked; // Whether or not to alpha test on shadow maps, whether or not the shader does discarding does not affect it
	float roughness; // This will act as a multiplier for the roughness texture, so you can make a material rougher or shinier
};

struct FullSampler
{
	VkSampler sampler;
	SamplerSettings settings;
};

struct FullSetLayout
{
	uint32_t numVBuffers, numPBuffers, numSamplers, numStorageBuffers, numStorageImages;
	VkDescriptorSetLayout setLayout;
};