#include "DescriptorSet.h"
#include "VulkanBackend.h"
#include "../engine.h"

DescriptorSet::DescriptorSet(DescriptorSetCreateInfo& createInfo)
{
	info = createInfo;
	layout = *GetDescriptorSetLayout(info.numUniformBuffers, info.numPUniformBuffers, info.numSamplers, info.numStorageBuffers, info.numStorageImages);

	AllocateDescriptorSets(1, &layout, &set);
}

DescriptorSet::DescriptorSet(uint32_t numUniformBuffers, uint32_t numStorageBuffers, uint32_t numPUniformBuffers, uint32_t numSamplers, uint32_t numStorageImages)
{
	info.numUniformBuffers = numUniformBuffers;
	info.numStorageBuffers = numStorageBuffers;
	info.numPUniformBuffers = numPUniformBuffers;
	info.numSamplers = numSamplers;
	info.numStorageImages = numStorageImages;

	AllocateDescriptorSets(1, GetDescriptorSetLayout(info.numUniformBuffers, info.numPUniformBuffers, info.numSamplers, info.numStorageBuffers, info.numStorageImages), &set);
}

void DescriptorSet::Update(VkDescriptorBufferInfo* uniformBufferInfos, VkDescriptorBufferInfo* storageBufferInfos, VkDescriptorBufferInfo* uniformBufferPInfos, VkDescriptorImageInfo* samplerInfos, VkDescriptorImageInfo* storageImageInfos) const
{
	uint32_t numDescriptors = info.numUniformBuffers + info.numStorageBuffers + info.numPUniformBuffers + info.numSamplers + info.numStorageImages;
	std::vector<VkWriteDescriptorSet> writes(numDescriptors);

	for (size_t i = 0; i < numDescriptors; i++)
	{
		writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[i].descriptorCount = 1;
		writes[i].dstSet = set;
		writes[i].dstArrayElement = 0;
		writes[i].dstBinding = (uint32_t)i;
		writes[i].pNext = VK_NULL_HANDLE;
	}

	uint32_t writeDex = 0;
	for (size_t i = 0; i < info.numUniformBuffers; i++)
	{
		writes[writeDex].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writes[writeDex++].pBufferInfo = &uniformBufferInfos[i];
	}

	for (size_t i = 0; i < info.numStorageBuffers; i++)
	{
		writes[writeDex].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writes[writeDex++].pBufferInfo = &storageBufferInfos[i];
	}

	for (size_t i = 0; i < info.numPUniformBuffers; i++)
	{
		writes[writeDex].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writes[writeDex++].pBufferInfo = &uniformBufferPInfos[i];
	}

	for (size_t i = 0; i < info.numSamplers; i++)
	{
		writes[writeDex].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[writeDex++].pImageInfo = &samplerInfos[i];
	}

	for (uint32_t i = 0; i < info.numStorageImages; i++)
	{
		writes[writeDex].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		writes[writeDex++].pImageInfo = &storageImageInfos[i];
	}

	vkUpdateDescriptorSets(BACKEND->logicalDevice, numDescriptors, writes.data(), 0, VK_NULL_HANDLE);
}

