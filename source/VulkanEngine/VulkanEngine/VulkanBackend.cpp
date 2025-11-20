// The comments explaining vulkan are not from me, they are taken from the vulkan-tutorial guide that I followed to get started.
// I'd recommend starting there to get an idea of what my engine is doing: https://vulkan-tutorial.com

#include "VulkanBackend.h"
#include "engine.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vulkan/vk_enum_string_helper.h>

#include <stb_image.h>

#include <iostream>
#include <set>
#include <limits> // Necessary for std::numeric_limits
#include <algorithm> // Necessary for std::clamp
#include <vector>

#include "engineUtils.h"
#include "BackendUtils.h"
#include "luaUtils.h"
#include "luafunctions.h"
#include "engineSettings.h"

#include "Thing.h"
#include "SpotLight.h"
#include "Shader.h"
#include "VulkanMemory.h"
#include "Texture.h"
#include "Mesh.h"
#include "DescriptorSet.h"


const std::vector<const char*> validationLayers = {
	"VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*> deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME,
#ifdef ENABLE_RAYTRACING
	"VK_KHR_acceleration_structure",
	"VK_KHR_ray_tracing_pipeline",
	"VK_KHR_ray_query"
#endif
};


#ifdef _DEBUG
const bool enableValidationLayers = true;
#else
const bool enableValidationLayers = false;
#endif


bool VulkanBackend::checkValidationLayerSupport()
{
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	for (const char* layerName : validationLayers)
	{
		bool layerFound = false;

		for (const auto& layerProperties : availableLayers)
		{
			if (!strcmp(layerName, layerProperties.layerName))
			{
				layerFound = true;
				break;
			}
		}

		if (!layerFound)
			return false;
	}

	return true;
}

VkApplicationInfo VulkanBackend::MakeAppInfo(const char* appName, uint32_t appVersion)
{
	// Create Instance
	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = appName;
	appInfo.applicationVersion = appVersion;
	appInfo.pEngineName = "Zack\'s Vulkan Engine";
	appInfo.engineVersion = ENGINE_VERSION;
	appInfo.apiVersion = VK_API_VERSION_1_2;
	appInfo.pNext = nullptr;
	return appInfo;
}

static bool HasAllQueueFamilies(QueueFamilyIndices q)
{
	return q.hasGraphicsFamily && q.hasPresentFamily;
}

QueueFamilyIndices VulkanBackend::findQueueFamilies(VkPhysicalDevice device)
{
	QueueFamilyIndices q;
	q.hasGraphicsFamily = false;
	q.hasPresentFamily = false;

	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

	VkBool32 presentSupport = false;
	for (uint32_t i = 0; i < queueFamilyCount; i++)
	{
		if (!q.hasPresentFamily)
		{
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
			if (presentSupport)
			{
				q.presentFamily = i;
				q.hasPresentFamily = true;
			}
		}

		if ((queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT))
		{
			if (queueFamilies[i].timestampValidBits == 0)
				std::cout << "Graphics Family does not support time stamping!\n";
			else
			{
				q.graphicsFamily = i;
				q.hasGraphicsFamily = true;
			}
		}

		if (HasAllQueueFamilies(q))
			break;
	}

	return q;
}

static bool checkDeviceExtensionSupport(VkPhysicalDevice device)
{
	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

	std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

	for (const auto& extension : availableExtensions)
		requiredExtensions.erase(extension.extensionName);

	return requiredExtensions.empty();
}

SwapChainSupportDetails VulkanBackend::querySwapChainSupport(VkPhysicalDevice device)
{
	SwapChainSupportDetails details;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
	if (formatCount)
	{
		details.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
	}

	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

	if (presentModeCount)
	{
		details.presentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
	}

	return details;
}

struct requiredGPUFeature
{
	size_t offset;
	const char* description;
};

#define featureOffset(m) offsetof(VkPhysicalDeviceFeatures, m)

std::vector<requiredGPUFeature> requiredGPUFeatures = {
	{ featureOffset(samplerAnisotropy), "anisotropic filtering" },
	{ featureOffset(independentBlend), "multiple render targets" },
	{ featureOffset(fillModeNonSolid), "non-solid fill mode" },
	//{ featureOffset(sampleRateShading), "MSAA texture sampling" }
};

static void EnableRequiredFeatures(VkPhysicalDeviceFeatures* features)
{
	size_t ptr;
	for (const auto& feature : requiredGPUFeatures)
	{
		// I don't know how or why, but I have to do this in the most round-about way possible or it won't get the right address
		ptr = (size_t)features;
		ptr += feature.offset;

		*(VkBool32*)ptr = VK_TRUE;
	}
}

static bool HasRequiredFeatures(VkPhysicalDeviceFeatures* features)
{
	size_t ptr;
	for (const auto& feature : requiredGPUFeatures)
	{
		// I don't know how or why, but I have to do this in the most round-about way possible or it won't get the right address
		ptr = (size_t)features;
		ptr += feature.offset;

		if (!(*(VkBool32*)ptr))
		{
			std::cout << "This device is unsuitable: does not support " << feature.description << "\n";
			return false;
		}
	}

	return true;
}

#define CHECK_FEATURE(feature, description) if (!feature) { std::cout << "This device is unsuitable: " << description << "\n"; return 0; }

uint32_t VulkanBackend::RankDevice(VkPhysicalDevice device)
{
	uint32_t rank = 0;

	VkPhysicalDeviceProperties properties;
	vkGetPhysicalDeviceProperties(device, &properties);

	VkPhysicalDeviceFeatures features;
	vkGetPhysicalDeviceFeatures(device, &features);

	std::cout << "Discrete: " << ((properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) ? "True" : "False") << "\nGeometryShader: " << (features.geometryShader ? "True" : "False") << "\n";

	QueueFamilyIndices q = findQueueFamilies(device);

	bool swapChainAdequate = false;

	CHECK_FEATURE(checkDeviceExtensionSupport(device), "does not support extensions");

	SwapChainSupportDetails swapChainDetails = querySwapChainSupport(device);
	swapChainAdequate = !swapChainDetails.formats.empty() && !swapChainDetails.presentModes.empty();

	CHECK_FEATURE(HasAllQueueFamilies(q), "cannot render and present images");

	CHECK_FEATURE(swapChainAdequate, "swap chain is not adequate");

	if (!HasRequiredFeatures(&features))
		return 0;

	// The more supported features, the higher the rank
	rank += features.geometryShader;
	rank += features.wideLines;
	rank += features.tessellationShader;
	rank += features.depthClamp;

	rank += properties.apiVersion;
	rank += properties.limits.maxFramebufferHeight;
	rank += properties.limits.maxFramebufferWidth;
	rank += properties.limits.maxGeometryInputComponents;
	rank += properties.limits.maxFragmentOutputAttachments;

	return rank;
}

void VulkanBackend::PickGPU()
{
	// For some reason you have to call it twice, first to get the number of devices, then to actually get the devices
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

	check(deviceCount, "No Vulkan support on this machine!");

	std::cout << deviceCount << " devices\n";

	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

	uint32_t bestRank = 0;
	uint32_t rank;

	for (const auto& device : devices)
	{
		rank = RankDevice(device);
		if (rank > bestRank)
		{
			bestRank = rank;
			physicalDevice = device;
		}
	}

	if (physicalDevice == nullptr)
		throw std::runtime_error("Vulkan support found, but none of the GPUs are suitable!");

	VkPhysicalDeviceProperties properties;
	vkGetPhysicalDeviceProperties(physicalDevice, &properties);
	timestampPeriod = properties.limits.timestampPeriod;
}

static VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
{
	for (const auto& fmt : availableFormats)
	{
		if (fmt.format == VK_FORMAT_B8G8R8A8_SRGB && fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			return fmt;
	}

	return availableFormats[0];
}

std::vector<VkPresentModeKHR> desiredPresentModes = {
	//VK_PRESENT_MODE_IMMEDIATE_KHR,
	VK_PRESENT_MODE_MAILBOX_KHR
};

static VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availableModes)
{
	uint32_t bestIndex = 99999;
	VkPresentModeKHR bestMode = VK_PRESENT_MODE_FIFO_KHR;

	for (const auto& mode : availableModes)
	{
		for (uint32_t i = 0; i < desiredPresentModes.size(); i++)
		{
			if (mode == desiredPresentModes[i])
			{
				if (i < bestIndex)
				{
					bestMode = mode;
					bestIndex = i;
				}
			}
		}
	}

	std::cout << "Present Mode: " << string_VkPresentModeKHR(bestMode) << "\n";
	return bestMode;
}

VkExtent2D VulkanBackend::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
{
	if (capabilities.currentExtent.width != (uint32_t)-1)
		return capabilities.currentExtent;

	int width, height;
	glfwGetFramebufferSize(glWindow, &width, &height);

	while (width == 0)
		glfwWaitEvents();

	VkExtent2D actualExtent = {
		static_cast<uint32_t>(width),
		static_cast<uint32_t>(height),
	};

	actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
	actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

	return actualExtent;
}

void VulkanBackend::createSwapChain()
{
	SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

	VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
	VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
	VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

	swapChainImageFormat = surfaceFormat.format;
	swapChainExtent = extent;

	uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;

	if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
		imageCount = swapChainSupport.capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR swapCreateInfo{};
	swapCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapCreateInfo.surface = surface;
	swapCreateInfo.minImageCount = imageCount;
	swapCreateInfo.imageFormat = surfaceFormat.format;
	swapCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
	swapCreateInfo.imageExtent = extent;
	swapCreateInfo.imageArrayLayers = 1;
	swapCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

	QueueFamilyIndices q = findQueueFamilies(physicalDevice);
	uint32_t queueFamilyIndices[] = { q.graphicsFamily, q.presentFamily };

	if (q.graphicsFamily != q.presentFamily) {
		swapCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		swapCreateInfo.queueFamilyIndexCount = 2;
		swapCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else {
		swapCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		swapCreateInfo.queueFamilyIndexCount = 0; // Optional
		swapCreateInfo.pQueueFamilyIndices = nullptr; // Optional
	}

	swapCreateInfo.preTransform = swapChainSupport.capabilities.currentTransform;
	swapCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapCreateInfo.presentMode = presentMode;
	swapCreateInfo.clipped = VK_TRUE;
	swapCreateInfo.oldSwapchain = VK_NULL_HANDLE;

	if (vkCreateSwapchainKHR(logicalDevice, &swapCreateInfo, nullptr, &swapChain) != VK_SUCCESS)
		throw std::runtime_error("failed to create swap chain!");

	vkGetSwapchainImagesKHR(logicalDevice, swapChain, &imageCount, nullptr);
	MAX_FRAMES_IN_FLIGHT = imageCount;
	swapChainImages.resize(imageCount);
	vkGetSwapchainImagesKHR(logicalDevice, swapChain, &imageCount, swapChainImages.data());

	perspectiveMatrix = glm::perspective(glm::radians(45.0f), swapChainExtent.width / (float)swapChainExtent.height, 0.1f, 10000.0f);
	perspectiveMatrix[1][1] *= -1;

	createImageViews();
}

VkFormat VulkanBackend::findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
{
	for (VkFormat format : candidates) {
		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);
		if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features)) {
			return format;
		}
		else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features)) {
			return format;
		}
	}
	throw std::runtime_error("failed to find supported format!");
}

VkFormat VulkanBackend::findDepthFormat()
{
	return findSupportedFormat(
		{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D16_UNORM },
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
	);
}

VkFormat VulkanBackend::findDepthStencilFormat()
{
	return findSupportedFormat(
		{ VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM_S8_UINT },
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
	);
}

void VulkanBackend::createLightRenderPass()
{
	VkAttachmentDescription frameAttachment{};

	frameAttachment.format = findDepthFormat();
	frameAttachment.samples = msaaSamples;

	frameAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	frameAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

	frameAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	frameAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

	frameAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	frameAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkAttachmentReference depthAttachmentRef{};
	depthAttachmentRef.attachment = 0;
	depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	// Vulkan may also support compute subpasses in the future, so we have to be explicit about this being a graphics subpass.
	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

	// Next, we specify the reference to the color attachment:
	// The index of the attachment in this array is directly referenced from the fragment shader with the
	// layout(location = 0) out vec4 outColor
	// directive!

	// The following other types of attachments can be referenced by a subpass :

	// pInputAttachments: Attachments that are read from a shader
	// pResolveAttachments : Attachments used for multisampling color attachments
	// pDepthStencilAttachment : Attachment for depth and stencil data
	// pPreserveAttachments : Attachments that are not used by this subpass, but for which the data must be preserved
	subpass.colorAttachmentCount = 0;
	subpass.pColorAttachments = VK_NULL_HANDLE;
	subpass.pDepthStencilAttachment = &depthAttachmentRef;

	//subpass.pResolveAttachments = &colorAttachmentResolveRef;

	// The first two fields specify the indices of the dependency and the dependent subpass.
	// The special value VK_SUBPASS_EXTERNAL refers to the implicit subpass before or after the render pass depending on whether it is specified in srcSubpass or dstSubpass.
	// The index 0 refers to our subpass, which is the first and only one.
	// The dstSubpass must always be higher than srcSubpass to prevent cycles in the dependency graph (unless one of the subpasses is VK_SUBPASS_EXTERNAL).
	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;

	// The next two fields specify the operations to wait on and the stages in which these operations occur.
	// We need to wait for the swap chain to finish reading from the image before we can access it.
	// This can be accomplished by waiting on the color attachment output stage itself.
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	//dependency.srcAccessMask = 0;

	// The operations that should wait on this are in the color attachment stage and involve the writing of the color attachment.
	// These settings will prevent the transition from happening until it's actually necessary (and allowed): when we want to start writing colors to it.
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &frameAttachment;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	if (vkCreateRenderPass(logicalDevice, &renderPassInfo, nullptr, &Light::renderPass) != VK_SUCCESS)
		throw std::runtime_error("failed to create light render pass!");
}

void VulkanBackend::DestroyMainFrameBuffer()
{
	/*
	vkDestroySampler(logicalDevice, mainRenderTarget_C->sampler, VK_NULL_HANDLE);
	vkDestroySampler(logicalDevice, mainRenderTarget_D->sampler, VK_NULL_HANDLE);
	vkDestroySampler(logicalDevice, mainRenderTarget_N->sampler, VK_NULL_HANDLE);
	vkDestroySampler(logicalDevice, mainRenderTarget_P->sampler, VK_NULL_HANDLE);
	vkDestroySampler(logicalDevice, mainRenderTarget_G->sampler, VK_NULL_HANDLE);
	*/

	delete mainRenderTarget_C;
	delete mainRenderTarget_D;
	delete mainRenderTarget_N;
	delete mainRenderTarget_P;
	delete mainRenderTarget_G;

	vkDestroyFramebuffer(logicalDevice, mainFrameBuffer, VK_NULL_HANDLE);
	vkDestroyFramebuffer(logicalDevice, depthPrepassFrameBuffer, VK_NULL_HANDLE);
}

