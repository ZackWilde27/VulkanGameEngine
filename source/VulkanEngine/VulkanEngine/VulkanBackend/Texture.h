#pragma once
#include "../engineTypes.h"

#include <stb_image_write.h>

void TextureWriteFunc(std::ofstream* context, void* data, int size);

class Texture
{
	bool uniqueSampler;

public:
	zstring<CHAR_T>* filename;
	VkImage image;
	VkImageView view;
	VkDeviceMemory memory;
	VkSampler sampler;
	VkImageAspectFlags aspect;
	uint3 size;
	uint32_t mipLevels;
	uint32_t layerCount;
	VkImageLayout theoreticalLayout; // This is used when reading the render stage from engine.lua, to anticipate layout errors before they happen
	std::vector<VkImageLayout> layout; // This layout is the texture's current layout (for each mip), for updating descriptor sets in lua
	VkFormat format;

private:
	VkSampler CreateTextureSampler(struct SamplerSettings& settings);
	void GetTextureSampler(struct SamplerSettings& settings, bool addToList = true);
	void CreateImageView(VkImageViewType viewType, VkImageViewCreateFlags flags);

public:
	Texture(const CHAR_T* filename, bool isNonColour);
	Texture(VkImageType imageType, VkImageViewType imageViewType, VkFormat imageFormat, uint32_t width, uint32_t height, uint32_t depth, int mipLevels, int arrayLayers, VkSampleCountFlagBits sampleCount, VkImageTiling imageTiling, VkImageUsageFlags usage, VkImageAspectFlags imageAspectFlags, VkFilter magFilter, VkFilter minFilter, VkSamplerAddressMode samplerAddressMode, bool addSamplerToList, class VulkanBackend* backend);
	~Texture();

	VkDescriptorImageInfo GetImageInfo() const;
	VkDescriptorImageInfo GetImageInfo(VkImageLayout layoutOverride) const;

	void CopyFromBuffer(void* data, VkDeviceSize sz) const;
	void CopyFromBuffer(GPUMemory* buffer) const;

	std::vector<float4> CopyToBuffer();

	void GenerateMipMaps();

	// This overload records it to a command buffer to be executed later
	// A NULL area means the entire texture
	// A layout of UNDEFINED means to get it from the texture itself, if the texture itself is also UNDEFINED, it uses SHADER_READ_ONLY_OPTIMAL
	void BlitTo(VkCommandBuffer commandBuffer, Texture* dst, VkFilter filter=VK_FILTER_LINEAR, Rect* srcArea=NULL, uint32_t srcMip=0, uint32_t srcLayer=0, Rect* dstArea=NULL, uint32_t dstMip=0, uint32_t dstLayer=0, VkImageLayout srcFinalLayout=VK_IMAGE_LAYOUT_UNDEFINED, VkImageLayout dstFinalLayout=VK_IMAGE_LAYOUT_UNDEFINED, VkImageLayout srcStartLayout=VK_IMAGE_LAYOUT_UNDEFINED, VkImageLayout dstStartLayout=VK_IMAGE_LAYOUT_UNDEFINED);

	// This overload blits immediately, blocking until that's done
	// A NULL area means the entire texture
	// A layout of UNDEFINED means to get it from the texture itself, if the texture itself is also UNDEFINED, it uses SHADER_READ_ONLY_OPTIMAL
	void BlitTo(Texture* dst, VkFilter filter=VK_FILTER_LINEAR, Rect* srcArea=NULL, uint32_t srcMip=0, uint32_t srcLayer=0, Rect* dstArea=NULL, uint32_t dstMip=0, uint32_t dstLayer=0, VkImageLayout srcFinalLayout=VK_IMAGE_LAYOUT_UNDEFINED, VkImageLayout dstFinalLayout=VK_IMAGE_LAYOUT_UNDEFINED);

	// This overload records it to a given command buffer to be executed later
	// If mip or layer is -1, it means all of them
	void TransitionImageLayout(VkCommandBuffer commandBuffer, VkImageLayout newLayout, int mip=-1, int layer=-1);

	// This overload will do the transition immediately, blocking until that's done
	// If mip or layer is -1, it means all of them
	void TransitionImageLayout(VkImageLayout newLayout, int mip = -1, int layer = -1);

	template <typename T>
	void SaveToPNG(const T* filename)
	{
		std::vector<float4> fullData = CopyToBuffer();

		std::ofstream file(filename, std::ios_base::binary);
		stbi_write_png_to_func((stbi_write_func*)TextureWriteFunc, &file, size.x, size.y, 4, fullData.data(), size.x * sizeof(float4));
		file.close();
	}

	static Texture*& LoadTexture(const CHAR_T* filename, bool isNonColour);
	// Returns a reference to the texture's spot in the array
	static Texture*& AddTexture(Texture* tex);
	static void DestroyAllTextures();
};