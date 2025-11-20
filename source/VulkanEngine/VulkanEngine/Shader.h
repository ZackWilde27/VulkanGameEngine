#pragma once
#include "engineTypes.h"

class Shader
{
private:
	VkDevice device;

public:
	VkPipeline pipeline;
	VkPipelineLayout pipelineLayout;

	zstring<CHAR_T>* zlslFile;
	std::filesystem::file_time_type mtime;
	zstring<CHAR_T>* vertexShader;
	zstring<CHAR_T>* pixelShader;
	int shaderType;
	VkCullModeFlags cullMode;
	VkPolygonMode polygonMode;
	VkRenderPass renderPass;
	VkSampleCountFlagBits sampleCount;
	BlendMode blendMode;
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

private:
	VkShaderModule CreateShaderModule(const std::vector<char>& code);

	void CreatePipeline(const CHAR_T* vertfilename, const CHAR_T* pixlfilename, VkExtent2D screenSize);
	void DestroyPipeline();

	std::vector<VkDescriptorSetLayout> GetDescriptorSetLayoutsFromZLSL(const CHAR_T* filename, uint32_t* outAttachments);

public:
	Shader(const CHAR_T* zlsl, const CHAR_T* vertfilename, const CHAR_T* pixlfilename, VkRenderPass renderPass, int shaderType, VkExtent2D screenSize, VkCullModeFlags cullMode, VkPolygonMode polygonMode, VkSampleCountFlagBits sampleCount, BlendMode blendMode, bool depthTest, bool depthWrite, VkPushConstantRange* pushConstantRanges, uint32_t numPushConstantRanges, uint32_t stencilWriteMask, VkCompareOp stencilTestOp, uint32_t stencilTestValue, float depthBias, bool masked, VulkanBackend* backend);
#ifdef WIDE_STRINGS
	Shader(const char* zlsl, const char* vertfilename, const char* pixlfilename, VkRenderPass renderPass, int shaderType, VkExtent2D screenSize, VkCullModeFlags cullMode, VkPolygonMode polygonMode, VkSampleCountFlagBits sampleCount, BlendMode blendMode, bool depthTest, bool depthWrite, VkPushConstantRange* pushConstantRanges, uint32_t numPushConstantRanges, uint32_t stencilWriteMask, VkCompareOp stencilTestOp, uint32_t stencilTestValue, float depthBias, bool masked, VulkanBackend* backend);
#endif
	~Shader();

	void Recompile(const CHAR_T* vertfilename, const CHAR_T* pixlfilename, VkExtent2D screenSize);
};