void VulkanBackend::CreateMainFrameBuffer(float resolutionScale)
{
	renderExtent.width = (uint32_t)(swapChainExtent.width * resolutionScale);
	renderExtent.height = (uint32_t)(swapChainExtent.height * resolutionScale);
	renderViewport = { 0, 0, (float)renderExtent.width, (float)renderExtent.height, 0.0f, 1.0f };

	VkFormat depthFmt = findDepthStencilFormat();

	// Writing mainRenderTarget_C = Texture(...) corrupts the members, even though this is what that line should be doing, right?
	mainRenderTarget_C = new Texture(VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_2D, renderFormat, renderExtent.width, renderExtent.height, 1, 1, 1, msaaSamples, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, false, this);
	mainRenderTarget_G = new Texture(VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_2D, GIFormat, renderExtent.width, renderExtent.height, 1, 1, 1, msaaSamples, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, false, this);
	mainRenderTarget_N = new Texture(VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_2D, normalFormat, renderExtent.width, renderExtent.height, 1, 1, 1, msaaSamples, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, false, this);
	mainRenderTarget_P = new Texture(VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_2D, positionFormat, renderExtent.width, renderExtent.height, 1, 1, 1, msaaSamples, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_IMAGE_ASPECT_COLOR_BIT, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, false, this);
	mainRenderTarget_D = new Texture(VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_2D, depthFmt, renderExtent.width, renderExtent.height, 1, 1, 1, msaaSamples, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, false, this);

	if (mainRenderPass)
	{
		vkDestroyRenderPass(logicalDevice, mainRenderPass, VK_NULL_HANDLE);
		mainRenderPass = NULL;
	}
		

	if (depthPrepassRenderPass)
	{
		vkDestroyRenderPass(logicalDevice, depthPrepassRenderPass, VK_NULL_HANDLE);
		depthPrepassRenderPass = NULL;
	}
		

	std::array<VkAttachmentDescription, 5> attachments;
	VkFormat attachmentFormats[5] = {
		renderFormat,
		depthFmt,
		normalFormat,
		positionFormat,
		GIFormat
	};

	for (uint32_t i = 0; i < attachments.size(); i++)
	{
		attachments[i].format = attachmentFormats[i];
		attachments[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[i].samples = msaaSamples;
		attachments[i].flags = VK_FLAGS_NONE;
		if (i != 1)
		{
			attachments[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attachments[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attachments[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachments[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachments[i].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}
	}

	attachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	attachments[1].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;

	std::array<VkAttachmentReference, 4> colourRefs;
	VkAttachmentReference depthRef;

	for (uint32_t i = 0; i < 4; i++)
	{
		colourRefs[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		colourRefs[i].attachment = i ? (i + 1) : 0;
	}

	depthRef.attachment = 1;
	depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependency.dependencyFlags = 0;

	VkSubpassDescription subPass{};
	subPass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subPass.colorAttachmentCount = static_cast<uint32_t>(colourRefs.size());
	subPass.pColorAttachments = colourRefs.data();
	subPass.pDepthStencilAttachment = &depthRef;
	subPass.pResolveAttachments = VK_NULL_HANDLE;
	subPass.preserveAttachmentCount = 0;
	subPass.pPreserveAttachments = VK_NULL_HANDLE;
	subPass.inputAttachmentCount = 0;

	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.pNext = VK_NULL_HANDLE;
	renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;
	renderPassInfo.pSubpasses = &subPass;
	renderPassInfo.subpassCount = 1;

	vkCreateRenderPass(logicalDevice, &renderPassInfo, VK_NULL_HANDLE, &mainRenderPass);

	attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	subPass.colorAttachmentCount = 0;
	depthRef.attachment = 0;
	renderPassInfo.pAttachments = &attachments[1];
	renderPassInfo.attachmentCount = 1;
	vkCreateRenderPass(logicalDevice, &renderPassInfo, VK_NULL_HANDLE, &depthPrepassRenderPass);

	std::array<VkImageView, 5> views = {
		mainRenderTarget_C->view,
		mainRenderTarget_D->view,
		mainRenderTarget_N->view,
		mainRenderTarget_P->view,
		mainRenderTarget_G->view
	};

	VkFramebufferCreateInfo frameBufferInfo{};
	frameBufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	frameBufferInfo.attachmentCount = static_cast<uint32_t>(views.size());
	frameBufferInfo.pAttachments = views.data();
	frameBufferInfo.renderPass = mainRenderPass;
	frameBufferInfo.width = renderExtent.width;
	frameBufferInfo.height = renderExtent.height;
	frameBufferInfo.layers = 1;
	vkCreateFramebuffer(logicalDevice, &frameBufferInfo, VK_NULL_HANDLE, &mainFrameBuffer);

	frameBufferInfo.attachmentCount = 1;
	frameBufferInfo.pAttachments = &views[1];
	frameBufferInfo.renderPass = depthPrepassRenderPass;
	vkCreateFramebuffer(logicalDevice, &frameBufferInfo, VK_NULL_HANDLE, &depthPrepassFrameBuffer);

	mainRenderTarget_C->theoreticalLayout = mainRenderTarget_D->theoreticalLayout = mainRenderTarget_G->theoreticalLayout = mainRenderTarget_N->theoreticalLayout = mainRenderTarget_P->theoreticalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

static char* ReadLayout(char* string, uint32_t* outSet, uint32_t* outBinding)
{
	char* endPtr;

	*outSet = 0;
	*outBinding = 0;

	// Did I mention how much I hate string manipulation in C?

	while (*string++ != '(');

	while (*string != ')')
	{
		if (*string == 's')
		{
			while (*string++ != '=');

			while (isspace(*string))
				string++;

			*outSet = strtol(string, &endPtr, 10);
			string = endPtr;

			while (*string == ',' || isspace(*string))
				string++;
		}
		else if (*string == 'b')
		{
			while (*string++ != '=');

			while (isspace(*string))
				string++;

			*outBinding = strtol(string, &endPtr, 10);
			string = endPtr;

			while (*string == ',' || isspace(*string))
				string++;
		}
		else
			string++;
	}

	return string;
}



// if outVShader is NULL, then it's the same as the zlsl. It will need to be freed afterwards if valid.
std::vector<std::array<uint32_t, 4>> VulkanBackend::GetInfoFromZLSL(const char* zlsl, uint32_t* outAttachments, bool vertexShaderOnly)
{
	std::vector<std::array<uint32_t, 4>> setLayoutNums = { { 0, 0, 0, 0 } };

	constexpr uint32_t SAMPLERS = 0;
	constexpr uint32_t VBUFFERS = 1;
	constexpr uint32_t PBUFFERS = 2;
	constexpr uint32_t STORAGE = 3;

	*outAttachments = 0;

	auto buffer = readFile(zlsl);

	bool vertexShader = true;
	char* ptr = buffer.data();
	size_t i = 0;
	int depth = 0;
	size_t shaderLen = 0;
	size_t prevIndex = 0;
	uint32_t set, binding;
	char* tempPtr;

	while (i < buffer.size())
	{
		if (buffer[i] == '[' || buffer[i] == '{' || buffer[i] == '(') depth++;
		if (buffer[i] == ']' || buffer[i] == '}' || buffer[i] == ')') depth--;

		if (!i || ((buffer[i] == ';' || buffer[i] == '\n') && !depth))
		{
			if (buffer[i] == ';') i++;
			while (isspace(buffer[i])) i++;

			switch (buffer[i])
			{
			case 's':
				if (StringCompare(&buffer[i], "sampler2D") || StringCompare(&buffer[i], "samplerCUBE"))
					setLayoutNums[0][SAMPLERS]++;
				break;
			case 'u':
				if (StringCompare(&buffer[i], "uniform"))
				{
					if (vertexShader)
						setLayoutNums[0][VBUFFERS]++;
					else
						setLayoutNums[0][PBUFFERS]++;
				}
				break;
			case 'V':
				if (StringCompare(&buffer[i], "VertexShader"))
				{
					vertexShader = false;
					if (vertexShaderOnly) i = buffer.size();
				}

				break;

			case 'l':
				if (StringCompare(&buffer[i], "layout("))
				{
					tempPtr = ReadLayout(&buffer[i], &set, &binding);
					i += (tempPtr - &buffer[i]);

					while (setLayoutNums.size() <= set)
						setLayoutNums.push_back({ 0, 0, 0, 0 });

					i++;

					while (isspace(buffer[i])) i++;

					if (StringCompare(&buffer[i], "out"))
						(*outAttachments)++;
					else if (StringCompare(&buffer[i], "readonly buffer"))
						setLayoutNums[set][STORAGE]++;
					else if (StringCompare(&buffer[i], "uniform"))
					{
						i += 8;

						while (isspace(buffer[i])) i++;

						if (StringCompare(&buffer[i], "sampler2D") || StringCompare(&buffer[i], "samplerCUBE"))
							setLayoutNums[set][SAMPLERS]++;
						else
						{
							if (vertexShader)
								setLayoutNums[set][VBUFFERS]++;
							else
								setLayoutNums[set][PBUFFERS]++;
						}
					}
				}

				break;

			case '#':
				if (StringCompare(&buffer[i], "#VertexShader"))
				{
					i += 14;
					while (isspace(buffer[i])) i++;

					i++;
					prevIndex = i;
					while (!isspace(buffer[i]))
					{
						shaderLen++;
						i++;
					}

					shaderLen--;

					auto tempBuffer = (char*)malloc(shaderLen + 1);
					check(tempBuffer, "Failed to allocate in GetInfoFromZLSL()!");
					StringCopy(tempBuffer, &buffer[prevIndex], shaderLen);
					tempBuffer[shaderLen] = NULL;

					char vshaderBuffer[256];
					StringCopySafe(vshaderBuffer, 256, "shaders/");
					StringConcatSafe(vshaderBuffer, 256, tempBuffer);

					setLayoutNums = GetInfoFromZLSL(vshaderBuffer, outAttachments, true);

					free(tempBuffer);
					vertexShader = false;
				}
				break;

			default:
				break;
			}
		}
		i++;
	}

	return setLayoutNums;
}

void VulkanBackend::createDescriptorSetLayout(size_t vBuffers, size_t pBuffers, size_t numSamplers, size_t numStorageBuffers, size_t numStorageImages, VkDescriptorSetLayout* outLayout)
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

	if (vkCreateDescriptorSetLayout(logicalDevice, &layoutInfo, nullptr, outLayout) != VK_SUCCESS)
		throw std::runtime_error("failed to create descriptor set layout!");

	free(bindings);
}

void VulkanBackend::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels, VkImageViewType viewType, int flags, VkImageView* outImageView)
{
	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = image;
	viewInfo.viewType = viewType;
	viewInfo.format = format;
	viewInfo.flags = flags;
	viewInfo.subresourceRange.aspectMask = aspectFlags;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = mipLevels;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = (viewType == VK_IMAGE_VIEW_TYPE_CUBE) ? 6 : 1;

	if (vkCreateImageView(logicalDevice, &viewInfo, nullptr, outImageView) != VK_SUCCESS)
		throw std::runtime_error("failed to create texture image view!");
}

struct ComputeDescriptorSetLayout
{
	VkDescriptorSetLayout layout;
	size_t buffers, textures;
};
std::vector<VkDescriptorSetLayout> computeDescriptorSetLayouts;

VkDescriptorSetLayout* VulkanBackend::GetComputeDescriptorSet(size_t numUniformBuffers, size_t numStorageBuffers, size_t numStorageTextures, size_t numSamplers)
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

	vkCreateDescriptorSetLayout(logicalDevice, &layoutInfo, VK_NULL_HANDLE, &computeDescriptorSetLayouts.back());

	return &computeDescriptorSetLayouts.back();
}


VkFormat VulkanBackend::GetStorageImageFormat(VkImageType type, VkImageTiling tiling)
{
	std::vector<VkFormat> supportedFormats;
	VkImageFormatProperties properties;
	for (VkFormat i = (VkFormat)1; i < (VkFormat)185; i = (VkFormat)((uint32_t)i + 1))
	{
		if (vkGetPhysicalDeviceImageFormatProperties(physicalDevice, i, type, tiling, VK_IMAGE_USAGE_STORAGE_BIT, 0, &properties) != VK_ERROR_FORMAT_NOT_SUPPORTED)
			supportedFormats.push_back(i);
	}

	std::cout << "\nSupported Storage Formats:\n";
	for (auto format : supportedFormats)
		std::cout << string_VkFormat(format) << "\n";
	std::cout << "\n";

	return supportedFormats[0];
}

struct ComputeShaderConfig
{
	float4x4 viewProj;
	float4x4 view;
	float4 camDir;
	float4 camPos;
	uint32_t numIndices;
	uint32_t numVertices;
	uint32_t numObjects;
	uint32_t width;
	uint32_t height;
};

struct ComputeObject
{
	float4x4 matrix;
	uint32_t startingIndex;
	uint32_t numIndices;
	uint32_t startingVertex;
};

std::vector<ComputeObject> computeObjects;
VulkanMemory* computeObjectsMem;

/*
void VulkanBackend::SaveTextureToPNG(Texture* texture, VkImageLayout currentLayout, const char* filename)
{
	std::vector<float4> fullData = CopyImageToBuffer(texture, currentLayout);
	stbi_write_png(filename, texture->size.x, texture->size.y, 4, fullData.data(), texture->size.x * sizeof(float4));
}
*/

void VulkanBackend::SaveBeegShadowMapToPNG(const char* filename)
{
	beegShadowMap->SaveToPNG(filename);
}

void VulkanBackend::RunComputeShader()
{
	RTShader = new ComputeShader(this, STRING("shaders/testcompute.comp"), 1, 3, 1, 2);
	allComputeShaders.push_back(RTShader);

	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		RTShader->uniformBuffers[i] = new VulkanMemory(sizeof(ComputeShaderConfig), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, "ComputeShader", false, NULL);

	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		DescriptorSet::AllocateDescriptorSets(1, &RTShader->setLayout, &RTShader->descriptorSets[i]);

	VkDescriptorBufferInfo bufferInfos[4];
	VkDescriptorImageInfo imageInfos[3];

	imageInfos[0] = mainRenderTarget_P->GetImageInfo();
	imageInfos[1] = mainRenderTarget_N->GetImageInfo();
	imageInfos[2] = RTTexture->GetImageInfo();

	bufferInfos[0].offset = bufferInfos[1].offset = bufferInfos[2].offset = bufferInfos[3].offset = 0;

	bufferInfos[1].buffer = Mesh::allVertexBuffer->buffer;
	bufferInfos[2].buffer = Mesh::allIndexBuffer->buffer;
	bufferInfos[3].buffer = computeObjectsMem->buffer;

	bufferInfos[0].range = sizeof(ComputeShaderConfig);
	bufferInfos[1].range = bufferInfos[2].range = bufferInfos[3].range = VK_WHOLE_SIZE;

	VkWriteDescriptorSet writes[7];
	writes[0].descriptorCount = writes[1].descriptorCount = writes[2].descriptorCount = writes[3].descriptorCount = writes[4].descriptorCount = writes[5].descriptorCount = writes[6].descriptorCount = 1;
	writes[0].sType = writes[1].sType = writes[2].sType = writes[3].sType = writes[4].sType = writes[5].sType = writes[6].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstArrayElement = writes[1].dstArrayElement = writes[2].dstArrayElement = writes[3].dstArrayElement = writes[4].dstArrayElement = writes[5].dstArrayElement = writes[6].dstArrayElement = 0;
	writes[0].pNext = writes[1].pNext = writes[2].pNext = writes[3].pNext = writes[4].pNext = writes[5].pNext = writes[6].pNext = VK_NULL_HANDLE;

	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

	for (uint32_t i = 0; i < 7; i++)
		writes[i].dstBinding = i;

	writes[0].pBufferInfo = &bufferInfos[0];
	writes[1].pBufferInfo = &bufferInfos[1];
	writes[2].pBufferInfo = &bufferInfos[2];
	writes[3].pBufferInfo = &bufferInfos[3];

	writes[4].pImageInfo = &imageInfos[0];
	writes[5].pImageInfo = &imageInfos[1];
	writes[6].pImageInfo = &imageInfos[2];

	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		bufferInfos[0].buffer = RTShader->uniformBuffers[i]->buffer;
		writes[0].dstSet = writes[1].dstSet = writes[2].dstSet = writes[3].dstSet = writes[4].dstSet = writes[5].dstSet = writes[6].dstSet = RTShader->descriptorSets[i];
		vkUpdateDescriptorSets(logicalDevice, 7, writes, 0, VK_NULL_HANDLE);
	}
}

VkShaderModule VulkanBackend::createShaderModule(const std::vector<char>& code)
{
	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

	VkShaderModule shaderModule;
	if (vkCreateShaderModule(logicalDevice, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
		throw std::runtime_error("failed to create shader module!");

	return shaderModule;
}


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

/*
Texture* VulkanBackend::CreateTextureArray(Texture* textures, uint32_t numTextures, uint32_t width, uint32_t height, VkFormat format)
{
	if (numTextures >= MAX_TEXTURES)
	{
		std::cout << "Ran out of room for more textures!\n";
		return allTextures[0];
	}

	auto tex = new Texture(VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_2D, format, width, height, 1, 1, numTextures, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, true, this);
	allTextures[numTextures++] = tex;

	auto commandBuffer = beginSingleTimeCommands();
	Rect srcArea{};
	Rect dstArea = { 0, 0, width, height };
	for (uint32_t i = 0; i < numTextures; i++)
	{
		srcArea = { 0, 0, textures[i].size.x, textures[i].size.y };
		BlitImage(commandBuffer, &textures[i], srcArea, tex, dstArea, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_FILTER_LINEAR, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0, 0, i);
	}
	endSingleTimeCommands(commandBuffer);
	vkDeviceWaitIdle(logicalDevice);

	return tex;
}
*/


void VulkanBackend::CreateCubemap(const CHAR_T* filename, Texture*& outTexture)
{
	int numChannels;
	VkDeviceSize size;

	std::vector<stbi_uc*> pixels = {};
	pixels.resize(6);

	ZEROMEM(strBuffer, 256 * sizeof(CHAR_T));
	size_t length = WStringLength(filename) - sizeof(CHAR_T);

	const CHAR_T* sides[] = {
		STRING("_Front"),
		STRING("_Back"),
		STRING("_Up"),
		STRING("_Down"),
		STRING("_Right"),
		STRING("_Left")
	};

	StringCopySafe(strBuffer, 256, filename);

	uint32_t width, height, mips;

	for (char i = 0; i < 6; i++)
	{
		StringConcatSafe(strBuffer, 256, sides[i]);
		StringConcatSafe(strBuffer, 256, STRING(".png"));
		pixels[i] = LoadImageFromDisk(strBuffer, &width, &height, &numChannels, STBI_rgb_alpha, &size, &mips);
		ZEROMEM((CHAR_T*)((intptr_t)strBuffer + length), WStringLength(sides[i]));
	}

	// Calculate the image size and the layer size.
	const VkDeviceSize imageSize = (size_t)width * height * 4 * 6; //4 since I always load my textures with an alpha channel, and multiply it by 6 because the image must have 6 layers.
	const VkDeviceSize layerSize = imageSize / 6; //This is just the size of each layer.

	outTexture = new Texture(VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_CUBE, VK_FORMAT_R8G8B8A8_SRGB, width, height, 1, mips, 6, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, true, this);
	outTexture->TransitionImageLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	auto stagingBuffer = new VulkanMemory(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, "CreateCubemap staging buffer", false, NULL);

	void* data = stagingBuffer->Map();

	for (uint8_t i = 0; i < 6; ++i)
		memcpy((uint8_t*)data + (layerSize * i), pixels[i], static_cast<size_t>(layerSize));

	stagingBuffer->UnMap();

	outTexture->CopyFromBuffer(stagingBuffer);

	delete stagingBuffer;

	for (char i = 0; i < 6; i++)
		stbi_image_free(pixels[i]);

	outTexture->GenerateMipMaps();

	outTexture->aspect = VK_IMAGE_ASPECT_COLOR_BIT;
	outTexture->format = VK_FORMAT_R8G8B8A8_SRGB;
	outTexture->layout = { VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
	outTexture->filename = NULL;
}


void VulkanBackend::createCommandPool()
{
	auto queueFamilyIndices = findQueueFamilies(physicalDevice);

	// There are two possible flags for command pools:
	// VK_COMMAND_POOL_CREATE_TRANSIENT_BIT: Hint that command buffers are rerecorded with new commands very often(may change memory allocation behavior)
	// VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT : Allow command buffers to be rerecorded individually, without this flag they all have to be reset together
	// We will be recording a command buffer every frame, so we want to be able to reset and rerecord over it. Thus, we need to set the VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT flag bit for our command pool.
	auto poolInfo = MakeCommandPoolCreateInfo(queueFamilyIndices.graphicsFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

	if (vkCreateCommandPool(logicalDevice, &poolInfo, nullptr, &commandPool) != VK_SUCCESS)
		throw std::runtime_error("failed to create command pool!");

	for (uint32_t i = 0; i < NUMCASCADES; i++)
		vkcheck(vkCreateCommandPool(logicalDevice, &poolInfo, VK_NULL_HANDLE, &SunLight::commandPools[i]), "Failed to create opaque command pool!");
}

void VulkanBackend::CreatePostProcessingRenderPass()
{
	VkFormat depthFormat = findDepthFormat();

	std::array<VkAttachmentDescription, 1> d = {};

	d[0].format = swapChainImageFormat;
	d[0].samples = VK_SAMPLE_COUNT_1_BIT;
	d[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	d[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	d[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	d[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	d[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	d[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

	VkSubpassDescription subpassDescription = {};
	subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpassDescription.colorAttachmentCount = 1;
	subpassDescription.pColorAttachments = &colorReference;
	subpassDescription.pDepthStencilAttachment = nullptr;

	// Use subpass dependencies for layout transitions
	std::array<VkSubpassDependency, 1> dependencies;

	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	// Create the actual renderpass
	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = static_cast<uint32_t>(d.size());
	renderPassInfo.pAttachments = d.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpassDescription;
	renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
	renderPassInfo.pDependencies = dependencies.data();

	vkcheck(vkCreateRenderPass(logicalDevice, &renderPassInfo, nullptr, &postProcRenderPass), "Failed to create post processing render pass!");

	d[0].initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	d[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;

	vkcheck(vkCreateRenderPass(logicalDevice, &renderPassInfo, nullptr, &guiRenderPass), "Failed to create render pass for GUI!");
}

VkResult VulkanBackend::CreateFrameBuffer(VkImageView* attachments, uint32_t attachmentCount, VkRenderPass* renderPass, VkExtent2D size, uint32_t layers, VkFramebuffer* out_frameBuffer)
{
	VkFramebufferCreateInfo framebufferInfo{};
	framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	// We first need to specify with which renderPass the framebuffer needs to be compatible.
	// You can only use a framebuffer with the render passes that it is compatible with, which roughly means that they use the same number and type of attachments.
	framebufferInfo.renderPass = *renderPass;
	// The attachmentCount and pAttachments parameters specify the VkImageView objects that should be bound to the respective attachment descriptions in the render pass pAttachment array.
	framebufferInfo.attachmentCount = attachmentCount;
	framebufferInfo.pAttachments = attachments;
	// The width and height parameters are self-explanatory and layers refers to the number of layers in image arrays.
	framebufferInfo.width = size.width;
	framebufferInfo.height = size.height;
	// Our swap chain images are single images, so the number of layers is 1.
	framebufferInfo.layers = layers;

	return vkCreateFramebuffer(logicalDevice, &framebufferInfo, nullptr, out_frameBuffer);
}

void VulkanBackend::createFrameBuffers()
{
	swapChainFramebuffers.resize(swapChainImageViews.size());

	for (size_t i = 0; i < swapChainImageViews.size(); i++) {
		std::array<VkImageView, 1> attachments = { swapChainImageViews[i] };
		vkcheck(CreateFrameBuffer(attachments.data(), (uint32_t)attachments.size(), &postProcRenderPass, swapChainExtent, 1, &swapChainFramebuffers[i]), "failed to create framebuffer!");
	}
}

uint32_t VulkanBackend::findMemoryType(VkPhysicalDevice device, uint32_t typeFilter, VkMemoryPropertyFlags properties)
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

void VulkanBackend::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory)
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
	allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties);

	if (vkAllocateMemory(logicalDevice, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS)
		throw std::runtime_error("failed to allocate buffer memory!");

	vkBindBufferMemory(logicalDevice, buffer, bufferMemory, 0);
}

void VulkanBackend::createUniformBuffers()
{
	uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
	uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);

	psBuffers.resize(MAX_FRAMES_IN_FLIGHT);
	psBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		createBuffer(sizeof(UniformBufferObject), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformBuffers[i], uniformBuffersMemory[i]);
		createBuffer(sizeof(PostBuffer), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, psBuffers[i], psBuffersMemory[i]);
	}
}

void VulkanBackend::createCommandBuffers()
{
	commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
	commandBuffers_DepthPrepass.resize(MAX_FRAMES_IN_FLIGHT);
	commandBuffers_Post.resize(MAX_FRAMES_IN_FLIGHT);
	commandBuffers_GUI.resize(MAX_FRAMES_IN_FLIGHT);

	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = commandPool;
	// The level parameter specifies if the allocated command buffers are primary or secondary command buffers.
	// VK_COMMAND_BUFFER_LEVEL_PRIMARY: Can be submitted to a queue for execution, but cannot be called from other command buffers.
	// VK_COMMAND_BUFFER_LEVEL_SECONDARY : Cannot be submitted directly, but can be called from primary command buffers.
	// We won't make use of the secondary command buffer functionality here, but you can imagine that it's helpful to reuse common operations from primary command buffers.
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	// Since we are only allocating one command buffer, the commandBufferCount parameter is just one.
	allocInfo.commandBufferCount = (uint32_t)commandBuffers.size();

	vkcheck(vkAllocateCommandBuffers(logicalDevice, &allocInfo, commandBuffers.data()), "failed to allocate command buffers!");
	vkcheck(vkAllocateCommandBuffers(logicalDevice, &allocInfo, commandBuffers_DepthPrepass.data()), "failed to allocate command buffers!");
	vkcheck(vkAllocateCommandBuffers(logicalDevice, &allocInfo, commandBuffers_Post.data()), "failed to allocate Post command buffers!");
	vkcheck(vkAllocateCommandBuffers(logicalDevice, &allocInfo, commandBuffers_GUI.data()), "failed to allocate GUI command buffers!");
}

void VulkanBackend::createSyncObjects()
{
	imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		if (vkCreateSemaphore(logicalDevice, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
			vkCreateSemaphore(logicalDevice, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
			vkCreateFence(logicalDevice, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {

			throw std::runtime_error("failed to create synchronization objects for a frame!");
		}
	}
}

void VulkanBackend::RefreshCommandBufferRefs()
{
	commandBufferRefs.clear();
	commandBufferRefs.resize(MAX_FRAMES_IN_FLIGHT);

	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		commandBufferRefs[i] = {};

		if (theSun)
		{
			for (uint32_t j = 0; j < NUMCASCADES; j++)
				commandBufferRefs[i].push_back(theSun->commandBuffers[i][j]);
		}

		for (uint32_t j = 0; j < numSpotLights; j++)
			commandBufferRefs[i].push_back(allSpotLights[j]->commandBuffers[i]);

		commandBufferRefs[i].push_back(commandBuffers_DepthPrepass[i]);
		commandBufferRefs[i].push_back(commandBuffers[i]);
		commandBufferRefs[i].push_back(commandBuffers_Post[i]);
		commandBufferRefs[i].push_back(commandBuffers_GUI[i]);
	}

	submitInfo.commandBufferCount = static_cast<uint32_t>(commandBufferRefs[0].size());
}

void VulkanBackend::updateUniformBufferDescriptorSets()
{
	uniformBufferDescriptorSets.clear();
	uniformBufferDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);

	VkDescriptorSetAllocateInfo allocateInfo{};
	allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocateInfo.descriptorPool = DescriptorSet::descriptorPool;
	allocateInfo.descriptorSetCount = 1;
	allocateInfo.pSetLayouts = DescriptorSet::GetDescriptorSetLayout(1, 0, 1);
	allocateInfo.pNext = VK_NULL_HANDLE;

	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		vkAllocateDescriptorSets(logicalDevice, &allocateInfo, &uniformBufferDescriptorSets[i][0]);

	allocateInfo.pSetLayouts = DescriptorSet::GetDescriptorSetLayout(1, 0, 0);
	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		vkAllocateDescriptorSets(logicalDevice, &allocateInfo, &uniformBufferDescriptorSets[i][1]);


	for (uint32_t imageIndex = 0; imageIndex < MAX_FRAMES_IN_FLIGHT; imageIndex++)
	{
		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = uniformBuffers[imageIndex];
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(UniformBufferObject);

		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = beegShadowMap->view;
		imageInfo.sampler = beegShadowMap->sampler;

		VkWriteDescriptorSet write[2];
		write[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write[0].dstSet = uniformBufferDescriptorSets[imageIndex][0];
		write[0].dstBinding = 0;
		write[0].dstArrayElement = 0;
		write[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		write[0].descriptorCount = 1;
		write[0].pBufferInfo = &bufferInfo;
		write[0].pNext = VK_NULL_HANDLE;

		write[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write[1].dstSet = uniformBufferDescriptorSets[imageIndex][0];
		write[1].dstBinding = 1;
		write[1].dstArrayElement = 0;
		write[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		write[1].descriptorCount = 1;
		write[1].pImageInfo = &imageInfo;
		write[1].pNext = VK_NULL_HANDLE;

		vkUpdateDescriptorSets(logicalDevice, 2, write, 0, VK_NULL_HANDLE);

		write[0].dstSet = uniformBufferDescriptorSets[imageIndex][1];
		vkUpdateDescriptorSets(logicalDevice, 1, write, 0, VK_NULL_HANDLE);
	}
}

Camera* activeCamera;

bool MeshGroupOnScreen(RenderStageMeshGroup* meshGroup, float3& camPos, float3& camDir)
{
#ifdef ENABLE_CULLING
	float3 points[8];
	points[0] = meshGroup->boundingBoxMax;
	points[1] = meshGroup->boundingBoxMin;

	points[2] = float3(meshGroup->boundingBoxMax.x, meshGroup->boundingBoxMax.y, meshGroup->boundingBoxMin.z);
	points[3] = float3(meshGroup->boundingBoxMax.x, meshGroup->boundingBoxMin.y, meshGroup->boundingBoxMin.z);
	points[4] = float3(meshGroup->boundingBoxMax.x, meshGroup->boundingBoxMin.y, meshGroup->boundingBoxMax.z);

	points[5] = float3(meshGroup->boundingBoxMin.x, meshGroup->boundingBoxMax.y, meshGroup->boundingBoxMin.z);
	points[6] = float3(meshGroup->boundingBoxMin.x, meshGroup->boundingBoxMax.y, meshGroup->boundingBoxMin.z);
	points[7] = float3(meshGroup->boundingBoxMin.x, meshGroup->boundingBoxMin.y, meshGroup->boundingBoxMax.z);

	for (uint32_t i = 0; i < 8; i++)
	{
		if (glm::dot(glm::normalize(points[i] - camPos), camDir) > 0.0)
			return true;
	}

	return false;
#else
	return true;
#endif
}

VulkanBackend::VulkanBackend(GLFWwindow* glWindow, void (*drawGUIFunc)(VkCommandBuffer), float resolutionScale)
{
	physicalDevice = VK_NULL_HANDLE;
	queuePriority = 1.0f;
	currentFrame = 0;
	gpuTime = 0;
	this->glWindow = glWindow;
	drawGUI = drawGUIFunc;

	numShaders = 0;
	numSpotLights = 0;

	cullThreshold = 0.0;
	theSun = NULL;
	beegShadowMap = NULL;
	setup = false;

	mainRenderPass = NULL;
	depthPrepassRenderPass = NULL;

	levelFilename = NULL;
	nonLevelPackedThings = {};

	// Check validation layers
	if (enableValidationLayers && !checkValidationLayerSupport())
		throw std::runtime_error("validation layers requested, but not available!");

	VkApplicationInfo appInfo = MakeAppInfo("Zack's Engine Test App", VK_MAKE_VERSION(1, 0, 0));

	VkInstanceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;

	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	createInfo.enabledExtensionCount = glfwExtensionCount;
	createInfo.ppEnabledExtensionNames = glfwExtensions;

	if (enableValidationLayers) {
		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();
	}
	else {
		createInfo.enabledLayerCount = 0;
	}

	if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
		throw std::runtime_error("Failed to create instance!");

	// Creating Surface
	if (glfwCreateWindowSurface(instance, glWindow, nullptr, &surface) != VK_SUCCESS)
		throw std::runtime_error("failed to create window surface!");

	PickGPU();

	// Creating the logical device
	QueueFamilyIndices q = findQueueFamilies(physicalDevice);
	VkDeviceQueueCreateInfo queueCreateInfo{};
	queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueCreateInfo.queueFamilyIndex = q.graphicsFamily;
	queueCreateInfo.queueCount = 1;

	queueCreateInfo.pQueuePriorities = &queuePriority;

	VkPhysicalDeviceFeatures deviceFeatures{};
	EnableRequiredFeatures(&deviceFeatures);

	VkDeviceCreateInfo deviceCreateInfo{};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
	deviceCreateInfo.queueCreateInfoCount = 1;
	deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

	deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
	deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

	// Create Queues

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<uint32_t> uniqueQueueFamilies = { q.graphicsFamily, q.presentFamily };

	for (uint32_t queueFamily : uniqueQueueFamilies) {
		VkDeviceQueueCreateInfo queueCreateInfo{};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamily;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		queueCreateInfos.push_back(queueCreateInfo);
	}

	deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();

	// Create Logical Device
	if (vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &logicalDevice) != VK_SUCCESS)
		throw std::runtime_error("failed to create logical device!");

	vkGetDeviceQueue(logicalDevice, q.presentFamily, 0, &presentQueue);
	// Get Queue Handles
	vkGetDeviceQueue(logicalDevice, q.graphicsFamily, 0, &graphicsQueue);

	// Create Swap Chain
	createSwapChain();

	VkQueryPoolCreateInfo queryPoolInfo{};
	queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
	queryPoolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
	queryPoolInfo.queryCount = 2;
	vkCreateQueryPool(logicalDevice, &queryPoolInfo, VK_NULL_HANDLE, &queryPool);

	VkFormat depthFormat = findDepthFormat();

	createLightRenderPass();

	createCommandPool();
	CreatePostProcessingRenderPass();
	createFrameBuffers();

	createUniformBuffers();


	createCommandBuffers();
	createSyncObjects();

	// Pre-filling some structures required for drawing, just so it only updates what it has to.
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;
	static VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.commandBufferCount = 3;

	// The first parameters are the render pass itself and the attachments to bind.
	// We created a framebuffer for each swap chain image where it is specified as a color attachment.
	// Thus we need to bind the framebuffer for the swapchain image we want to draw to.
	// Using the imageIndex parameter which was passed in, we can pick the right framebuffer for the current swapchain image.
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;

	static std::array<VkClearValue, 1> lightClearValues = {};
	lightClearValues[0].depthStencil = { 1.0f, 0 };

	renderPassInfo.clearValueCount = 1;
	renderPassInfo.pClearValues = lightClearValues.data();
	renderPassInfo.renderArea = { { 0, 0 }, { SHADOWMAPSIZE, SHADOWMAPSIZE } };

	// The next two parameters define the size of the render area.
	// The render area defines where shader loads and stores will take place. Pixels outside this region will have undefined values.
	// It should match the size of the attachments for best performance.
	renderPassInfo.renderArea.offset = { 0, 0 };
	renderPassInfo.renderArea.extent = swapChainExtent;

	// Note that the order of clearValues should be identical to the order of your attachments.
	clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
	clearValues[1].depthStencil = { 1.0f, 0 };
	clearValues[2].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
	clearValues[3].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
	clearValues[4].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };

	// The last two parameters define the clear values to use for VK_ATTACHMENT_LOAD_OP_CLEAR, which we used as load operation for the color attachment.
	renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	renderPassInfo.pClearValues = clearValues.data();

	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	// The first two parameters specify which semaphores to wait on before presentation can happen, just like VkSubmitInfo.
	// Since we want to wait on the command buffer to finish execution, thus our triangle being drawn, we take the semaphores which will be signalled and wait on them, thus we use signalSemaphores.
	presentInfo.waitSemaphoreCount = 1;
	// The next two parameters specify the swap chains to present images to and the index of the image for each swap chain. This will almost always be a single one.
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &swapChain;

	// There is one last optional parameter called pResults. It allows you to specify an array of VkResult values to check for every individual swap chain if presentation was successful.
	// It's not necessary if you're only using a single swap chain, because you can simply use the return value of the present function.
	presentInfo.pResults = nullptr; // Optional

	scissor.offset = { 0, 0 };
	scissor.extent = swapChainExtent;

	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(swapChainExtent.width);
	viewport.height = static_cast<float>(swapChainExtent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	// The flags parameter specifies how we're going to use the command buffer. The following values are available:

	// VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT: The command buffer will be rerecorded right after executing it once.
	// VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT : This is a secondary command buffer that will be entirely within a single render pass.
	// VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT : The command buffer can be resubmitted while it is also already pending execution.
	// None of these flags are applicable for us right now.
	beginInfo.flags = VK_FLAGS_NONE; // Optional

	// The pInheritanceInfo parameter is only relevant for secondary command buffers. It specifies which state to inherit from the calling primary command buffers.
	beginInfo.pInheritanceInfo = nullptr; // Optional

	uint32_t extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

	queryResults.resize(MAX_FRAMES_IN_FLIGHT);

	VkFormat storageFormat = VK_FORMAT_R8G8B8A8_UNORM;

	GUIBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	GUIBeginInfo.renderPass = guiRenderPass;
	GUIBeginInfo.clearValueCount = 0;
	GUIBeginInfo.renderArea = { { 0, 0 }, swapChainExtent };
	GUIBeginInfo.pNext = VK_NULL_HANDLE;

	vkDeviceWaitIdle(logicalDevice);
}

void VulkanBackend::AfterConstruction(float resolutionScale)
{
	DescriptorSet::CreateDescriptorPool();

	CreateMainFrameBuffer(resolutionScale);

	Light::lightShaderOpaqueStatic = new Shader(STRING("shaders/core-light-static.zlsl"), STRING("shaders/core-light-static_vert.spv"), NULL, Light::renderPass, SF_SHADOW, swapChainExtent, VK_CULL_MODE_NONE, VK_POLYGON_MODE_FILL, VK_SAMPLE_COUNT_1_BIT, BM_OPAQUE, true, true, NULL, 0, 0, VK_COMPARE_OP_EQUAL, 0, 0.0f, false, this);
	Light::lightShaderMaskedStatic = new Shader(STRING("shaders/core-light-masked-static.zlsl"), STRING("shaders/core-light-masked-static_vert.spv"), STRING("shaders/core-light-masked-static_pixl.spv"), Light::renderPass, SF_SHADOW, swapChainExtent, VK_CULL_MODE_NONE, VK_POLYGON_MODE_FILL, VK_SAMPLE_COUNT_1_BIT, BM_OPAQUE, true, true, NULL, 0, 0, VK_COMPARE_OP_EQUAL, 0, 0.0f, true, this);

	UI2DPipeline = new Shader(STRING("shaders/core-debug2d.zlsl"), STRING("shaders/core-debug2d_vert.spv"), STRING("shaders/core-debug2d_pixl.spv"), mainRenderPass, SF_DEFAULT, renderExtent, VK_CULL_MODE_NONE, VK_POLYGON_MODE_FILL, msaaSamples, BM_OPAQUE, false, false, NULL, 0, 0, VK_COMPARE_OP_ALWAYS, 0, 0.0f, false, this);
	UI3DPipeline = new Shader(STRING("shaders/core-debug3d.zlsl"), STRING("shaders/core-debug3d_vert.spv"), STRING("shaders/core-debug3d_pixl.spv"), mainRenderPass, SF_DEFAULT, renderExtent, VK_CULL_MODE_NONE, VK_POLYGON_MODE_FILL, msaaSamples, BM_OPAQUE, false, false, NULL, 0, 0, VK_COMPARE_OP_ALWAYS, 0, 0.0f, false, this);

	debugBBoxShader = new Shader(STRING("shaders/core-debug-bbox.zlsl"), STRING("shaders/core-debug-bbox_vert.spv"), STRING("shaders/core-debug-bbox_pixl.spv"), mainRenderPass, SF_DEFAULT, renderExtent, VK_CULL_MODE_NONE, VK_POLYGON_MODE_LINE, msaaSamples, BM_OPAQUE, true, true, NULL, 0, 0, VK_COMPARE_OP_ALWAYS, 0, 0.0f, false, this);

	depthPrepassStaticShader = new Shader(STRING("shaders/core-light-static.zlsl"), STRING("shaders/core-light-static_vert.spv"), NULL, depthPrepassRenderPass, SF_SHADOW, swapChainExtent, VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_FILL, msaaSamples, BM_OPAQUE, true, true, NULL, 0, 0, VK_COMPARE_OP_EQUAL, 0, DEPTH_PREPASS_BIAS, false, this);

	debugBoundingBox = Mesh::LoadMesh("debug_bbox");

	depthPrepassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	depthPrepassBeginInfo.framebuffer = depthPrepassFrameBuffer;
	depthPrepassBeginInfo.renderPass = depthPrepassRenderPass;
	depthPrepassBeginInfo.clearValueCount = 1;
	depthPrepassBeginInfo.pClearValues = &clearValues[1];
	depthPrepassBeginInfo.renderArea = { { 0, 0 }, renderExtent };
	depthPrepassBeginInfo.pNext = VK_NULL_HANDLE;
}

static inline VkAccelerationStructureBuildGeometryInfoKHR MakeAccelerationStructureBuildGeometryInfo(VkBuildAccelerationStructureModeKHR mode, VkAccelerationStructureTypeKHR type, VkBuildAccelerationStructureFlagsKHR flags, uint32_t geometryCount, const VkAccelerationStructureGeometryKHR* pGeometries, const VkAccelerationStructureGeometryKHR* const* ppGeometries, VkDeviceOrHostAddressKHR scratchData, VkAccelerationStructureKHR srcStructure, VkAccelerationStructureKHR dstStructure)
{
	VkAccelerationStructureBuildGeometryInfoKHR info{};
	info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	info.dstAccelerationStructure = dstStructure;
	info.flags = flags;
	info.mode = mode;
	info.geometryCount = geometryCount;
	info.type = type;
	info.pGeometries = pGeometries;
	info.ppGeometries = ppGeometries;
	info.scratchData = scratchData;
	info.srcAccelerationStructure = srcStructure;
	info.pNext = VK_NULL_HANDLE;
}

static inline VkAccelerationStructureGeometryKHR MakeAccelerationStructureGeometry(VkGeometryTypeKHR geometryType, VkAccelerationStructureGeometryDataKHR geometry, VkGeometryFlagsKHR flags)
{
	VkAccelerationStructureGeometryKHR geom{};
	geom.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	geom.geometryType = geometryType;
	geom.geometry = geometry;
	geom.pNext = VK_NULL_HANDLE;
	return geom;
}

static inline VkAccelerationStructureGeometryTrianglesDataKHR MakeAccelerationStructureTriangleData(VkDeviceOrHostAddressConstKHR indexData, VkIndexType indexType, VkDeviceOrHostAddressConstKHR vertexData, VkFormat vertexFormat, VkDeviceSize vertexStride, uint32_t maxVertex, VkDeviceOrHostAddressConstKHR transformData)
{
	VkAccelerationStructureGeometryTrianglesDataKHR triangleData{};
	triangleData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
	triangleData.indexData = indexData;
	triangleData.indexType = indexType;
	triangleData.vertexData = vertexData;
	triangleData.vertexFormat = vertexFormat;
	triangleData.vertexStride = vertexStride;
	triangleData.maxVertex = maxVertex;
	triangleData.transformData = transformData;
	triangleData.pNext = VK_NULL_HANDLE;
	return triangleData;
}

/*
void VulkanBackend::InitRayTracing()
{
	VkAccelerationStructureKHR accelerationStructure;

	VkAccelerationStructureGeometryDataKHR geometr{};
	geometr.triangles = MakeAccelerationStructureTriangleData();
	geometr.instances = 1;

	auto geometry = MakeAccelerationStructureGeometry(
		VK_GEOMETRY_TYPE_TRIANGLES_KHR,
		geometr,
		VK_FLAGS_NONE
		);

	auto buildInfo = MakeAccelerationStructureBuildGeometryInfo(
		VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
		VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
		VK_FLAGS_NONE,
		0,
		VK_NULL_HANDLE,
		VK_NULL_HANDLE,
	);

	uint32_t maxPrimitiveCount = 0;
	VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
	vkGetAccelerationStructureBuildSizesKHR(logicalDevice, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &maxPrimitiveCount, &sizeInfo);
}
*/

void VulkanBackend::RecordBufferForCopyingToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, uint32_t depth, uint32_t layerCount)
{
	VkCommandBuffer commandBuffer = beginSingleTimeCommands();

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

	endSingleTimeCommands(commandBuffer);
}

void VulkanBackend::CreateShadowPassShader()
{
	SunLight::shadowPassShader = new Shader(STRING("shaders/shadowPass-sun.zlsl"), STRING("shaders/post_vert.spv"), STRING("shaders/shadowPass-sun_pixl.spv"), SunLight::sunShadowPassRenderPass, SF_SUNSHADOWPASS, swapChainExtent, VK_CULL_MODE_NONE, VK_POLYGON_MODE_FILL, VK_SAMPLE_COUNT_1_BIT, BM_OPAQUE, false, false, NULL, 0, 0, VK_COMPARE_OP_EQUAL, 0, 0.0f, false, this);
	SpotLight::shadowPassShader = new Shader(STRING("shaders/shadowPass-spot.zlsl"), STRING("shaders/post_vert.spv"), STRING("shaders/shadowPass-spot_pixl.spv"), SpotLight::spotShadowPassRenderPass, SF_SPOTSHADOWPASS, swapChainExtent, VK_CULL_MODE_NONE, VK_POLYGON_MODE_FILL, VK_SAMPLE_COUNT_1_BIT, BM_MAX, false, false, NULL, 0, 0, VK_COMPARE_OP_EQUAL, 0, 0.0f, false, this);
}

void VulkanBackend::cleanupSwapChain()
{
	for (size_t i = 0; i < swapChainFramebuffers.size(); i++)
		vkDestroyFramebuffer(logicalDevice, swapChainFramebuffers[i], nullptr);

	for (size_t i = 0; i < swapChainImageViews.size(); i++)
		vkDestroyImageView(logicalDevice, swapChainImageViews[i], nullptr);

	vkDestroySwapchainKHR(logicalDevice, swapChain, nullptr);
}

void VulkanBackend::RecreateMainFrameBuffer(float resolutionScale)
{
	DestroyMainFrameBuffer();
	CreateMainFrameBuffer(resolutionScale);
}

VulkanBackend::~VulkanBackend()
{
	delete RTShader;

	vkDestroyQueryPool(logicalDevice, queryPool, VK_NULL_HANDLE);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		vkDestroySemaphore(logicalDevice, renderFinishedSemaphores[i], nullptr);
		vkDestroySemaphore(logicalDevice, imageAvailableSemaphores[i], nullptr);
		vkDestroyFence(logicalDevice, inFlightFences[i], nullptr);
	}

	DestroyMainFrameBuffer();

	delete RTTexture;

	for (const auto& set : UI3DDescriptorSets)
		delete set.buffer;

	for (const auto& set : UI2DDescriptorSets)
		delete set.buffer;

	vkDestroyCommandPool(logicalDevice, commandPool, VK_NULL_HANDLE);
	for (uint32_t i = 0; i < NUMCASCADES; i++)
		vkDestroyCommandPool(logicalDevice, SunLight::commandPools[i], VK_NULL_HANDLE);

	cleanupSwapChain();

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		vkDestroyBuffer(logicalDevice, uniformBuffers[i], nullptr);
		vkFreeMemory(logicalDevice, uniformBuffersMemory[i], nullptr);
		vkDestroyBuffer(logicalDevice, psBuffers[i], nullptr);
		vkFreeMemory(logicalDevice, psBuffersMemory[i], nullptr);
	}

	delete Mesh::allIndexBuffer;
	delete Mesh::allVertexBuffer;

	vkDestroyRenderPass(logicalDevice, postProcRenderPass, VK_NULL_HANDLE);
	vkDestroyRenderPass(logicalDevice, mainRenderPass, VK_NULL_HANDLE);
	vkDestroyRenderPass(logicalDevice, depthPrepassRenderPass, VK_NULL_HANDLE);
	vkDestroyRenderPass(logicalDevice, guiRenderPass, VK_NULL_HANDLE);
	vkDestroyRenderPass(logicalDevice, Light::renderPass, VK_NULL_HANDLE);

	vkDestroyDescriptorPool(logicalDevice, DescriptorSet::descriptorPool, nullptr);

	vkDestroyDevice(logicalDevice, nullptr);
	vkDestroySurfaceKHR(instance, surface, nullptr);
	vkDestroyInstance(instance, nullptr);
}

uint32_t VulkanBackend::GetGraphicsFamily()
{
	QueueFamilyIndices q = findQueueFamilies(physicalDevice);
	return q.graphicsFamily;
}

VkCommandBuffer VulkanBackend::beginSingleTimeCommands()
{
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = commandPool;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	vkAllocateCommandBuffers(logicalDevice, &allocInfo, &commandBuffer);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	return commandBuffer;
}

void VulkanBackend::endSingleTimeCommands(VkCommandBuffer commandBuffer)
{
	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(graphicsQueue);

	vkFreeCommandBuffers(logicalDevice, commandPool, 1, &commandBuffer);
}

void VulkanBackend::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
	VkCommandBuffer commandBuffer = beginSingleTimeCommands();

	VkBufferCopy copyRegion{};
	copyRegion.size = size;
	vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

	endSingleTimeCommands(commandBuffer);
}


void VulkanBackend::CreateStaticBuffer(void* data, size_t dataSize, VkBufferUsageFlags usage, VkBuffer& buffer, VkDeviceMemory& memory)
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

inline void VulkanBackend::updateUniformBuffer(Camera* activeCamera, uint32_t imageIndex)
{
	static auto startTime = std::chrono::high_resolution_clock::now();
	auto currentTime = std::chrono::high_resolution_clock::now();
	UniformBufferObject* ubo;

	vkMapMemory(logicalDevice, uniformBuffersMemory[imageIndex], 0, sizeof(UniformBufferObject), 0, (void**)&ubo);

	ubo->time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
	ubo->CAMERA = activeCamera->position;
	ubo->viewProj = activeCamera->matrix;

	vkUnmapMemory(logicalDevice, uniformBuffersMemory[imageIndex]);
}

void VulkanBackend::recordGUICommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
	vkResetCommandBuffer(commandBuffer, 0);

	vkcheck(vkBeginCommandBuffer(commandBuffer, &beginInfo), "failed to begin recording GUI command buffer!");

	GUIBeginInfo.framebuffer = swapChainFramebuffers[imageIndex];

	vkCmdBeginRenderPass(commandBuffer, &GUIBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		drawGUI(commandBuffer);
	vkCmdEndRenderPass(commandBuffer);

	if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
		throw std::runtime_error("failed to record command buffer!");
}

void VulkanBackend::createImageViews()
{
	swapChainImageViews.resize(swapChainImages.size());

	for (uint32_t i = 0; i < swapChainImages.size(); i++)
		createImageView(swapChainImages[i], swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1, VK_IMAGE_VIEW_TYPE_2D, 0, &swapChainImageViews[i]);
}

void VulkanBackend::recreateSwapChain()
{
	vkDeviceWaitIdle(logicalDevice);

	cleanupSwapChain();
	createSwapChain();
	createFrameBuffers();
}



void VulkanBackend::Render(Camera* activeCamera)
{
	//  Drawing
	submitInfo.pCommandBuffers = commandBufferRefs[currentFrame].data();
	submitInfo.pWaitSemaphores = &imageAvailableSemaphores[currentFrame];
	submitInfo.pSignalSemaphores = &renderFinishedSemaphores[currentFrame];

	vkResetFences(logicalDevice, 1, &inFlightFences[currentFrame]);

	// Update post processing info
	PostBuffer* ubo;
	vkMapMemory(logicalDevice, psBuffersMemory[imageIndex], 0, sizeof(PostBuffer), 0, (void**)&ubo);
	ubo->viewProj = activeCamera->matrix;
	ubo->viewMatrix = activeCamera->viewMatrix;
	ubo->camPos = float4(activeCamera->position, 1);
	ubo->velocity = activeCamera->velocityVec * 4.0f;
	vkUnmapMemory(logicalDevice, psBuffersMemory[imageIndex]);

	updateUniformBuffer(activeCamera, currentFrame);
	recordGUICommandBuffer(commandBuffers_GUI[currentFrame], currentFrame);

	vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]);

	presentInfo.pImageIndices = &imageIndex;
	presentInfo.pWaitSemaphores = &renderFinishedSemaphores[currentFrame];

	auto presentStart = std::chrono::high_resolution_clock::now();
	vkQueuePresentKHR(presentQueue, &presentInfo);
	auto presentEnd = std::chrono::high_resolution_clock::now();
	presentTime = std::chrono::duration_cast<std::chrono::microseconds>(presentEnd - presentStart).count();

	// Move to the next frame-buffer in the swapchain
	++currentFrame %= MAX_FRAMES_IN_FLIGHT;
}

bool VulkanBackend::AddThingToExistingRenderStage(RenderStage* renderStage, Thing* thing, Mexel* mexel, Material* material)
{
	for (size_t i = 0; i < renderStage->shaderGroups.size(); i++)
	{
		if (renderStage->shaderGroups[i].shader == material->shader)
		{
			AddThingToShaderGroup(&renderStage->shaderGroups[i], thing, mexel, material);
			return true;
		}
	}

	return false;
}

RenderStageMeshGroup* VulkanBackend::NewMeshGroup(Thing* thing, Mexel* mexel)
{
	auto ptr = new RenderStageMeshGroup();

	thing->meshGroups.push_back(ptr);
	thing->matrixIndices.push_back(0);

	ptr->mexel = mexel;
	ptr->numInstances = 1;
	float4x4 worldMatrix = WorldMatrix(thing->position, thing->rotation, thing->scale);
	ptr->boundingBoxMax = (float3)(worldMatrix * float4(mexel->boundingBoxMax, 1));
	ptr->boundingBoxMin = (float3)(worldMatrix * float4(mexel->boundingBoxMin, 1));
	ptr->matrices = { worldMatrix };
	ptr->shadowMapOffsets = { float4(thing->shadowMapOffset, thing->shadowMapScale, thing->shadowMapScale) };
	ptr->isStatic = thing->isStatic;

	if (setup)
	{
		vkDeviceWaitIdle(logicalDevice);
		SetupMeshGroup(ptr);
	}


	return ptr;
}

RenderStageMaterialGroup* VulkanBackend::NewMaterialGroup(Material* material, Thing* object, Mexel* mexel)
{
	auto newGroup = new RenderStageMaterialGroup();
	newGroup->material = material;
	newGroup->meshGroups = { NewMeshGroup(object, mexel) };
	return newGroup;
}

void VulkanBackend::AddMexelToMainRenderStage(Thing* thing, Mexel* mexel, Material* material)
{
	if (material->shader->blendMode)
		AddMexelToRenderStage(&mainRenderStageTransparency, thing, mexel, material);
	else
	{
		AddMexelToRenderStage(&mainRenderStage, thing, mexel, material);

		if (thing->castShadow && material->shader->shaderType != SF_SKYBOX)
		{
			if (material->masked)
				AddThingToShaderGroup(&Light::shadowRenderStageMasked, thing, mexel, material);
			else
				AddThingToShaderGroup(&Light::shadowRenderStageOpaque, thing, mexel, material);
		}
	}
}

void VulkanBackend::AddMexelToRenderStage(RenderStage* renderStage, Thing* thing, Mexel* mexel, Material* material)
{
	if (AddThingToExistingRenderStage(renderStage, thing, mexel, material))
		return;

	renderStage->shaderGroups.push_back({ material->shader, { NewMaterialGroup(material, thing, mexel)} });
}

void VulkanBackend::AddThingToRenderStage(RenderStage* renderStage, Thing* thing)
{
	for (size_t i = 0; i < thing->materials.size(); i++)
	{
		auto mexel = thing->mesh->mexels[i];
		if (!mexel) continue;

		auto material = thing->materials[i];

		if (AddThingToExistingRenderStage(renderStage, thing, mexel, material))
			continue;

		renderStage->shaderGroups.push_back({ material->shader, { NewMaterialGroup(material, thing, mexel)}});
	}
}

struct ShadowMapRef
{
	uint32_t size;
	std::vector<Thing*> objects;
};
std::vector<ShadowMapRef*> shadowMapRefs = {};

static void AddThingToBeegShadowMap(Thing* thing)
{
	for (auto ref : shadowMapRefs)
	{
		if (thing->shadowMap->size.x == ref->size)
		{
			ref->objects.push_back(thing);
			return;
		}
	}

	auto newRef = new ShadowMapRef();
	newRef->size = thing->shadowMap->size.x;
	newRef->objects = { thing };
	shadowMapRefs.push_back(newRef);
}

uint32_t beegShadowMapSize = 1024;

struct ShadowMapSpot
{
	uint32_t x, y, size;
	Thing* object;
};
std::vector<ShadowMapSpot> beegShadowMapSpots = {};

static bool PointInSpot(uint32_t x, uint32_t y, ShadowMapSpot spot)
{
	if (x == spot.x && y == spot.y)
		return true;

	if (x > spot.x && y > spot.y)
	{
		if (x < (spot.x + spot.size) && y < (spot.y + spot.size))
			return true;
	}

	return false;
}

static bool SpotInSpot(ShadowMapSpot spot1, ShadowMapSpot spot2)
{
	uint32_t z = spot1.x + spot1.size;
	uint32_t w = spot1.y + spot1.size;

	return PointInSpot(spot1.x, spot1.y, spot2) || PointInSpot(z, w, spot2) || PointInSpot(z, spot1.y, spot2) || PointInSpot(spot1.x, w, spot2);
}

static bool BeegShadowMap_SpotIsEmpty(ShadowMapSpot spot)
{
	if ((spot.x + spot.size > beegShadowMapSize) || (spot.y + spot.size > beegShadowMapSize))
		return false;

	for (const auto& s : beegShadowMapSpots)
	{
		if (SpotInSpot(spot, s))
			return false;
	}

	return true;
}

static bool FillShadowMap()
{
	uint32_t x, y;
	for (const auto ref : shadowMapRefs)
	{
		for (const auto thing : ref->objects)
		{
			x = y = 0;
			while (!BeegShadowMap_SpotIsEmpty({ x, y, ref->size }))
			{
				x += ref->size;
				if (x >= beegShadowMapSize)
				{
					x = 0;
					y += ref->size;

					if (y >= beegShadowMapSize)
					{
						beegShadowMapSpots.clear();
						return true;
					}
				}
			}
			beegShadowMapSpots.push_back({ x, y, ref->size, thing });
			thing->shadowMapOffset = float2((float)x / beegShadowMapSize, (float)y / beegShadowMapSize);
			thing->shadowMapScale = (float)ref->size / beegShadowMapSize;
		}
	}

	return false;
}

void VulkanBackend::CreateBeegShadowMap()
{
	beegShadowMap = new Texture(VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_2D, BEEG_SHADOWMAP_FORMAT, beegShadowMapSize, beegShadowMapSize, 1, 1, 1, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_IMAGE_ASPECT_COLOR_BIT, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, true, this);
	beegShadowMap->TransitionImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void VulkanBackend::AddThingToExistingBeegShadowMap(Thing* thing)
{
	uint32_t size = thing->shadowMap->size.x;
	uint32_t x = 0;
	uint32_t y = 0;

	bool resize = false;

	while (!BeegShadowMap_SpotIsEmpty({ x, y, size }))
	{
		x += size;
		if (x >= beegShadowMapSize)
		{
			x = 0;
			y += size;

			if (y >= beegShadowMapSize)
			{
				resize = true;
				x = 0;
				y = 0;
				beegShadowMapSize += 256;
			}
		}
	}

	beegShadowMapSpots.push_back({ x, y, size });

	Rect srcArea = { 0, 0, size, size };
	Rect dstArea = { x, y, size, size };

	Rect oldImageArea = { 0, 0, beegShadowMap->size.x, beegShadowMap->size.y };

	Texture* oldImage = NULL;

	auto commandBuffer = beginSingleTimeCommands();
	if (resize)
	{
		oldImage = beegShadowMap;
		CreateBeegShadowMap();
		oldImage->BlitTo(commandBuffer, beegShadowMap, VK_FILTER_LINEAR, &oldImageArea, 0, 0, &oldImageArea, 0, 0);
	}
	thing->shadowMap->BlitTo(commandBuffer, beegShadowMap, VK_FILTER_LINEAR, NULL, 0, 0, &dstArea, 0, 0);
	endSingleTimeCommands(commandBuffer);

	if (oldImage)
		delete oldImage;
}



void VulkanBackend::SortAndMakeBeegShadowMap()
{
	std::cout << "Sorting Shadow Map Sizes...\n";

	bool sorted;
	size_t len = shadowMapRefs.size() - 1;
	ShadowMapRef* temp;
	do
	{
		sorted = true;

		for (size_t i = 0; i < len; i++)
		{
			if (shadowMapRefs[i]->size < shadowMapRefs[i + 1]->size)
			{
				temp = shadowMapRefs[i];
				shadowMapRefs[i] = shadowMapRefs[i + 1];
				shadowMapRefs[i + 1] = temp;
				sorted = false;
			}
		}
	} while (!sorted);

	std::cout << "Placing Shadow Maps...\n";
	beegShadowMapSpots.clear();
	beegShadowMapSize = 1024;

	while (FillShadowMap())
	{
		beegShadowMapSize += 256;

		if (beegShadowMapSize > MAX_SHADOW_MAP_SIZE)
			throw std::runtime_error("Can't fit all the shadow maps onto the beeg shadow map!");
	}


	std::cout << "Creating and Filling Beeg Shadow Map...\n";

	CreateBeegShadowMap();

	auto commandBuffer = beginSingleTimeCommands();
	Rect srcArea, dstArea;
	VkImageLayout dstLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	for (const auto& spot : beegShadowMapSpots)
	{
		srcArea = { 0, 0, spot.size, spot.size };
		dstArea = { spot.x, spot.y, spot.size, spot.size };
		spot.object->shadowMap->BlitTo(commandBuffer, beegShadowMap, VK_FILTER_LINEAR, &srcArea, 0, 0, &dstArea, 0, 0);
		dstLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}
	endSingleTimeCommands(commandBuffer);
	vkDeviceWaitIdle(logicalDevice);

	std::cout << "Deleting old shadow maps\n";

	for (const auto& spot : beegShadowMapSpots)
	{
		if (spot.object->shadowMap)
		{
			delete spot.object->shadowMap;
			spot.object->shadowMap = NULL;
		}
	}

	std::cout << "Done!\n";
}

void VulkanBackend::UpdateMeshGroupBufferDescriptorSet(RenderStageMeshGroup* meshGroup)
{
	VkDescriptorBufferInfo bufferInfos[2] = { meshGroup->matrixMem->GetBufferInfo(), meshGroup->shadowMapOffsetsMem->GetBufferInfo() };

	meshGroup->descriptorSet->Update(NULL, bufferInfos, NULL, NULL, NULL);
}

void VulkanBackend::AllocateMeshGroupBuffers(RenderStageMeshGroup* meshGroup)
{
	if (meshGroup->matrixMem) delete meshGroup->matrixMem;
	if (meshGroup->shadowMapOffsetsMem) delete meshGroup->shadowMapOffsetsMem;

	meshGroup->matrixMem = new VulkanMemory(meshGroup->matrices.size() * sizeof(float4x4), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, "MeshGroupMatrices", meshGroup->isStatic, meshGroup->matrices.data());
	meshGroup->shadowMapOffsetsMem = new VulkanMemory(meshGroup->shadowMapOffsets.size() * sizeof(float4), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, "MeshGroupShadowOffsets", true, meshGroup->shadowMapOffsets.data());
}

void VulkanBackend::SetupMeshGroup(RenderStageMeshGroup* meshGroup)
{
	AllocateMeshGroupBuffers(meshGroup);
	meshGroup->boundingBoxCentre = ((meshGroup->boundingBoxMax - meshGroup->boundingBoxMin) * float3(0.5)) + meshGroup->boundingBoxMin;

	DescriptorSetCreateInfo info{0};
	info.numStorageBuffers = 2;
	meshGroup->descriptorSet = new DescriptorSet(info);

	UpdateMeshGroupBufferDescriptorSet(meshGroup);
}

void VulkanBackend::SetupPipelineGroup(RenderStageShaderGroup* pipelineGroup)
{
	for (size_t j = 0; j < pipelineGroup->materialGroups.size(); j++)
	{
		auto materialGroup = pipelineGroup->materialGroups[j];
		for (size_t k = 0; k < materialGroup->meshGroups.size(); k++)
			SetupMeshGroup(materialGroup->meshGroups[k]);
	}
}

static void DestroyPipelineGroup(RenderStageShaderGroup* pipelineGroup)
{
	for (const auto& materialGroup : pipelineGroup->materialGroups)
	{
		if (materialGroup->meshGroups.size())
		{
			for (uint32_t k = 0; k < materialGroup->meshGroups.size(); k++)
			{
				delete materialGroup->meshGroups[k]->matrixMem;
				delete materialGroup->meshGroups[k]->shadowMapOffsetsMem;
			}

			materialGroup->meshGroups.clear();
		}
	}
	pipelineGroup->materialGroups.clear();
}

static void DestroyRenderStage(RenderStage* renderStage)
{
	renderStage->meshIDs.clear();

	for (uint32_t j = 0; j < renderStage->shaderGroups.size(); j++)
		DestroyPipelineGroup(&renderStage->shaderGroups[j]);

	renderStage->shaderGroups.clear();
}

void VulkanBackend::SetupThings()
{
	mainRenderStage.shader = NULL;
	DestroyRenderStage(&mainRenderStage);
	DestroyRenderStage(&mainRenderStageTransparency);
	DestroyPipelineGroup(&Light::shadowRenderStageOpaque);
	DestroyPipelineGroup(&Light::shadowRenderStageMasked);
	if (beegShadowMap)
	{
		delete beegShadowMap;
		beegShadowMap = NULL;
	}

	mainRenderStage.shaderGroups = {};
	mainRenderStageTransparency.shaderGroups = {};

	if (!levelIsPacked)
	{
		shadowMapRefs.clear();

		for (THING_INDEX i = 0; i < allThingsLen; i++)
			AddThingToBeegShadowMap(allThings[i]);

		SortAndMakeBeegShadowMap();
	}
	else
	{
		auto shadowMapFilename = new zstring(STRING("levels/%hs/textures/beegShadowMap.png"), (char*)*levelFilename);
		beegShadowMap = new Texture(*shadowMapFilename, true);
		delete shadowMapFilename;
	}

	std::cout << "Setting up render stage...\n";

	for (THING_INDEX i = 0; i < allThingsLen; i++)
	{
		AddToMainRenderStage(allThings[i]);

		/*
		for (auto mexel : allThings[i]->mesh->mexels)
		{
			if (!mexel) continue;
			computeObjects.push_back({
				glm::inverse(WorldMatrix(allThings[i]->position, allThings[i]->rotation, allThings[i]->scale)),
				(uint32_t)mexel->startingIndex,
				(uint32_t)mexel->IndexBufferLength,
				(uint32_t)mexel->startingVertex
			});
		}
		*/
	}

	std::cout << "Done!\n";

	std::cout << "Setting up instances...\n";

	for (size_t i = 0; i < mainRenderStage.shaderGroups.size(); i++)
		SetupPipelineGroup(&mainRenderStage.shaderGroups[i]);

	for (size_t i = 0; i < mainRenderStageTransparency.shaderGroups.size(); i++)
		SetupPipelineGroup(&mainRenderStageTransparency.shaderGroups[i]);

	SetupPipelineGroup(&Light::shadowRenderStageOpaque);
	SetupPipelineGroup(&Light::shadowRenderStageMasked);

	std::cout << "Done!\n";

	//computeObjectsMem = new VulkanMemory(this, computeObjects.size() * sizeof(ComputeObject), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, true, computeObjects.data());

	setup = true;
}

void VulkanBackend::DrawBoundingBoxes(VkCommandBuffer commandBuffer, RenderStage* renderStage, VkDescriptorSet* uniformBufferDescriptorSet)
{
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, debugBBoxShader->pipeline);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, debugBBoxShader->pipelineLayout, 0, 1, uniformBufferDescriptorSet, 0, nullptr);

	Mexel* mexel = debugBoundingBox->mexels[0];

	for (THING_INDEX i = 0; i < allThingsLen; i++)
	{
		if (allThings[i]->collisionType == CT_NONE) continue;

		if (allThings[i]->collisionType == CT_BOX)
		{
			float3 scale = (allThings[i]->mesh->boundingBoxMax - allThings[i]->mesh->boundingBoxMin);
			float3 pos = allThings[i]->mesh->boundingBoxCentre;

			auto matrix = allThings[i]->boundingBoxBuffer->Map<float4x4>();
			*matrix = allThings[i]->matrix * WorldMatrix(pos, scale);
			allThings[i]->boundingBoxBuffer->UnMap();

			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, debugBBoxShader->pipelineLayout, 1, 1, &allThings[i]->boundingBoxDescriptorSet->set, 0, VK_NULL_HANDLE);
			vkCmdDrawIndexed(commandBuffer, mexel->IndexBufferLength, 1, mexel->startingIndex, mexel->startingVertex, 0);
		}
		else
		{
			auto matrix = allThings[i]->boundingBoxBuffer->Map<float4x4>();
			*matrix = allThings[i]->matrix;
			allThings[i]->boundingBoxBuffer->UnMap();

			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, debugBBoxShader->pipelineLayout, 1, 1, &allThings[i]->boundingBoxDescriptorSet->set, 0, VK_NULL_HANDLE);
			for (auto m : allThings[i]->mesh->mexels)
				vkCmdDrawIndexed(commandBuffer, m->IndexBufferLength, 1, m->startingIndex, m->startingVertex, 0);
		}
	}
}

void VulkanBackend::DrawRenderStage(VkCommandBuffer commandBuffer, VkCommandBuffer prepassCommandBuffer, RenderStage* renderStage, VkDescriptorSet* uniformBufferDescriptorSet)
{
	Shader* pipeline;

	// It keeps track of whether or not it's bound the material/pipeline yet, so it only gets done when there's a mesh instance to draw
	bool boundMaterial;
	bool boundPipeline;

	float3 camDir = glm::normalize(activeCamera->target - activeCamera->position);

	// Objects are grouped by the mesh they use, those groups are grouped by the textures they use, and those groups are grouped by the pipeline (shader) they use.
	// It's quite convoluted but it should mean that it only binds things when absolutely necessary, without having to check anything
	for (const auto& pipelineGroup : renderStage->shaderGroups)
	{
		boundPipeline = false;
		pipeline = renderStage->shader ? renderStage->shader : pipelineGroup.shader;

		for (const auto materialGroup : pipelineGroup.materialGroups)
		{
			boundMaterial = false;

			for (const auto meshGroup : materialGroup->meshGroups)
			{
				if (meshGroup->isStatic && !MeshGroupOnScreen(meshGroup, activeCamera->position, camDir))
					continue;

				if (!boundPipeline)
				{
					vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);
					vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipelineLayout, 0, 1, uniformBufferDescriptorSet, 0, nullptr);
				}

				if (!boundMaterial)
				{
					boundMaterial = true;
					vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipelineLayout, 2, 1, *materialGroup->material->descriptorSets[0], 0, nullptr);
				}

				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipelineLayout, 1, 1, *meshGroup->descriptorSet, 0, nullptr);
				vkCmdDrawIndexed(commandBuffer, meshGroup->mexel->IndexBufferLength, meshGroup->numInstances, meshGroup->mexel->startingIndex, meshGroup->mexel->startingVertex, 0);

				if (prepassCommandBuffer && !materialGroup->material->masked && !materialGroup->material->shader->blendMode)
				{
					vkCmdBindDescriptorSets(prepassCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, depthPrepassStaticShader->pipelineLayout, 1, 1, *meshGroup->descriptorSet, 0, nullptr);
					vkCmdDrawIndexed(prepassCommandBuffer, meshGroup->mexel->IndexBufferLength, meshGroup->numInstances, meshGroup->mexel->startingIndex, meshGroup->mexel->startingVertex, 0);
				}
			}
		}
	}
}

void VulkanBackend::ResizeDebugPoints(std::vector<CombinedBufferAndDescriptorSet>& descriptorSetList, std::vector<UIInstance>& instanceList)
{
	CombinedBufferAndDescriptorSet b;
	size_t newLength = instanceList.size();

	while (descriptorSetList.size() < newLength)
	{
		UIInstance* instance = &instanceList[descriptorSetList.size()];

		b.buffer = new VulkanMemory(sizeof(float4x4) + sizeof(float4), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, "DebugPoints", false, NULL);
		DescriptorSet::AllocateDescriptorSets(1, DescriptorSet::GetDescriptorSetLayout(1, 0, 1, 0), &b.descriptorSet);

		VkDescriptorBufferInfo bufferInfo{};
		VkDescriptorImageInfo imageInfo{};

		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(float4x4) + sizeof(float4);

		imageInfo.sampler = instance->texture->sampler;
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = instance->texture->view;

		VkWriteDescriptorSet writes[2];
		writes[0].sType = writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[0].descriptorCount = writes[1].descriptorCount = 1;
		writes[0].dstArrayElement = writes[1].dstArrayElement = 0;
		writes[0].pNext = writes[1].pNext = VK_NULL_HANDLE;
		writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[0].dstBinding = 0;
		writes[1].dstBinding = 1;
		writes[0].pBufferInfo = &bufferInfo;
		writes[1].pImageInfo = &imageInfo;

		writes[0].dstSet = writes[1].dstSet = b.descriptorSet;
		bufferInfo.buffer = b.buffer->buffer;

		vkUpdateDescriptorSets(logicalDevice, 2, writes, 0, VK_NULL_HANDLE);

		descriptorSetList.push_back(b);
	}

	while (descriptorSetList.size() > newLength)
	{
		delete descriptorSetList.back().buffer;
		vkFreeDescriptorSets(logicalDevice, DescriptorSet::descriptorPool, 1, &descriptorSetList.back().descriptorSet);
		descriptorSetList.pop_back();
	}
}

struct DebugPoint2DStruct
{
	float4x4 matrix;
	float2 point;
};

struct DebugPoint3DStruct
{
	float4x4 matrix;
	float3 point;
};

void VulkanBackend::RecordMainCommandBuffer(uint32_t imageIndex)
{
	auto start = std::chrono::high_resolution_clock::now();

	Light::lightMapImageIndex = imageIndex;

	if (theSun)
		theSun->SetupSunThreads(imageIndex);

	for (size_t i = 0; i < numSpotLights; i++)
	{
		allSpotLights[i]->thread.done = false;
		allSpotLights[i]->thread.go = true;
	}

	VkCommandBuffer commandBuffer = commandBuffers[imageIndex];
	VkCommandBuffer depthCommands = commandBuffers_DepthPrepass[imageIndex];

	vkResetCommandBuffer(commandBuffer, 0);
	vkBeginCommandBuffer(commandBuffer, &beginInfo);
	BindVertexAndIndexBuffer(commandBuffer);
	vkCmdSetViewport(commandBuffer, 0, 1, &renderViewport);
	vkCmdSetScissor(commandBuffer, 0, 1, &renderPassInfo.renderArea);
	vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

	vkResetCommandBuffer(depthCommands, 0);
	vkBeginCommandBuffer(depthCommands, &beginInfo);
	BindVertexAndIndexBuffer(depthCommands);
	vkCmdSetViewport(depthCommands, 0, 1, &renderViewport);
	vkCmdSetScissor(depthCommands, 0, 1, &renderPassInfo.renderArea);
	vkCmdBindPipeline(depthCommands, VK_PIPELINE_BIND_POINT_GRAPHICS, depthPrepassStaticShader->pipeline);
	vkCmdBindDescriptorSets(depthCommands, VK_PIPELINE_BIND_POINT_GRAPHICS, depthPrepassStaticShader->pipelineLayout, 0, 1, &uniformBufferDescriptorSets[imageIndex][1], 0, nullptr);
	vkCmdBeginRenderPass(depthCommands, &depthPrepassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	DrawRenderStage(commandBuffer, depthCommands, &mainRenderStage, &uniformBufferDescriptorSets[imageIndex][0]);
	DrawRenderStage(commandBuffer, depthCommands, &mainRenderStageTransparency, &uniformBufferDescriptorSets[imageIndex][0]);

#ifdef ENABLE_DEBUG_COLLISION
	DrawBoundingBoxes(commandBuffer, &mainRenderStage, &uniformBufferDescriptorSets[imageIndex][0]);
	DrawBoundingBoxes(commandBuffer, &mainRenderStageTransparency, &uniformBufferDescriptorSets[imageIndex][0]);
#endif

	vkCmdEndRenderPass(depthCommands);
	vkEndCommandBuffer(depthCommands);

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, UI3DPipeline->pipeline);
	size_t len = UI3D.size();
	for (size_t i = 0; i < len; i++)
	{
		if (!UI3D[i].isStatic)
		{
			DebugPoint3DStruct* data = (DebugPoint3DStruct*)UI3DDescriptorSets[i].buffer->Map();
			data->matrix = activeCamera->matrix;
			data->point = UI3D[i].point;
			UI3DDescriptorSets[i].buffer->UnMap();
		}
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, UI3DPipeline->pipelineLayout, 0, 1, &UI3DDescriptorSets[i].descriptorSet, 0, VK_NULL_HANDLE);
		vkCmdDraw(commandBuffer, 6, 1, 0, 0);
	}

	len = UI2D.size();
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, UI2DPipeline->pipeline);
	for (size_t i = 0; i < len; i++)
	{
		if (!UI2D[i].isStatic)
		{
			DebugPoint2DStruct* data = (DebugPoint2DStruct*)UI2DDescriptorSets[i].buffer->Map();
			data->matrix = activeCamera->matrix;
			data->point = UI2D[i].point;
			UI2DDescriptorSets[i].buffer->UnMap();
		}
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, UI2DPipeline->pipelineLayout, 0, 1, &UI2DDescriptorSets[i].descriptorSet, 0, VK_NULL_HANDLE);
		vkCmdDraw(commandBuffer, 6, 1, 0, 0);
	}

	vkCmdEndRenderPass(commandBuffer);
	//RTShader->Go(commandBuffer, imageIndex, swapChainExtent.width, swapChainExtent.height, 1);
	vkEndCommandBuffer(commandBuffer);

	// Optimization idea: What if I combine all cascades into a single texture and use instancing to draw NUMCASCADES copies of the mesh, each one accessing their own viewProj with the instance ID
	// that way it'll draw on every shadow map at the same time with 1 draw call
	// The only caveat is that meshes can't be skipped that way, it'll waste time drawing every mesh from the big map onto the small map where it's probably not going to be visible
	// and clipping will have to be done manually in the shader

	if (theSun)
		SunLight::WaitForSunThreads();

	for (size_t i = 0; i < numSpotLights; i++)
		while (!allSpotLights[i]->thread.done);

	auto end = std::chrono::high_resolution_clock::now();

	recordTime = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
}

