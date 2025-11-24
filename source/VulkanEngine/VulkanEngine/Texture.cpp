#include "engineTypes.h"
#include "Texture.h"
#include "zstring.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STBI_MSC_SECURE_CRT
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <iostream>
#include "engineUtils.h"
#include "engine.h"
#include "VulkanBackend.h"
#include "VulkanMemory.h"

// Helper function that takes care of error handling, size, and the mip levels calculation
template<typename T>
static stbi_uc* LoadImageFromDisk(const T* filename, uint32_t* outWidth, uint32_t* outHeight, int* outChannels, int requiredChannels, VkDeviceSize* outSize, uint32_t* outMipLevels)
{
	auto buffer = readFile(filename);
	auto pixels = stbi_load_from_memory((stbi_uc*)buffer.data(), buffer.size(), (int*)outWidth, (int*)outHeight, outChannels, requiredChannels);

	if (!pixels)
	{
		std::wcout << filename << "\n";
		throw std::runtime_error("failed to load texture image!");
	}

	*outSize = ((unsigned long long) * outWidth) * (*outHeight) * 4;
	*outMipLevels = static_cast<uint32_t>(std::floor(std::log2(MAX(*outWidth, *outHeight)))) + 1;

	return pixels;
}

void Texture::GenerateMipMaps()
{
	VulkanBackend* backend = GetEngine()->backend;

	VkFormatProperties formatProperties;
	vkGetPhysicalDeviceFormatProperties(backend->physicalDevice, format, &formatProperties);

	if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
		throw std::runtime_error("texture image format does not support linear blitting!");

	VkCommandBuffer commandBuffer = backend->beginSingleTimeCommands();

	int32_t mipWidth = size.x;
	int32_t mipHeight = size.y;

	for (uint32_t i = 1; i < mipLevels; i++) {
		TransitionImageLayout(commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, i - 1);

		// Next, we specify the regions that will be used in the blit operation. The source mip level is i - 1 and the destination mip level is i. The two elements of the srcOffsets array determine the 3D region that data will be blitted from. dstOffsets determines the region that data will be blitted to. The X and Y dimensions of the dstOffsets[1] are divided by two since each mip level is half the size of the previous level. The Z dimension of srcOffsets[1] and dstOffsets[1] must be 1, since a 2D image has a depth of 1.
		VkImageBlit blit{};
		blit.srcOffsets[0] = { 0, 0, 0 };
		blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
		blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.srcSubresource.mipLevel = i - 1;
		blit.srcSubresource.baseArrayLayer = 0;
		blit.srcSubresource.layerCount = layerCount;
		blit.dstOffsets[0] = { 0, 0, 0 };
		blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
		blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.dstSubresource.mipLevel = i;
		blit.dstSubresource.baseArrayLayer = 0;
		blit.dstSubresource.layerCount = layerCount;

		vkCmdBlitImage(commandBuffer,
			image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &blit,
			VK_FILTER_LINEAR);

		TransitionImageLayout(commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, i - 1);

		if (mipWidth > 1) mipWidth /= 2;
		if (mipHeight > 1) mipHeight /= 2;
	}

	TransitionImageLayout(commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, mipLevels - 1);

	backend->endSingleTimeCommands(commandBuffer);
}

static uint32_t FindMemoryType(VkPhysicalDevice device, uint32_t typeFilter, VkMemoryPropertyFlags properties)
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

static void CreateImage(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, VkImageType imageType, uint32_t width, uint32_t height, uint32_t depth, uint32_t mipLevels, uint32_t arrayLayers, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory) {

	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = imageType;
	imageInfo.extent.width = width;
	imageInfo.extent.height = height;
	imageInfo.extent.depth = depth;
	imageInfo.mipLevels = mipLevels;
	imageInfo.flags = (arrayLayers == 6) ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;
	imageInfo.arrayLayers = arrayLayers;
	imageInfo.format = format;
	imageInfo.tiling = tiling;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = usage;
	imageInfo.samples = numSamples;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	vkcheck(vkCreateImage(logicalDevice, &imageInfo, nullptr, &image), "failed to create image!");

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(logicalDevice, image, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = FindMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties);

	vkcheck(vkAllocateMemory(logicalDevice, &allocInfo, nullptr, &imageMemory), "failed to allocate image memory!");

	vkBindImageMemory(logicalDevice, image, imageMemory, 0);
}