void DescriptorSet::CreateDescriptorPool()
{
	uint32_t maxFramesInFlight = BACKEND->MAX_FRAMES_IN_FLIGHT;

	std::array<VkDescriptorPoolSize, 6> poolSizes{};

	for (uint32_t i = 0; i < 6; i++)
		poolSizes[i].descriptorCount = maxFramesInFlight;

	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[2].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[3].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[4].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[5].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolInfo.pPoolSizes = poolSizes.data();

	// Aside from the maximum number of individual descriptors that are available, we also need to specify the maximum number of descriptor sets that may be allocated:
	poolInfo.maxSets = static_cast<uint32_t>(maxFramesInFlight) + 32768;

	// The structure has an optional flag similar to command pools that determines if individual descriptor sets can be freed or not: VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT.
	// We're not going to touch the descriptor set after creating it, so we don't need this flag. You can leave flags to its default value of 0.

	if (vkCreateDescriptorPool(BACKEND->logicalDevice, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
		throw std::runtime_error("failed to create descriptor pool!");
}

void DescriptorSet::AllocateDescriptorSets(uint32_t numDescriptorSets, VkDescriptorSetLayout* pSetLayouts, VkDescriptorSet* out_sets)
{
	VkDescriptorSetAllocateInfo allocateInfo{};
	allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocateInfo.pNext = VK_NULL_HANDLE;
	allocateInfo.descriptorSetCount = numDescriptorSets;
	allocateInfo.descriptorPool = descriptorPool;
	allocateInfo.pSetLayouts = pSetLayouts;
	vkcheck(vkAllocateDescriptorSets(BACKEND->logicalDevice, &allocateInfo, out_sets), "Failed to AllocateDescriptorSets!");
}

void DescriptorSet::CreateDescriptorSetLayout(size_t vBuffers, size_t pBuffers, size_t numSamplers, size_t numStorageBuffers, size_t numStorageImages, VkDescriptorSetLayout* outLayout)
{
	VkDescriptorSetLayoutBinding* bindings = (VkDescriptorSetLayoutBinding*)malloc(sizeof(VkDescriptorSetLayoutBinding) * (vBuffers + pBuffers + numSamplers + numStorageBuffers + numStorageImages));

	check(bindings, "Failed to allocate memory for descriptor set layout!");

	int binding;
	for (binding = 0; binding < vBuffers; binding++)
	{
		// The first two fields specify the binding used in the shader and the type of descriptor, which is a uniform buffer object.
		// It is possible for the shader variable to represent an array of uniform buffer objects, and descriptorCount specifies the number of values in the array.
		// This could be used to specify a transformation for each of the bones in a skeleton for skeletal animation, for example.
		bindings[binding].binding = binding;
		bindings[binding].descriptorCount = 1;
		bindings[binding].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		bindings[binding].pImmutableSamplers = nullptr;
		bindings[binding].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	}

	for (size_t i = 0; i < numStorageBuffers; i++)
	{
		bindings[binding].binding = binding;
		bindings[binding].descriptorCount = 1;
		bindings[binding].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		bindings[binding].pImmutableSamplers = nullptr;
		bindings[binding].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

		binding++;
	}

	for (size_t i = 0; i < pBuffers; i++)
	{
		bindings[binding].binding = binding;
		bindings[binding].descriptorCount = 1;
		bindings[binding].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		bindings[binding].pImmutableSamplers = nullptr;
		bindings[binding].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		binding++;
	}

	for (size_t i = 0; i < numSamplers; i++)
	{
		bindings[binding].binding = binding;
		bindings[binding].descriptorCount = 1;
		bindings[binding].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		bindings[binding].pImmutableSamplers = nullptr;
		bindings[binding].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		binding++;
	}

	for (size_t i = 0; i < numStorageImages; i++)
	{
		bindings[binding].binding = binding;
		bindings[binding].descriptorCount = 1;
		bindings[binding].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		bindings[binding].pImmutableSamplers = nullptr;
		bindings[binding].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		binding++;
	}

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(vBuffers + pBuffers + numSamplers + numStorageBuffers + numStorageImages);
	layoutInfo.pBindings = bindings;

	if (vkCreateDescriptorSetLayout(BACKEND->logicalDevice, &layoutInfo, nullptr, outLayout) != VK_SUCCESS)
		throw std::runtime_error("failed to create descriptor set layout!");

	free(bindings);
}

uint32_t numSetLayouts = 0;
FullSetLayout allSetLayouts[100];

VkDescriptorSetLayout* DescriptorSet::GetDescriptorSetLayout(uint32_t numVBuffers, uint32_t numPBuffers, uint32_t numTextures, uint32_t numStorageBuffers, uint32_t numStorageImages)
{
	for (uint32_t i = 0; i < numSetLayouts; i++)
	{
		if (allSetLayouts[i].numPBuffers == numPBuffers &&
			allSetLayouts[i].numVBuffers == numVBuffers &&
			allSetLayouts[i].numSamplers == numTextures &&
			allSetLayouts[i].numStorageBuffers == numStorageBuffers &&
			allSetLayouts[i].numStorageImages == numStorageImages) {
			return &allSetLayouts[i].setLayout;
		}
	}

	check(numSetLayouts < 100, "allSetLayouts is too small!");

	allSetLayouts[numSetLayouts].numVBuffers = numVBuffers;
	allSetLayouts[numSetLayouts].numPBuffers = numPBuffers;
	allSetLayouts[numSetLayouts].numSamplers = numTextures;
	allSetLayouts[numSetLayouts].numStorageBuffers = numStorageBuffers;
	allSetLayouts[numSetLayouts].numStorageImages = numStorageImages;
	CreateDescriptorSetLayout(numVBuffers, numPBuffers, numTextures, numStorageBuffers, numStorageImages, &allSetLayouts[numSetLayouts].setLayout);
	return &allSetLayouts[numSetLayouts++].setLayout;
}



void DescriptorSet::DestroyAllSetLayouts()
{
	VkDevice device = BACKEND->logicalDevice;

	for (uint32_t i = 0; i < numSetLayouts; i++)
		vkDestroyDescriptorSetLayout(device, allSetLayouts[i].setLayout, VK_NULL_HANDLE);

	numSetLayouts = 0;
}