size_t VulkanBackend::Add2DUIElement(float2& pos, Texture* image, bool isStatic)
{
	size_t index = UI2D.size();
	UI2D.push_back({ float4(pos, 0, 0), image, isStatic });
	ResizeDebugPoints(UI2DDescriptorSets, UI2D);
	return index;
}

size_t VulkanBackend::Add3DUIElement(float3& pos, Texture* texture, bool isStatic)
{
	size_t index = UI3D.size();
	UI3D.push_back({ float4(pos, 0), texture, isStatic });
	ResizeDebugPoints(UI3DDescriptorSets, UI3D);
	return index;
}


void VulkanBackend::AddThingToShaderGroup(RenderStageShaderGroup* pipelineGroup, Thing* thing, Mexel* mexel, Material* material)
{
	RenderStageMaterialGroup* materialGroup;
	RenderStageMeshGroup* group;
	for (uint32_t i = 0; i < pipelineGroup->materialGroups.size(); i++)
	{
		materialGroup = pipelineGroup->materialGroups[i];
		if (material == materialGroup->material)
		{
			for (uint32_t j = 0; j < materialGroup->meshGroups.size(); j++)
			{
				group = materialGroup->meshGroups[j];
				if (thing->isStatic == group->isStatic && mexel == group->mexel)
				{
					thing->meshGroups.push_back(group);
					thing->matrixIndices.push_back((uint32_t)group->matrices.size());

					float4x4 worldMatrix = WorldMatrix(thing->position, thing->rotation, thing->scale);
					group->matrices.push_back(worldMatrix);
					group->boundingBoxMax = glm::max(group->boundingBoxMax, (float3)(worldMatrix * float4(mexel->boundingBoxMax, 1)));
					group->boundingBoxMin = glm::min(group->boundingBoxMin, (float3)(worldMatrix * float4(mexel->boundingBoxMin, 1)));
					group->shadowMapOffsets.push_back(float4(thing->shadowMapOffset, thing->shadowMapScale, thing->shadowMapScale));
					group->numInstances++;

					if (setup)
					{
						vkDeviceWaitIdle(logicalDevice);
						AllocateMeshGroupBuffers(group);
						UpdateMeshGroupBufferDescriptorSet(group);
					}

					return;
				}
			}

			materialGroup->meshGroups.push_back(NewMeshGroup(thing, mexel));
			return;
		}
	}

	pipelineGroup->materialGroups.push_back(NewMaterialGroup(material, thing, mexel));
}