Texture::Texture(const CHAR_T* path, bool isNonColour)
{
	VulkanBackend* backend = GetEngine()->backend;

	filename = new zstring(path);
	aspect = VK_IMAGE_ASPECT_COLOR_BIT;
	format = isNonColour ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_R8G8B8A8_SRGB;
	theoreticalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	layerCount = 1;
	uniqueSampler = false;

	int texChannels;
	VkDeviceSize imageSize;
	stbi_uc* pixels = LoadImageFromDisk(path, &size.x, &size.y, &texChannels, STBI_rgb_alpha, &imageSize, &mipLevels);
	size.z = 1;

	layout.resize(mipLevels);
	for (uint32_t i = 0; i < mipLevels; i++)
		layout[i] = VK_IMAGE_LAYOUT_UNDEFINED;

	CreateImage(backend->logicalDevice, backend->physicalDevice, VK_IMAGE_TYPE_2D, size.x, size.y, 1, mipLevels, 1, VK_SAMPLE_COUNT_1_BIT, format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, image, memory);
	TransitionImageLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	CopyFromBuffer(pixels, imageSize);
	stbi_image_free(pixels);

	GenerateMipMaps();
	CreateImageView(VK_IMAGE_VIEW_TYPE_2D, VK_FLAGS_NONE);

	SamplerSettings s{ VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, mipLevels };
	GetTextureSampler(s, true);
}

Texture* allTextures[MAX_TEXTURES];
TEXTURE_INDEX numTextures = 0;

Texture*& Texture::LoadTexture(const CHAR_T* path, bool isNonColour)
{
	if (!FileExists(path))
		return LoadTexture(STRING("textures/error_placeholder.png"), true);

	for (TEXTURE_INDEX i = 0; i < numTextures; i++)
	{
		if (allTextures[i] && allTextures[i]->filename && *allTextures[i]->filename == path)
			return allTextures[i];
	}

	if (numTextures < MAX_TEXTURES)
		return allTextures[numTextures++] = new Texture(path, isNonColour);

	throw std::runtime_error("Ran out of room for new textures!");
}

Texture::Texture(VkImageType imageType, VkImageViewType imageViewType, VkFormat imageFormat, uint32_t width, uint32_t height, uint32_t depth, int mipLevels, int arrayLayers, VkSampleCountFlagBits sampleCount, VkImageTiling imageTiling, VkImageUsageFlags usage, VkImageAspectFlags imageAspectFlags, VkFilter magFilter, VkFilter minFilter, VkSamplerAddressMode samplerAddressMode, bool addSamplerToList, VulkanBackend* backend)
{
	CreateImage(backend->logicalDevice, backend->physicalDevice, imageType, width, height, depth, mipLevels, arrayLayers, sampleCount, imageFormat, imageTiling, usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, image, memory);
	size = uint3(width, height, depth);
	aspect = imageAspectFlags;
	filename = NULL;
	theoreticalLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	layout.resize(mipLevels);
	for (int i = 0; i < mipLevels; i++)
		layout[i] = VK_IMAGE_LAYOUT_UNDEFINED;

	layerCount = arrayLayers;
	format = imageFormat;
	this->mipLevels = mipLevels;

	view = NULL;
	// If the image is only used for transferring, there's no need to create an image view
	if (usage & ~(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT))
		CreateImageView(imageViewType, VK_FLAGS_NONE);

	sampler = NULL;
	uniqueSampler = false;
	// No need to create a sampler if it won't be sampled
	if (usage & VK_IMAGE_USAGE_SAMPLED_BIT)
	{
		uniqueSampler = !addSamplerToList;

		SamplerSettings s{ magFilter, minFilter, samplerAddressMode, mipLevels };
		GetTextureSampler(s, addSamplerToList);
	}
}

Texture::~Texture()
{
	if (filename)
		delete filename;

	VkDevice device = GetEngine()->backend->logicalDevice;

	vkDestroyImage(device, image, nullptr);
	if (view)
		vkDestroyImageView(device, view, nullptr);

	vkFreeMemory(device, memory, nullptr);

	if (uniqueSampler)
		vkDestroySampler(device, sampler, nullptr);
}

void Texture::CopyFromBuffer(void* data, VkDeviceSize sz) const
{
	VulkanMemory* stagingBuffer = new VulkanMemory(sz, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, "Texture::CopyFromBuffer", false, NULL);

	void* ptr = stagingBuffer->Map();
	memcpy(ptr, data, static_cast<size_t>(sz));
	stagingBuffer->UnMap();

	CopyFromBuffer(stagingBuffer);

	delete stagingBuffer;
}

