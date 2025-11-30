#include "ComputeShader.h"
#include "DescriptorSet.h"
#include "VulkanBackend.h"
#include "../engine.h"

struct ComputeDescriptorSetLayout
{
	VkDescriptorSetLayout layout;
	size_t buffers, textures;
};

std::vector<VkDescriptorSetLayout> computeDescriptorSetLayouts;

static VkDescriptorSetLayout* GetComputeDescriptorSet(size_t numUniformBuffers, size_t numStorageBuffers, size_t numStorageTextures, size_t numSamplers)
{
	std::vector<VkDescriptorSetLayoutBinding> bindings;
	uint32_t binding = 0;

	for (size_t i = 0; i < numUniformBuffers; i++)
	{
		bindings.push_back({});
		bindings.back().binding = binding++;
		bindings.back().descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		bindings.back().stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		bindings.back().descriptorCount = 1;
		bindings.back().pImmutableSamplers = VK_NULL_HANDLE;
	}

	for (size_t i = 0; i < numStorageBuffers; i++)
	{
		bindings.push_back({});
		bindings.back().binding = binding++;
		bindings.back().descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		bindings.back().stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		bindings.back().descriptorCount = 1;
		bindings.back().pImmutableSamplers = VK_NULL_HANDLE;
	}

	for (size_t i = 0; i < numSamplers; i++)
	{
		bindings.push_back({});
		bindings.back().binding = binding++;
		bindings.back().descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		bindings.back().stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		bindings.back().descriptorCount = 1;
		bindings.back().pImmutableSamplers = VK_NULL_HANDLE;
	}

	for (size_t i = 0; i < numStorageTextures; i++)
	{
		bindings.push_back({});
		bindings.back().binding = binding++;
		bindings.back().descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		bindings.back().stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		bindings.back().descriptorCount = 1;
		bindings.back().pImmutableSamplers = VK_NULL_HANDLE;
	}

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();
	layoutInfo.flags = VK_FLAGS_NONE;
	layoutInfo.pNext = VK_NULL_HANDLE;

	computeDescriptorSetLayouts.push_back(NULL);

	vkCreateDescriptorSetLayout(((VulkanBackend*)GetEngine()->backend)->logicalDevice, &layoutInfo, VK_NULL_HANDLE, &computeDescriptorSetLayouts.back());

	return &computeDescriptorSetLayouts.back();
}

ComputeShader::ComputeShader(VulkanBackend* backend, const CHAR_T* filename, uint32_t numUniformBuffers, uint32_t numStorageBuffers, uint32_t numStorageImages, uint32_t numSamplers)
{
	this->filename = new zstring(filename);
	this->numUniformBuffers = numUniformBuffers;
	this->numStorageBuffers = numStorageBuffers;
	this->numStorageImages = numStorageImages;
	this->numSamplers = numSamplers;

	uniformBuffers.resize(backend->MAX_FRAMES_IN_FLIGHT);
	descriptorSets.resize(backend->MAX_FRAMES_IN_FLIGHT);

	setLayout = *GetComputeDescriptorSet(numUniformBuffers, numStorageBuffers, numStorageImages, numSamplers);

	for (uint32_t i = 0; i < backend->MAX_FRAMES_IN_FLIGHT; i++)
		DescriptorSet::AllocateDescriptorSets(1, &setLayout, &descriptorSets[i]);

	lastModified = FileDate(filename);

	ConvertFilename();
	GetInfoFromComp();

	device = backend->logicalDevice;

	MakePipeline(backend);
}

ComputeShader::~ComputeShader()
{
	delete filename;

	DestroyPipeline();

	vkDestroyDescriptorSetLayout(device, setLayout, VK_NULL_HANDLE);

	for (auto buffer : uniformBuffers)
		delete buffer;

	free((void*)spvFilename);
}