void VulkanBackend::RecordPostProcessCommandBuffers()
{
	vkDeviceWaitIdle(logicalDevice);

	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		vkResetCommandBuffer(commandBuffers_Post[i], 0);
		recordCommandBuffer(commandBuffers_Post[i], i);
	}

	renderPassInfo.renderPass = mainRenderPass;
	renderPassInfo.framebuffer = mainFrameBuffer;
	renderPassInfo.pClearValues = clearValues.data();
	renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	renderPassInfo.renderArea = { { 0, 0 }, renderExtent };
}

static void GetStencilParametersFromString(const char* string, VkCompareOp* out_compareOp, uint32_t* out_stencilValue)
{
	switch (*string)
	{
	case '=':
		*out_compareOp = VK_COMPARE_OP_EQUAL;
		break;
	case '!':
		*out_compareOp = VK_COMPARE_OP_NOT_EQUAL;
		break;
	case '<':
		if (*(string + 1) == '=')
		{
			*out_compareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
			string++;
		}
		else
			*out_compareOp = VK_COMPARE_OP_LESS;

		break;
	case '>':
		if (*(string + 1) == '=')
		{
			*out_compareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
			string++;
		}
		else
			*out_compareOp = VK_COMPARE_OP_GREATER;
		break;
	case '*':
		*out_compareOp = VK_COMPARE_OP_ALWAYS;
		break;
	}

	string++;
	*out_stencilValue = atoi(string);
}