static void RecordBufferForCopyingToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, uint32_t depth, uint32_t layerCount)
{
	VulkanBackend* backend = GetEngine()->backend;
	VkCommandBuffer commandBuffer = backend->beginSingleTimeCommands();

	VkBufferImageCopy region{};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;

	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = layerCount;

	region.imageOffset = { 0, 0, 0 };
	region.imageExtent = {
		width,
		height,
		depth
	};

	vkCmdCopyBufferToImage(
		commandBuffer,
		buffer,
		image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1,
		&region
	);

	backend->endSingleTimeCommands(commandBuffer);
}

void Texture::CopyFromBuffer(VulkanMemory* buffer) const
{
	RecordBufferForCopyingToImage(*buffer, this->image, size.x, size.y, size.z, layerCount);
}

std::vector<float4> Texture::CopyToBuffer()
{
	std::vector<float4> fullData = {};

	VkImageLayout oldLayout = layout[0];
	TransitionImageLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0);

	fullData.resize((size_t)size.x * size.y);

	VulkanMemory* imageBuffer = new VulkanMemory(sizeof(float4) * size.x, VK_BUFFER_USAGE_TRANSFER_DST_BIT, "Copying image to buffer", false, NULL);

	VulkanBackend* backend = GetEngine()->backend;

	for (uint32_t y = 0; y < size.y; y++)
	{
		VkBufferImageCopy region{};
		region.bufferImageHeight = 0;
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.imageOffset.x = 0;
		region.imageOffset.y = y;
		region.imageOffset.z = 0;
		region.imageExtent.width = size.x;
		region.imageExtent.height = 1;
		region.imageExtent.depth = 1;
		region.imageSubresource.aspectMask = aspect;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		region.imageSubresource.mipLevel = 0;

		
		auto commandBuffer = backend->beginSingleTimeCommands();
		vkCmdCopyImageToBuffer(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, imageBuffer->buffer, 1, &region);
		backend->endSingleTimeCommands(commandBuffer);
		vkDeviceWaitIdle(backend->logicalDevice);

		void* data = imageBuffer->Map();
		memcpy(&fullData[y * size.x], data, sizeof(float4) * size.x);
		imageBuffer->UnMap();
	}
	delete imageBuffer;

	TransitionImageLayout(oldLayout, 0);

	return fullData;
}

void TextureWriteFunc(std::ofstream* context, void* data, int size)
{
	context->write((char*)data, size);
}

VkSampler Texture::CreateTextureSampler(SamplerSettings& settings)
{
	VulkanBackend* backend = GetEngine()->backend;

	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = settings.magFilter;
	samplerInfo.minFilter = settings.minFilter;

	samplerInfo.addressModeU = samplerInfo.addressModeV = samplerInfo.addressModeW = settings.addressMode;

	samplerInfo.anisotropyEnable = VK_TRUE;

	VkPhysicalDeviceProperties properties{};
	vkGetPhysicalDeviceProperties(backend->physicalDevice, &properties);

	samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;

	// The borderColor field specifies which color is returned when sampling beyond the image with clamp to border addressing mode. It is possible to return black, white or transparent in either float or int formats. You cannot specify an arbitrary color.
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;

	// The unnormalizedCoordinates field specifies which coordinate system you want to use to address texels in an image. If this field is VK_TRUE, then you can simply use coordinates within the 0-texWidth and 0-texHeight range. If it is VK_FALSE, then the texels are addressed using the 0-1 range on all axes. Real-world applications almost always use normalized coordinates, because then it's possible to use textures of varying resolutions with the exact same coordinates.
	samplerInfo.unnormalizedCoordinates = VK_FALSE;

	// If a comparison function is enabled, then texels will first be compared to a value, and the result of that comparison is used in filtering operations. This is mainly used for percentage-closer filtering on shadow maps. We'll look at this in a future chapter.
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.minLod = 0.f;
	samplerInfo.maxLod = static_cast<float>(settings.mipLevels);

	VkSampler outSampler;
	if (vkCreateSampler(backend->logicalDevice, &samplerInfo, nullptr, &outSampler) != VK_SUCCESS)
		throw std::runtime_error("failed to create texture sampler!");

	return outSampler;
}

std::vector<FullSampler> allSamplers = {};

void Texture::GetTextureSampler(SamplerSettings& settings, bool addToList)
{
	if (addToList)
	{
		for (size_t i = 0; i < allSamplers.size(); i++)
		{
			if (allSamplers[i].settings == settings)
			{
				sampler = allSamplers[i].sampler;
				return;
			}
		}

		allSamplers.push_back({ CreateTextureSampler(settings), settings });
		sampler = allSamplers.back().sampler;
	}
	else
		sampler = CreateTextureSampler(settings);
}

