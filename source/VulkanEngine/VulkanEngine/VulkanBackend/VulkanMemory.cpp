#include "../engineTypes.h"
#include "VulkanMemory.h"
#include "../engine.h"
#include "VulkanBackend.h"

std::vector<GPUMemory*> allBuffers = {};

static uint32_t findMemoryType(VkPhysicalDevice device, uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(device, &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}

	throw std::runtime_error("failed to find suitable memory type!");
}

GPUMemory::GPUMemory(VkDeviceSize size, VkBufferUsageFlags usage, const char* origin, bool isStatic, void* data)
{
	LastGenEngine* engine = GetEngine();

	this->origin = origin;
	this->size = size;
	this->logicalDevice = BACKEND->logicalDevice;
	destroyed = false;

	if (isStatic)
	{
		CreateStaticBuffer(data, size, usage, buffer, memory);
		return;
	}

	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateBuffer(BACKEND->logicalDevice, &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
		throw std::runtime_error("failed to create buffer!");

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(BACKEND->logicalDevice, buffer, &memRequirements);
	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(BACKEND->physicalDevice, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	VkResult vr;
	if ((vr = vkAllocateMemory(BACKEND->logicalDevice, &allocInfo, nullptr, &memory)) != VK_SUCCESS)
	{
		std::cout << String_VkResult(vr) << "\n";
		throw std::runtime_error("Failed to allocate buffer memory!");
	}

	vkBindBufferMemory(BACKEND->logicalDevice, buffer, memory, 0);

	if (data)
	{
		void* d = Map();
		memcpy(d, data, size);
		UnMap();
	}

	allBuffers.push_back(this);
}

GPUMemory::~GPUMemory()
{
	vkDestroyBuffer(logicalDevice, buffer, nullptr);
	vkFreeMemory(logicalDevice, memory, nullptr);
	destroyed = true;
}

void* GPUMemory::Map() const
{
	return Map(0, size, 0);
}

void* GPUMemory::Map(VkDeviceSize offset, VkDeviceSize size) const
{
	return Map(offset, size, 0);
}

void* GPUMemory::Map(VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags) const
{
	void* data;
	vkMapMemory(logicalDevice, memory, offset, size, flags, &data);
	return data;
}

void GPUMemory::UnMap() const
{
	vkUnmapMemory(logicalDevice, this->memory);
}

VkDescriptorBufferInfo GPUMemory::GetBufferInfo() const
{
	VkDescriptorBufferInfo info{};
	info.offset = 0;
	info.range = size;
	info.buffer = buffer;
	return info;
}

void GPUMemory::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory)
{
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateBuffer(logicalDevice, &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
		throw std::runtime_error("failed to create buffer!");

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(logicalDevice, buffer, &memRequirements);
	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(BACKEND->physicalDevice, memRequirements.memoryTypeBits, properties);

	if (vkAllocateMemory(logicalDevice, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS)
		throw std::runtime_error("failed to allocate buffer memory!");

	vkBindBufferMemory(logicalDevice, buffer, bufferMemory, 0);
}

static void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
	auto backend = BACKEND;
	VkCommandBuffer commandBuffer = backend->beginSingleTimeCommands();

	VkBufferCopy copyRegion{};
	copyRegion.size = size;
	vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

	backend->endSingleTimeCommands(commandBuffer);
}

void GPUMemory::CreateStaticBuffer(void* data, size_t dataSize, VkBufferUsageFlags usage, VkBuffer& buffer, VkDeviceMemory& memory)
{
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	createBuffer(dataSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, buffer, memory);

	if (data)
	{
		createBuffer(dataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

		void* mapped;
		vkMapMemory(logicalDevice, stagingBufferMemory, 0, dataSize, 0, &mapped);
		memcpy(mapped, data, dataSize);
		vkUnmapMemory(logicalDevice, stagingBufferMemory);

		copyBuffer(stagingBuffer, buffer, dataSize);

		vkDestroyBuffer(logicalDevice, stagingBuffer, nullptr);
		vkFreeMemory(logicalDevice, stagingBufferMemory, nullptr);
	}
}