void VulkanBackend::ReadRenderStages(lua_State* L)
{
	// It's a bit confusing to always use -1, but it means I'll know when there's a stack-balancing issue
	lua_getglobal(L, "renderStage");

	// Backwards compatibility
	if (lua_type(L, -1) == LUA_TNIL)
	{
		lua_pop(L, 1);
		lua_getglobal(L, "renderingProcess");
	}

	lua_Integer numPasses = Lua_Len(L, -1);
	for (lua_Integer i = 0; i < numPasses; i++)
	{
		lua_geti(L, -1, i + 1);

		renderStages.push_back({});

		renderStages.back().stageType = (RenderStageType)IntFromTable(L, -1, 1, "passType");

		if (renderStages.back().stageType == RST_BLIT)
		{
			lua_geti(L, -1, 2);
			lua_getfield(L, -1, "texture");
			Texture** srcTexture = (Texture**)lua_touserdata(L, -1);
			Texture** dstTexture;
			renderStages.back().srcImage = srcTexture;
			lua_pop(L, 1);

			lua_getfield(L, -1, "width");
			renderStages.back().srcX = (uint32_t)lua_tointeger(L, -1);
			lua_pop(L, 1);

			lua_getfield(L, -1, "height");
			renderStages.back().srcY = (uint32_t)lua_tointeger(L, -1);
			lua_pop(L, 1);

			lua_getfield(L, -1, "aspect");
			renderStages.back().srcAspect = (VkImageAspectFlags)lua_tointeger(L, -1);
			lua_pop(L, 1);
			lua_pop(L, 1);


			lua_geti(L, -1, 3);
			lua_getfield(L, -1, "texture");
			dstTexture = (Texture**)lua_touserdata(L, -1);
			renderStages.back().dstImage = dstTexture;
			lua_pop(L, 1);

			lua_getfield(L, -1, "width");
			renderStages.back().dstX = (uint32_t)lua_tointeger(L, -1);
			lua_pop(L, 1);

			lua_getfield(L, -1, "height");
			renderStages.back().dstY = (uint32_t)lua_tointeger(L, -1);
			lua_pop(L, 1);

			lua_getfield(L, -1, "aspect");
			renderStages.back().dstAspect = (VkImageAspectFlags)lua_tointeger(L, -1);
			lua_pop(L, 1);
			lua_pop(L, 1);

			renderStages.back().blitFilter = (VkFilter)IntFromTable(L, -1, 4, "blitFilter");

			renderStages.back().transitionSrc = (VkImageLayout)IntFromTable_Default(L, -1, 5, 0);
			renderStages.back().transitionDst = (VkImageLayout)IntFromTable_Default(L, -1, 6, 0);

			renderStages.back().srcLayout = (VkImageLayout)IntFromTable(L, -1, 7, "srcLayout");

			if ((*srcTexture)->theoreticalLayout != renderStages.back().srcLayout)
			{
				PrintF("Pass (%i): Source texture (%s) is not in the expected layout (%s)!\n", i, string_VkImageLayout((*srcTexture)->theoreticalLayout), string_VkImageLayout(renderStages.back().srcLayout));
				throw std::runtime_error("Error reading Blit Pass");
			}

			(*srcTexture)->theoreticalLayout = renderStages.back().transitionSrc ? renderStages.back().transitionSrc : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			(*dstTexture)->theoreticalLayout = renderStages.back().transitionDst ? renderStages.back().transitionDst : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

			lua_pop(L, 1);
			continue;
		}

		lua_geti(L, -1, 2);
		RenderPass* pass = Lua_GetRenderPass(L, -1);
		renderStages.back().renderPass = pass->renderPass;
		lua_pop(L, 1);

		lua_geti(L, -1, 3);
		if (lua_type(L, -1) == LUA_TNIL)
		{
			renderStages.back().frameBuffer = NULL;
			renderStages.back().extent = swapChainExtent;
		}
		else
		{
			lua_getfield(L, -1, "buffer");
			renderStages.back().frameBuffer = (VkFramebuffer)lua_touserdata(L, -1);
			lua_pop(L, 1);

			lua_getfield(L, -1, "textures");
			lua_Integer numTextures = Lua_Len(L, -1);
			for (lua_Integer j = 0; j < numTextures; j++)
			{
				lua_geti(L, -1, j + 1);
				Texture*& tex = *(Texture**)lua_touserdata(L, -1);
				if (pass->layouts[j].from != VK_IMAGE_LAYOUT_UNDEFINED)
				{
					if (tex->theoreticalLayout != pass->layouts[j].from)
					{
						PrintF("Pass %i: Image #%i (%s) is not in the expected layout (%s)\n", i, j, string_VkImageLayout(tex->theoreticalLayout), string_VkImageLayout(pass->layouts[j].from));
						throw std::runtime_error("Error reading pass");
					}
				}
				tex->theoreticalLayout = pass->layouts[j].to;
				lua_pop(L, 1);
			}
			lua_pop(L, 1);

			renderStages.back().extent = {};
			lua_getfield(L, -1, "width");
			renderStages.back().extent.width = (uint32_t)lua_tonumber(L, -1);
			lua_pop(L, 1);

			lua_getfield(L, -1, "height");
			renderStages.back().extent.height = (uint32_t)lua_tonumber(L, -1);
			lua_pop(L, 1);
		}
		lua_pop(L, 1);

		renderStages.back().clearValues = {};
		lua_geti(L, -1, 4);
		lua_Integer numClearValues = Lua_Len(L, -1);
		for (lua_Integer j = 0; j < numClearValues; j++)
		{
			lua_geti(L, -1, j + 1);
			renderStages.back().clearValues.push_back(Lua_GetClearValue(L, -1));
			lua_pop(L, 1);
		}
		lua_pop(L, 1);

		lua_geti(L, -1, 5);
		if (lua_type(L, -1) == LUA_TTABLE)
		{
			const char* stencilTestString = StringFromTable(-1, 9);
			VkCompareOp compareOp;
			uint32_t stencilTestValue;
			GetStencilParametersFromString(stencilTestString, &compareOp, &stencilTestValue);
			auto zlsl = GetWStringFromTable(L, -1, 1);
			auto pixl = GetWStringFromTable(L, -1, 2);
			auto vert = GetWStringFromTable(L, -1, 3);
			renderStages.back().shader = allShaders[numShaders++] = new Shader(*zlsl, *vert, *pixl, renderStages.back().renderPass, IntFromTable(L, -1, 4, "shaderType"), *(VkExtent2D*)UDataFromTable(-1, 5), (VkCullModeFlagBits)IntFromTable(L, -1, 6, "Pipeline CullMode"), VK_POLYGON_MODE_FILL, (VkSampleCountFlagBits)IntFromTable(L, -1, 7, "SampleCount"), (BlendMode)IntFromTable(L, -1, 8, "BlendMode"), BoolFromTable(-1, 11), BoolFromTable(-1, 12), NULL, 0, 0, compareOp, stencilTestValue, FloatFromTable(-1, 10), false, this);
			delete zlsl;
			delete vert;
			delete pixl;
		}
		else
			renderStages.back().shader = nullptr;
		lua_pop(L, 1);

		lua_geti(L, -1, 6);
		lua_Integer idsLength = Lua_Len(L, -1);
		renderStages.back().meshIDs = {};
		for (lua_Integer i = 0; i < idsLength; i++)
			renderStages.back().meshIDs.push_back(IntFromTable(L, -1, i + 1, "meshID"));
		lua_pop(L, 1);

		renderStages.back().descriptorSet.resize(MAX_FRAMES_IN_FLIGHT);

		VkDescriptorSetAllocateInfo allocateInfo{};
		allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocateInfo.descriptorPool = DescriptorSet::descriptorPool;
		allocateInfo.descriptorSetCount = 1;
		allocateInfo.pNext = nullptr;

		if (renderStages.back().stageType == RST_SHADOW)
			allocateInfo.pSetLayouts = DescriptorSet::GetDescriptorSetLayout(0, 1, 2);
		else
			allocateInfo.pSetLayouts = renderStages.back().shader ? &renderStages.back().shader->setLayouts[0] : DescriptorSet::GetDescriptorSetLayout(1, 1, 6);

		for (uint32_t j = 0; j < MAX_FRAMES_IN_FLIGHT; j++)
			vkAllocateDescriptorSets(logicalDevice, &allocateInfo, &renderStages.back().descriptorSet[j]);

		std::vector<VkDescriptorImageInfo> imageInfos = {};
		VkDescriptorBufferInfo bufferInfo{};
		std::vector<VkWriteDescriptorSet> writes = {};
		lua_geti(L, -1, 7);
		int writesDex = lua_gettop(L);
		if (lua_type(L, writesDex))
		{
			bufferInfo.offset = 0;
			bufferInfo.range = sizeof(PostBuffer);

			writes.push_back({});
			writes.back().pNext = VK_NULL_HANDLE;
			writes.back().sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes.back().dstBinding = 0;
			writes.back().dstArrayElement = 0;
			writes.back().descriptorCount = 1;
			writes.back().descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			writes.back().pBufferInfo = &bufferInfo;

			lua_Integer numWrites = Lua_Len(L, writesDex);
			int imagesWritten = 0;
			int buffersWritten = 0;
			imageInfos.resize(numWrites);
			for (lua_Integer j = 0; j < numWrites; j++)
			{
				lua_geti(L, writesDex, j + 1);
				int writeDex = lua_gettop(L);

				writes.push_back({});

				VkDescriptorType descriptorType = (VkDescriptorType)IntFromTable(L, -1, 1, "descriptorType");
				imageInfos[imagesWritten].imageLayout = (VkImageLayout)IntFromTable(L, -1, 3, "imageLayout");
				lua_geti(L, -1, 2);
					lua_getfield(L, -1, "texture");
						Texture** tex = (Texture**)lua_touserdata(L, -1);
				lua_pop(L, 2);
				assert((*tex)->view);
				imageInfos[imagesWritten].imageView = (*tex)->view;
				assert((*tex)->sampler);
				imageInfos[imagesWritten].sampler = (*tex)->sampler;

				writes.back().sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				writes.back().dstBinding = (uint32_t)(j + 1);
				writes.back().dstArrayElement = 0;
				writes.back().descriptorType = descriptorType;
				writes.back().descriptorCount = 1;

				writes.back().pImageInfo = &imageInfos[imagesWritten++];
				writes.back().pNext = nullptr;

				lua_pop(L, 1);
			}
		}
		lua_pop(L, 1);

		for (uint32_t j = 0; j < MAX_FRAMES_IN_FLIGHT; j++)
		{
			bufferInfo.buffer = psBuffers[j];
			for (uint32_t k = 0; k < writes.size(); k++)
				writes[k].dstSet = renderStages.back().descriptorSet[j];

			vkUpdateDescriptorSets(logicalDevice, (uint32_t)writes.size(), writes.data(), 0, NULL);
		}

		lua_pop(L, 1);
	}
	lua_pop(L, 1);
}

