#include "../engineTypes.h"
#include "D3D12Memory.h"
#include "../include/directx/d3dx12.h"
#include "../engine.h"

GPUMemory::GPUMemory(size_t size, VkBufferUsageFlags usage, const char* origin, bool isStatic, void* data)
{
	CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);

	//GetEngine()->backend->device;

	destroyed = false;
}

GPUMemory::~GPUMemory()
{
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
	allocInfo.memoryTypeIndex = findMemoryType(GetEngine()->backend->physicalDevice, memRequirements.memoryTypeBits, properties);

	if (vkAllocateMemory(logicalDevice, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS)
		throw std::runtime_error("failed to allocate buffer memory!");

	vkBindBufferMemory(logicalDevice, buffer, bufferMemory, 0);
}

static void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
	auto backend = (VulkanBackend*)GetEngine()->backend;
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