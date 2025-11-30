#pragma once
#include "../engineTypes.h"

// Now it's starting to look like Vulkan, but I can see why they did that, it's a little more tedious to write but much easier to read than 'new DescriptorSet(0, 0, 0, 1, 0)'
struct DescriptorSetCreateInfo
{
	uint32_t numUniformBuffers, numStorageBuffers, numPUniformBuffers, numSamplers, numStorageImages;
};

class DescriptorSet
{
	DescriptorSetCreateInfo info;

public:
	VkDescriptorSet set;
	VkDescriptorSetLayout layout;
	static inline VkDescriptorPool descriptorPool;

	operator const VkDescriptorSet* () const { return &set; }
	operator VkDescriptorSet* () const { return (VkDescriptorSet*)&set; }
	operator VkDescriptorSet() const { return set; }

	DescriptorSet(DescriptorSetCreateInfo& createInfo);
	// I mean you can still do that if you want the convenience
	DescriptorSet(uint32_t numUniformBuffers, uint32_t numStorageBuffers, uint32_t numPUniformBuffers, uint32_t numSamplers, uint32_t numStorageImages);

	void Update(VkDescriptorBufferInfo* uniformBufferInfos, VkDescriptorBufferInfo* storageBufferInfos, VkDescriptorBufferInfo* uniformBufferPInfos, VkDescriptorImageInfo* samplerInfos, VkDescriptorImageInfo* storageImageInfos) const;

	static void CreateDescriptorPool();
	static void AllocateDescriptorSets(uint32_t numDescriptorSets, VkDescriptorSetLayout* pSetLayouts, VkDescriptorSet* out_sets);
	static void CreateDescriptorSetLayout(size_t vBuffers, size_t pBuffers, size_t numSamplers, size_t numStorageBuffers, size_t numStorageImages, VkDescriptorSetLayout* outLayout);
	static VkDescriptorSetLayout* GetDescriptorSetLayout(uint32_t numVBuffers, uint32_t numPBuffers, uint32_t numTextures, uint32_t numStorageBuffers = 0, uint32_t numStorageImages = 0);

	static void DestroyAllSetLayouts();
	
};