void VulkanBackend::RecreateSwapChainStuff(float resolutionScale)
{
	recreateSwapChain();
	currentFrame = 0;
	RecreateMainFrameBuffer(resolutionScale);

	depthPrepassBeginInfo.framebuffer = depthPrepassFrameBuffer;
	depthPrepassBeginInfo.renderArea = { { 0, 0 }, renderExtent };
	depthPrepassBeginInfo.renderPass = depthPrepassRenderPass;

	GUIBeginInfo.renderArea = { { 0, 0 }, swapChainExtent };

	//viewport.width = renderExtent.width;
	//viewport.height = renderExtent.height;

	scissor.extent = renderExtent;

	DestroyPostProcessRenderStages();
}

void VulkanBackend::DestroyPostProcessRenderStages()
{
	size_t len = renderStages.size();

	for (size_t i = 0; i < len; i++)
		DestroyRenderStage(&renderStages[i]);

	renderStages.clear();
}

void VulkanBackend::DestroyRenderStages()
{
	DestroyPostProcessRenderStages();

	DestroyRenderStage(&mainRenderStage);
	DestroyRenderStage(&mainRenderStageTransparency);
	DestroyPipelineGroup(&Light::shadowRenderStageOpaque);
	DestroyPipelineGroup(&Light::shadowRenderStageMasked);
}

