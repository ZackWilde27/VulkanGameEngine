#pragma once
#include "engineTypes.h"

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
	// isStatic is whether or not the buffer can be updated by the CPU (static buffers are much faster for the GPU)
	// If static, data is what's copied to the buffer at the start before it's no longer possible to update it
	VulkanMemory(VkDeviceSize size, VkBufferUsageFlags usage, const char* origin, bool isStatic, void* data);
	~VulkanMemory();

	void* Map() const;
	void* Map(VkDeviceSize offset, VkDeviceSize size) const;
	void* Map(VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags) const;

	template<typename T>
	T* Map() const
	{
		return Map<T>(0);
	}

	template<typename T>
	T* Map(VkDeviceSize offset) const
	{
		return (T*)Map(offset, sizeof(T));
	}

	void UnMap() const;

	VkDescriptorBufferInfo GetBufferInfo() const;

	operator VkBuffer() const { return this->buffer; }

private:
	void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
	void CreateStaticBuffer(void* data, size_t dataSize, VkBufferUsageFlags usage, VkBuffer& buffer, VkDeviceMemory& memory);
};