void ComputeShader::GetInfoFromComp()
{
	auto data = readFile((CHAR_T*)*filename);

	const char* groupNames[] = {
		"local_size_x",
		"local_size_y",
		"local_size_z"
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
								workGroups[i] = atoi(ptr);
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
		if (!workGroups[i]) workGroups[i] = 1;

	printf("Groups: %u, %u, %u\n", workGroups.x, workGroups.y, workGroups.z);
}

void ComputeShader::ConvertFilename()
{
	size_t len = filename->Length() * 2;
	len += 20;
	spvFilename = (CHAR_T*)malloc(len);
	if (!spvFilename)
		throw std::runtime_error("Failed to allocate memory in ComputeShader::ConvertFilename()!");

	ZEROMEM(spvFilename, len);

	CHAR_T* outPtr = spvFilename;
	CHAR_T* ptr = *filename;
	while (*ptr != L'.')
		*outPtr++ = *ptr++;

	StringConcatSafe(spvFilename, len, STRING("_comp.spv"));
}

void ComputeShader::DestroyPipeline() const
{
	vkDestroyPipeline(device, pipeline, VK_NULL_HANDLE);
	vkDestroyPipelineLayout(device, pipelineLayout, VK_NULL_HANDLE);
}

void ComputeShader::UpdateDescriptorSets(VkDescriptorBufferInfo* uniformBufferInfos, VkDescriptorBufferInfo* storageBufferInfos, VkDescriptorImageInfo* storageImageInfos, VkDescriptorImageInfo* samplerInfos, uint32_t imageIndex)
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

	uint32_t writeDex = 0;
	for (uint32_t i = 0; i < numUniformBuffers; i++)
	{
		writes[writeDex].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writes[writeDex++].pBufferInfo = &uniformBufferInfos[i];
	}

	for (uint32_t i = 0; i < numStorageBuffers; i++)
	{
		writes[writeDex].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writes[writeDex++].pBufferInfo = &storageBufferInfos[i];
	}

	for (uint32_t i = 0; i < numSamplers; i++)
	{
		writes[writeDex].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[writeDex++].pImageInfo = &samplerInfos[i];
	}

	for (uint32_t i = 0; i < numStorageImages; i++)
	{
		writes[writeDex].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		writes[writeDex++].pImageInfo = &storageImageInfos[i];
	}

	vkUpdateDescriptorSets(device, numDescriptors, writes.data(), 0, VK_NULL_HANDLE);
}

void ComputeShader::Go(VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t instancesX, uint32_t instancesY, uint32_t instancesZ) const
{
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSets[imageIndex], 0, VK_NULL_HANDLE);
	vkCmdDispatch(commandBuffer, InstanceFromGroup(instancesX, workGroups.x), InstanceFromGroup(instancesY, workGroups.y), InstanceFromGroup(instancesZ, workGroups.z));
}

void ComputeShader::MakePipeline(VulkanBackend* backend)
{
	auto shaderModule = backend->createShaderModule(readFile(spvFilename));

	VkPipelineLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutInfo.pNext = VK_NULL_HANDLE;
	layoutInfo.flags = VK_FLAGS_NONE;
	layoutInfo.pSetLayouts = &setLayout;
	layoutInfo.setLayoutCount = 1;
	layoutInfo.pPushConstantRanges = VK_NULL_HANDLE;
	layoutInfo.pushConstantRangeCount = 0;
	vkCreatePipelineLayout(device, &layoutInfo, VK_NULL_HANDLE, &pipelineLayout);

	VkComputePipelineCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	createInfo.pNext = VK_NULL_HANDLE;
	createInfo.layout = pipelineLayout;
	createInfo.basePipelineHandle = VK_NULL_HANDLE;
	createInfo.basePipelineIndex = 0;
	createInfo.flags = VK_FLAGS_NONE;

	createInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	createInfo.stage.pName = "main";
	createInfo.stage.module = shaderModule;
	createInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	createInfo.stage.pNext = VK_NULL_HANDLE;
	createInfo.stage.flags = VK_FLAGS_NONE;

	vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &createInfo, VK_NULL_HANDLE, &pipeline);

	vkDestroyShaderModule(device, shaderModule, VK_NULL_HANDLE);
}

void ComputeShader::RemakePipeline(VulkanBackend* backend)
{
	DestroyPipeline();
	MakePipeline(backend);
}

uint32_t ComputeShader::InstanceFromGroup(uint32_t instances, uint32_t groups)
{
	// The actual number of instances is divided among the group, rounding up.
	return (uint32_t)ceil((float)instances / groups);
}