void VulkanBackend::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
	vkcheck(vkBeginCommandBuffer(commandBuffer, &beginInfo), "failed to begin recording post process command buffer!");

	stats.bound_buffers = 0;
	stats.bound_pipelines = 0;
	stats.drawcall_count = 0;
	stats.blits = 0;
	stats.passes = 0;
	stats.api_calls = 0;

	VkRect2D renderArea{};
	renderArea.offset = { 0, 0 };

	VkViewport passViewport{};
	passViewport.x = 0;
	passViewport.y = 0;
	passViewport.minDepth = 0.0f;
	passViewport.maxDepth = 1.0f;

	size_t len = renderStages.size();
	for (size_t i = 0; i < len; i++)
	{
		if (renderStages[i].stageType == RST_BLIT)
		{
			(*renderStages[i].srcImage)->BlitTo(commandBuffer, *renderStages[i].dstImage, renderStages[i].blitFilter, NULL, 0, 0, NULL, 0, 0, renderStages[i].transitionSrc, renderStages[i].transitionDst, renderStages[i].srcLayout);
			stats.blits++;
			continue;
		}

		renderPassInfo.clearValueCount = (uint32_t)renderStages[i].clearValues.size();
		renderPassInfo.pClearValues = renderStages[i].clearValues.data();

		renderPassInfo.renderPass = renderStages[i].renderPass;
		renderArea.extent = renderStages[i].extent;
		renderPassInfo.renderArea = renderArea;
		passViewport.width = (float)renderStages[i].extent.width;
		passViewport.height = (float)renderStages[i].extent.height;

		vkCmdSetViewport(commandBuffer, 0, 1, &passViewport);
		vkCmdSetScissor(commandBuffer, 0, 1, &renderArea);

		renderPassInfo.framebuffer = renderStages[i].frameBuffer == NULL ? swapChainFramebuffers[imageIndex] : renderStages[i].frameBuffer;

		vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
		{
			stats.passes++;
			if (renderStages[i].stageType & 1)
			{
				stats.bound_pipelines++;
				stats.api_calls += 2;

				if (renderStages[i].stageType == RST_SHADOW)
				{
					if (theSun)
					{
						vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, SunLight::shadowPassShader->pipeline);
						vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, SunLight::shadowPassShader->pipelineLayout, 0, 1, &renderStages[i].descriptorSet[imageIndex], 0, NULL);
						vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, SunLight::shadowPassShader->pipelineLayout, 1, 1, *theSun->descriptorSetPS[imageIndex], 0, VK_NULL_HANDLE);
						vkCmdDraw(commandBuffer, 3, 1, 0, 0);
						stats.drawcall_count++;
						stats.api_calls += 2;
					}

					if (numSpotLights)
					{
						vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, SpotLight::shadowPassShader->pipelineLayout, 0, 1, &renderStages[i].descriptorSet[imageIndex], 0, NULL);
						vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, SpotLight::shadowPassShader->pipeline);
						for (uint32_t j = 0; j < numSpotLights; j++)
						{
							vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, SpotLight::shadowPassShader->pipelineLayout, 1, 1, *allSpotLights[j]->descriptorSetPS[imageIndex], 0, VK_NULL_HANDLE);
							vkCmdDraw(commandBuffer, 3, 1, 0, 0);
							stats.drawcall_count++;
							stats.api_calls += 2;
						}
					}
				}
				else
				{
					vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderStages[i].shader->pipeline);
					vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderStages[i].shader->pipelineLayout, 0, 1, &renderStages[i].descriptorSet[imageIndex], 0, NULL);

					stats.drawcall_count++;
					vkCmdDraw(commandBuffer, 3, 1, 0, 0);
					stats.api_calls += 3;
				}
			}
		}
		vkCmdEndRenderPass(commandBuffer);
		stats.api_calls += 4;
	}

	if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
		throw std::runtime_error("failed to record command buffer!");
}

