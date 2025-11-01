#include "engineTypes.h"
#include "VulkanMemory.h"
#include "engine.h"

VulkanMemory::VulkanMemory(VkDeviceSize size, VkBufferUsageFlags usage, const char* origin, bool isStatic, void* data)
{
	LastGenEngine* engine = GetEngine();

	this->origin = origin;
	this->size = size;
	this->logicalDevice = engine->backend->logicalDevice;
	destroyed = false;

	if (isStatic)
	{
		engine->backend->CreateStaticBuffer(data, size, usage, buffer, memory);
		return;
	}

	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateBuffer(engine->backend->logicalDevice, &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
		throw std::runtime_error("failed to create buffer!");

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(engine->backend->logicalDevice, buffer, &memRequirements);
	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = engine->backend->findMemoryType(engine->backend->physicalDevice, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	VkResult vr;
	if ((vr = vkAllocateMemory(engine->backend->logicalDevice, &allocInfo, nullptr, &memory)) != VK_SUCCESS)
	{
		std::cout << String_VkResult(vr) << "\n";
		throw std::runtime_error("Failed to allocate buffer memory!");
	}

	vkBindBufferMemory(engine->backend->logicalDevice, buffer, memory, 0);

	if (data)
	{
		void* d = Map();
		memcpy(d, data, size);
		UnMap();
	}

	engine->allBuffers.push_back(this);
}

VulkanMemory::~VulkanMemory()
{
	vkDestroyBuffer(logicalDevice, buffer, nullptr);
	vkFreeMemory(logicalDevice, memory, nullptr);
	destroyed = true;
}

void* VulkanMemory::Map() const
{
	return Map(0, size, 0);
}

void* VulkanMemory::Map(VkDeviceSize offset, VkDeviceSize size) const
{
	return Map(offset, size, 0);
}

void* VulkanMemory::Map(VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags) const
{
	void* data;
	vkMapMemory(logicalDevice, memory, offset, size, flags, &data);
	return data;
}

void VulkanMemory::UnMap() const
{
	vkUnmapMemory(logicalDevice, this->memory);
}

VkDescriptorBufferInfo VulkanMemory::GetBufferInfo() const
{
	VkDescriptorBufferInfo info{};
	info.offset = 0;
	info.range = size;
	info.buffer = buffer;
	return info;
}