VkDescriptorImageInfo Texture::GetImageInfo() const
{
	VkDescriptorImageInfo info;

	info.imageLayout = layout[0];
	info.imageView = view;
	info.sampler = sampler;

	return info;
}

VkDescriptorImageInfo Texture::GetImageInfo(VkImageLayout layoutOverride) const
{
	VkDescriptorImageInfo info;

	info.imageLayout = layoutOverride;
	info.imageView = view;
	info.sampler = sampler;

	return info;
}

void Texture::CreateImageView(VkImageViewType viewType, VkImageViewCreateFlags flags)
{
	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = image;
	viewInfo.viewType = viewType;
	viewInfo.format = format;
	viewInfo.flags = flags;
	viewInfo.subresourceRange.aspectMask = aspect;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = mipLevels;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = layerCount;

	if (vkCreateImageView(GetEngine()->backend->logicalDevice, &viewInfo, nullptr, &view) != VK_SUCCESS)
		throw std::runtime_error("failed to create texture image view!");
}

static void LayoutToAccessMaskAndSourceStage(VkImageLayout layout, VkAccessFlags* outAccessMask, VkPipelineStageFlags* outStage)
{
	switch (layout)
	{
	case VK_IMAGE_LAYOUT_UNDEFINED:
		*outAccessMask = 0;
		*outStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		break;
	case VK_IMAGE_LAYOUT_GENERAL:
		*outAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		*outStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		break;
	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		*outAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		*outStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		break;
	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
		*outAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		*outStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		break;
	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
		*outAccessMask = VK_ACCESS_SHADER_READ_BIT;
		*outStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		break;
	case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL:
	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
		*outAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		*outStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		break;
	case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL:
		*outAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
		*outStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		break;
	case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
		*outAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		*outStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		break;
	default:
		throw std::invalid_argument("Add a new layout to LayoutToAccessMaskAndSourceStage()!");
	}
}

void Texture::TransitionImageLayout(VkCommandBuffer commandBuffer, VkImageLayout newLayout, int mip, int layer)
{
	VkImageLayout oldLayout = layout[mip > 0 ? mip : 0];

	if (newLayout == oldLayout)
		return;

	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;

	// The first two fields specify layout transition. It is possible to use VK_IMAGE_LAYOUT_UNDEFINED as oldLayout if you don't care about the existing contents of the image.
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;

	// If you are using the barrier to transfer queue family ownership, then these two fields should be the indices of the queue families. They must be set to VK_QUEUE_FAMILY_IGNORED if you don't want to do this (not the default value!).
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

	// The image and subresourceRange specify the image that is affected and the specific part of the image. Our image is not an array and does not have mipmapping levels, so only one level and layer are specified.
	barrier.image = image;
	barrier.subresourceRange.aspectMask = aspect;
	barrier.subresourceRange.baseMipLevel = mip > 0 ? mip : 0;
	barrier.subresourceRange.levelCount = mip >= 0 ? 1 : mipLevels;
	barrier.subresourceRange.baseArrayLayer = layer > 0 ? layer : 0;
	barrier.subresourceRange.layerCount = layer >= 0 ? 1 : layerCount;

	VkPipelineStageFlags sourceStage;
	VkPipelineStageFlags destinationStage;

	LayoutToAccessMaskAndSourceStage(oldLayout, &barrier.srcAccessMask, &sourceStage);
	LayoutToAccessMaskAndSourceStage(newLayout, &barrier.dstAccessMask, &destinationStage);

	vkCmdPipelineBarrier(
		commandBuffer,
		sourceStage, destinationStage,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier
	);

	if (mip >= 0)
		layout[mip] = newLayout;
	else
	{
		for (uint32_t i = 0; i < mipLevels; i++)
		{
			layout[i] = newLayout;
		}
	}
}

void Texture::TransitionImageLayout(VkImageLayout newLayout, int mip, int layer)
{
	VulkanBackend* backend = GetEngine()->backend;

	VkCommandBuffer commandBuffer = backend->beginSingleTimeCommands();
	TransitionImageLayout(commandBuffer, newLayout, mip, layer);
	backend->endSingleTimeCommands(commandBuffer);
}

