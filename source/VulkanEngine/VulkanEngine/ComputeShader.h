#pragma once
#include "engineTypes.h"

class ComputeShader
{
private:
	VkDevice device;
	VkPipeline pipeline;
	VkPipelineLayout pipelineLayout;
	uint3 workGroups;

public:
	VkDescriptorSetLayout setLayout;
	zstring<wchar_t>* filename;
	wchar_t* spvFilename;
	uint32_t numUniformBuffers, numStorageBuffers, numStorageImages, numSamplers;
	std::filesystem::file_time_type lastModified;
	std::vector<VulkanMemory*> uniformBuffers;
	std::vector<VkDescriptorSet> descriptorSets;

private:
	void GetInfoFromComp();
	void ConvertFilename();
	void MakePipeline(VulkanBackend* backend);
	void DestroyPipeline() const;

public:
	ComputeShader(VulkanBackend* backend, const wchar_t* filename, uint32_t numUniformBuffers, uint32_t numStorageBuffers, uint32_t numStorageImages, uint32_t numSamplers);
	~ComputeShader();

	void RemakePipeline(VulkanBackend* backend);

	void Go(VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t instancesX, uint32_t instancesY, uint32_t instancesZ) const;
	void UpdateDescriptorSets(VkDescriptorBufferInfo* uniformBufferInfos, VkDescriptorBufferInfo* storageBufferInfos, VkDescriptorImageInfo* storageImageInfos, VkDescriptorImageInfo* samplerInfos, uint32_t imageIndex);

	static uint32_t InstanceFromGroup(uint32_t instances, uint32_t groups);
};