static float SizeOfMesh(Mesh* mesh)
{
	float3 size = mesh->boundingBoxMax - mesh->boundingBoxMin;
	return size.x * size.y * size.z;
}

void VulkanBackend::SortThings()
{
	uint16_t len = allThingsLen - 1;
	Thing* temp;
	bool sorted;
	do
	{
		sorted = true;

		for (uint16_t i = 0; i < len; i++)
		{
			if (SizeOfMesh(allThings[i]->mesh) < SizeOfMesh(allThings[i + 1]->mesh))
			{
				sorted = false;

				temp = allThings[i];
				allThings[i] = allThings[i + 1];
				allThings[i + 1] = temp;
			}
		}
	} while (!sorted);
}

void VulkanBackend::UnloadLevel()
{
	delete levelFilename;
	levelFilename = NULL;

	vkDeviceWaitIdle(logicalDevice);

	if (!levelIsPacked)
		delete beegShadowMap;

	delete cubemap;
	delete skyCubeMap;

	SunLight::DeleteSunThreads();

	if (theSun)
		delete theSun;

	for (uint32_t i = 0; i < numSpotLights; i++)
		delete allSpotLights[i];

	numSpotLights = 0;

	DescriptorSet::DestroyAllSetLayouts();
	Texture::DestroyAllTextures();

	delete UI2DPipeline;
	delete UI3DPipeline;
	delete debugBBoxShader;
	delete depthPrepassStaticShader;
	delete Light::lightShaderOpaqueStatic;
	delete Light::lightShaderMaskedStatic;
	delete SunLight::shadowPassShader;
	delete SpotLight::shadowPassShader;

	for (uint32_t i = 0; i < numShaders; i++)
		delete allShaders[i];

	numShaders = 0;
	numMaterials = 0;

	Mesh::DeleteAllMeshes();

	// When deleting a thing, it is automatically removed from allThings, so this looks weird but it works
	while (allThingsLen)
		delete allThings[0];

	DestroyRenderStages();
	renderStages.clear();
}

void VulkanBackend::AddToMainRenderStage(Thing* thing)
{
	size_t len = std::min(thing->materials.size(), thing->mesh->mexels.size());
	for (size_t i = 0; i < len; i++)
	{
		if (!thing->mesh->mexels[i]) continue;

		AddMexelToMainRenderStage(thing, thing->mesh->mexels[i], thing->materials[i]);
	}
}

void VulkanBackend::UpdateComputeBuffer()
{
	auto buffer = (ComputeShaderConfig*)RTShader->uniformBuffers[imageIndex]->Map();

	buffer->camDir = float4(normalize(activeCamera->target - activeCamera->position), 1);
	buffer->camPos = float4(activeCamera->position, 1);
	buffer->numIndices = (uint32_t)Mesh::allIndices.size();
	buffer->numVertices = (uint32_t)Mesh::allVertices.size();
	buffer->numObjects = (uint32_t)computeObjects.size();
	buffer->height = swapChainExtent.height;
	buffer->width = swapChainExtent.width;
	buffer->viewProj = activeCamera->matrix;
	buffer->view = activeCamera->viewMatrix;

	RTShader->uniformBuffers[imageIndex]->UnMap();
}

bool VulkanBackend::PerFrame()
{
	auto fenceStart = std::chrono::high_resolution_clock::now();
	VkResult vr = vkWaitForFences(logicalDevice, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
	auto fenceEnd = std::chrono::high_resolution_clock::now();

	if (vr == VK_ERROR_DEVICE_LOST)
	{
		std::cout << "\nERROR: Device was lost!\n\n";
		return false;
	}

	gpuTime = std::chrono::duration_cast<std::chrono::microseconds>(fenceEnd - fenceStart).count();

	//UpdateComputeBuffer();
#ifndef RECORD_MAIN_ONCE
	RecordMainCommandBuffer(currentFrame);
#endif

	auto acquireStart = std::chrono::high_resolution_clock::now();
	vr = vkAcquireNextImageKHR(logicalDevice, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
	auto acquireEnd = std::chrono::high_resolution_clock::now();
	acquireTime = std::chrono::duration_cast<std::chrono::microseconds>(acquireEnd - acquireStart).count();

	// Checking if you've resized the window or otherwise need to re-create the frame-buffer
	if (vr == VK_ERROR_OUT_OF_DATE_KHR || vr == VK_SUBOPTIMAL_KHR)
		return true;

	// Whenever a spot light is updated, it needs to update the matrix on each swap chain image
	// It can't update them all since it's most likely still using one of them to draw
	// So it updates the current swap chain's matrix until it's gone through all of them
	for (uint32_t i = 0; i < numSpotLights; i++)
	{
		if (allSpotLights[i]->updateTimer)
		{
			allSpotLights[i]->UpdateMatrix(NULL, currentFrame);
			allSpotLights[i]->updateTimer--;
		}
	}

	if (theSun)
	{
		sunAngle += sunSwingSpeed;
		theSun->dir = glm::normalize(float3(sin(sunAngle), cos(sunAngle), sunDownAngle));
		theSun->offset = theSun->dir * -2500.f;
		theSun->UpdateMatrix(activeCamera, currentFrame);
	}

	auto renderStart = std::chrono::high_resolution_clock::now();
	Render(activeCamera);
	auto renderEnd = std::chrono::high_resolution_clock::now();
	setupRenderTime = std::chrono::duration_cast<std::chrono::microseconds>(renderEnd - renderStart).count();

	return false;
}

void VulkanBackend::OnLevelLoad()
{
	Mesh::CreateAllVertexBuffer();
	Mesh::CreateAllIndexBuffer();
	SortThings();

#ifdef RECORD_MAIN_ONCE
	// Record command buffers (if it's only going to be done once)
	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		RecordMainCommandBuffer(i);
#endif
}

const char* String_VkResult(VkResult vr)
{
	return string_VkResult(vr);
}

SpotLight* VulkanBackend::AddSpotLight(float3& position, float3& dir, float3& colour, float fov, float attenuation)
{
	allSpotLights[numSpotLights++] = new SpotLight(position, dir, colour, fov, attenuation, SHADOWMAPSIZE, SHADOWMAPSIZE, this);

	RefreshCommandBufferRefs();
	RecordPostProcessCommandBuffers();

#ifdef RECORD_MAIN_ONCE
	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		RecordMainCommandBuffer(i);
#endif

	return allSpotLights[numSpotLights - 1];
}

inline VkCommandPoolCreateInfo MakeCommandPoolCreateInfo(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags)
{
	VkCommandPoolCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	info.queueFamilyIndex = queueFamilyIndex;
	info.flags = flags;
	info.pNext = VK_NULL_HANDLE;

	return info;
}

template<typename T>
void AddToListOfLists(std::vector<std::vector<T>>& lists, size_t index, T item)
{
	while (lists.size() <= index)
		lists.push_back({});

	lists[index].push_back(item);
}

Thing* VulkanBackend::AddThing(float3 position, float3 rotation, float3 scale, Mesh* mesh, std::vector<Material*>& materials, Texture*& shadowMap, bool isStatic, bool castsShadows, CollisionType collision, BYTE id, float shadowMapOffsetX, float shadowMapOffsetY, float shadowMapScale)
{
	Thing* thing = new Thing(position, rotation, scale, mesh, shadowMap, 1.0f, isStatic, castsShadows, collision, id, NULL);

	AddToListOfLists(idThings, id, thing);

	if (collision != CT_NONE)
		AddToListOfLists(collisionThings, id, thing);

	thing->materials.resize(materials.size());
	memcpy(thing->materials.data(), materials.data(), materials.size() * sizeof(Material*));

	thing->shadowMapOffset = float2(shadowMapOffsetX, shadowMapOffsetY);
	thing->shadowMapScale = shadowMapScale;

	if (setup)
	{
		AddToMainRenderStage(thing);
		AddThingToExistingBeegShadowMap(thing);
	}
	else if (!shadowMapScale)
	{
		for (const auto& nonLevelThings : nonLevelPackedThings)
		{
			if (allThingsLen == nonLevelThings.index)
			{
				thing->shadowMapOffset = nonLevelThings.shadowMapOffsets;
				thing->shadowMapScale = nonLevelThings.shadowMapScale;
				break;
			}
		}
	}

	allThings[allThingsLen++] = thing;

	return thing;
}

void VulkanBackend::RemoveThing(Thing* thing)
{
	for (THING_INDEX i = 0; i < allThingsLen; i++)
	{
		if (allThings[i] == thing)
		{
			for (; i < allThingsLen; i++)
				allThings[i] = allThings[i + 1];

			allThingsLen--;

			break;
		}
	}

	for (size_t i = 0; i < nonLevelPackedThings.size(); i++)
	{
		if (allThingsLen == nonLevelPackedThings[i].index)
		{
			nonLevelPackedThings.erase(nonLevelPackedThings.begin() + i);
			break;
		}
	}
}

void VulkanBackend::AddThingToNonLevelPackedThings(Thing* thing, THING_INDEX index)
{
	nonLevelPackedThings.push_back({ index, thing->shadowMapOffset, thing->shadowMapScale });
}

void VulkanBackend::SaveNonLevelPackedThings()
{
	auto filename = new zstring("levels/%s/%s.nonlevelpackedthings", (char*)*levelFilename, (char*)*levelFilename);
	FILE* file = fopen(*filename, "wb");

	delete filename;

	check(file, "Failed to open file for saving non-level packed things!");

	// I could just save the entire list in one go, but the alignment causes wasted space in the file
	// so I'm doing this manually to pack it more efficiently
	for (const auto& that : nonLevelPackedThings)
	{
		fwrite(&that.index, 1, sizeof(THING_INDEX), file);
		fwrite(&that.shadowMapOffsets, 1, sizeof(float2), file);
		fwrite(&that.shadowMapScale, 1, sizeof(float), file);
	}

	fclose(file);
}

void VulkanBackend::LoadNonLevelPackedThings()
{
#ifdef WIDE_STRINGS
	auto filename = new zstring(L"levels/%hs/%hs.nonlevelpackedthings", (char*)*levelFilename, (char*)*levelFilename);
#else
	auto filename = new zstring("levels/%s/%s.nonlevelpackedthings", (char*)*levelFilename, (char*)*levelFilename);
#endif
	auto buffer = readFile((CHAR_T*)*filename);
	delete filename;

	char* ptr = buffer.data();
	char* endPtr = ptr + buffer.size();

	THING_INDEX index;
	float2 offset;
	float scale;

	while (ptr < endPtr)
	{
		index = IncReadAs(ptr, THING_INDEX);
		offset = IncReadAs(ptr, float2);
		scale = IncReadAs(ptr, float);

		nonLevelPackedThings.push_back({ index, offset, scale });
	}
}

void VulkanBackend::RenderTo(Camera* camera, VkFramebuffer frameBuffer, VkRect2D renderArea, uint32_t clearValueCount, const VkClearValue* pClearValues)
{
	auto commandBuffer = beginSingleTimeCommands();

	VkRenderPassBeginInfo passInfo{};

	passInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	passInfo.renderArea = renderArea;
	passInfo.framebuffer = frameBuffer;
	passInfo.renderPass = mainRenderPass;
	passInfo.clearValueCount = clearValueCount;
	passInfo.pClearValues = pClearValues;
	passInfo.pNext = VK_NULL_HANDLE;

	vkCmdBeginRenderPass(commandBuffer, &passInfo, VK_SUBPASS_CONTENTS_INLINE);
		DrawRenderStage(commandBuffer, NULL, &mainRenderStage, uniformBufferDescriptorSets[currentFrame].data());
		DrawRenderStage(commandBuffer, NULL, &mainRenderStageTransparency, uniformBufferDescriptorSets[currentFrame].data());
	vkCmdEndRenderPass(commandBuffer);

	endSingleTimeCommands(commandBuffer);
}


Material* VulkanBackend::AllocateMaterial()
{
	if (numMaterials >= MAX_MATERIALS)
	{
		std::cout << "Ran out of space to allocate a new material!";
		return &allMaterials[0];
	}

	return &allMaterials[numMaterials++];
}

static void RecursiveUpdateMatrices(float4x4& cameraMatrix, Thing* thing)
{
	float4x4 overrideMatrix = cameraMatrix * WorldMatrix(thing->position, thing->rotation, thing->scale);
	thing->UpdateMatrix(&overrideMatrix);

	for (auto child : thing->children)
		RecursiveUpdateMatrices(overrideMatrix, child);
}

void VulkanBackend::UpdateCamera()
{
	activeCamera->UpdateMatrix(&perspectiveMatrix);

	if (activeCamera->attachedThings.size())
	{
		float4x4 cameraMatrix = glm::inverse(activeCamera->viewMatrix);

		for (auto thing : activeCamera->attachedThings)
			RecursiveUpdateMatrices(cameraMatrix, thing);
	}
}

/*
void VulkanBackend::LoadFont3D(const CHAR_T* fontName)
{
	CHAR_T buffer[256];
	Font3D font;

	StringCopySafe(buffer, 256, STRING("text/"));
	StringConcatSafe(buffer, 256, fontName);
	StringConcatSafe(buffer, 256, STRING(".fnt"));

	auto file = readFile(buffer);

	char* ptr = file.data();

	font.legendLength = IncReadAs(ptr, uint32_t);
	StringCopySafe(font.fontName, FONT_NAME_SIZE, fontName);

	font.legend = (CHAR_T*)malloc(font.legendLength * 2);
	memcpy(font.legend, ptr, font.legendLength);
	ptr += font.legendLength;

	font.letters = (Mexel**)malloc(font.legendLength * sizeof(Mexel*));

	for (uint32_t i = 0; i < font.legendLength; i++)
		font.letters[i] = LoadMexelFromBuffer(ptr, &ptr);

	fonts.push_back(font);
}
*/

static size_t GetFont3DLetterIndex(Font3D* font, char letter)
{
	for (size_t i = 0; i < font->legendLength; i++)
	{
		if (font->legend[i] == letter)
			return i;
	}

	return 0;
}

Font3DInstance* VulkanBackend::Add3DText(const CHAR_T* fontName, const CHAR_T* text, float3 position, float3 rotation, float3 scale, bool isStatic)
{
	auto newInstance = new Font3DInstance(this, fontName, text, position, rotation, scale, isStatic);
	text3DInstances.push_back(newInstance);
	return newInstance;
}

void VulkanBackend::GetWindowSize(uint32_t& out_width, uint32_t& out_height) const
{
	out_width = swapChainExtent.width;
	out_height = swapChainExtent.height;
}

Font3DInstance::Font3DInstance(VulkanBackend* backend, const CHAR_T* fontName, const CHAR_T* text, float3& position, float3& rotation, float3& scale, bool isStatic)
{
	text = NULL;
	indexBuffer = NULL;
	vertexBuffer = NULL;

	float4x4 matrix = WorldMatrix(position, rotation, scale);
	worldMatrix = new VulkanMemory(sizeof(float4x4), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, "Font3DInstance", isStatic, &matrix);

	DescriptorSet::AllocateDescriptorSets(1, DescriptorSet::GetDescriptorSetLayout(1, 0, 0, 0), &descriptorSet);

	SetText(text);
	Update();
}

void Font3DInstance::SetTransform(float3& position, float3& rotation, float3& scale)
{
	auto matrix = (float4x4*)worldMatrix->Map();
	*matrix = WorldMatrix(position, rotation, scale);
	worldMatrix->UnMap();
}

Camera*& GetActiveCamera()
{
	return activeCamera;
}

void SetActiveCamera(Camera* camera)
{
	GetActiveCamera() = camera;
}