void Texture::BlitTo(VkCommandBuffer commandBuffer, Texture* dst, VkFilter filter, Rect* srcArea, uint32_t srcMip, uint32_t srcLayer, Rect* dstArea, uint32_t dstMip, uint32_t dstLayer, VkImageLayout srcFinalLayout, VkImageLayout dstFinalLayout, VkImageLayout srcStartLayout, VkImageLayout dstStartLayout)
{
	VkImageLayout actualLayoutSrc = layout[srcMip];
	VkImageLayout actualLayoutDst = dst->layout[dstMip];

	if (srcStartLayout != VK_IMAGE_LAYOUT_UNDEFINED)
		layout[srcMip] = srcStartLayout;

	if (dstStartLayout != VK_IMAGE_LAYOUT_UNDEFINED)
		dst->layout[dstMip] = dstStartLayout;

	VkImageLayout finalLayoutDst = dstFinalLayout != VK_IMAGE_LAYOUT_UNDEFINED ? dstFinalLayout : dst->layout[dstMip];
	VkImageLayout finalLayoutSrc = srcFinalLayout != VK_IMAGE_LAYOUT_UNDEFINED ? srcFinalLayout : layout[srcMip];

	if (finalLayoutDst == VK_IMAGE_LAYOUT_UNDEFINED)
		finalLayoutDst = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	if (finalLayoutSrc == VK_IMAGE_LAYOUT_UNDEFINED)
		finalLayoutSrc = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	TransitionImageLayout(commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, srcMip, srcLayer);
	dst->TransitionImageLayout(commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, dstMip, dstLayer);

	VkImageBlit blit{};
	blit.srcSubresource.mipLevel = srcMip;
	blit.srcSubresource.aspectMask = aspect;
	blit.srcSubresource.baseArrayLayer = 0;
	blit.srcSubresource.layerCount = 1;

	blit.srcOffsets[0].z = 0;
	blit.srcOffsets[1].z = 1;

	if (srcArea)
	{
		blit.srcOffsets[0].x = srcArea->x;
		blit.srcOffsets[0].y = srcArea->y;
		blit.srcOffsets[1].x = srcArea->x + srcArea->width;
		blit.srcOffsets[1].y = srcArea->y + srcArea->height;
	}
	else
	{
		blit.srcOffsets[0].x = 0;
		blit.srcOffsets[0].y = 0;
		blit.srcOffsets[1].x = size.x;
		blit.srcOffsets[1].y = size.y;
	}

	blit.dstSubresource.mipLevel = dstMip;
	blit.dstSubresource.aspectMask = dst->aspect;
	blit.dstSubresource.layerCount = 1;
	blit.dstSubresource.baseArrayLayer = 0;

	blit.dstOffsets[0].z = 0;
	blit.dstOffsets[1].z = 1;
	if (dstArea)
	{
		blit.dstOffsets[0].x = dstArea->x;
		blit.dstOffsets[0].y = dstArea->y;
		blit.dstOffsets[1].x = dstArea->x + dstArea->width;
		blit.dstOffsets[1].y = dstArea->y + dstArea->height;
	}
	else
	{
		blit.dstOffsets[0].x = 0;
		blit.dstOffsets[0].y = 0;
		blit.dstOffsets[1].x = dst->size.x;
		blit.dstOffsets[1].y = dst->size.y;
	}

	vkCmdBlitImage(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, filter);

	dst->TransitionImageLayout(commandBuffer, finalLayoutDst, dstMip, dstLayer);
	TransitionImageLayout(commandBuffer, finalLayoutSrc, srcMip, srcLayer);
}

void Texture::BlitTo(Texture* dst, VkFilter filter, Rect* srcArea, uint32_t srcMip, uint32_t srcLayer, Rect* dstArea, uint32_t dstMip, uint32_t dstLayer, VkImageLayout srcFinalLayout, VkImageLayout dstFinalLayout)
{
	VulkanBackend* backend = GetEngine()->backend;
	VkCommandBuffer commandBuffer = backend->beginSingleTimeCommands();
	BlitTo(commandBuffer, dst, filter, srcArea, srcMip, srcLayer, dstArea, dstMip, dstLayer, srcFinalLayout, dstFinalLayout);
	backend->endSingleTimeCommands(commandBuffer);
}

Texture*& Texture::AddTexture(Texture* tex)
{
	return allTextures[numTextures++] = tex;
}

void Texture::DestroyAllTextures()
{
	VkDevice device = GetEngine()->backend->logicalDevice;

	for (auto& sampler : allSamplers)
		vkDestroySampler(device, sampler.sampler, VK_NULL_HANDLE);

	allSamplers.clear();

	for (size_t i = 0; i < numTextures; i++)
	{
		if (allTextures[i])
			delete allTextures[i];
	}

	numTextures = 0;
}