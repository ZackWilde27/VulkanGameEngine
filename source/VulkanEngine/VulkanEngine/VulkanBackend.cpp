// The comments explaining vulkan are not from me, they are taken from the vulkan-tutorial guide that I followed to get started.
// I'd recommend starting there to get an idea of what my engine is doing: https://vulkan-tutorial.com

#include "VulkanBackend.h"
#include "engine.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vulkan/vk_enum_string_helper.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STBI_MSC_SECURE_CRT
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <iostream>
#include <set>
#include <limits> // Necessary for std::numeric_limits
#include <algorithm> // Necessary for std::clamp
#include "engineUtils.h"
#include "BackendUtils.h"
#include "luaUtils.h"
#include "luafunctions.h"

//#define ENABLE_RAYTRACING
//#define ENABLE_CULLING

// You may need to adjust this if you get flickering dark spots on things, it depends on your hardware for some reason
// On my PC, I can set it to 1.0 with no issues, but my laptop needs a really high bias to make it rare
// It can still happen but only if you get *really* close to something and tilt the camera around slowly
// The higher the value is, the less likely the flickering is, but the GPU will do more work shading pixels that will end up covered by other pixels
constexpr float DEPTH_PREPASS_BIAS = 20.f;

// The maximum size that the beeg shadow map can be, if the shadow map ends up larger it'll error out
constexpr uint32_t MAX_SHADOW_MAP_SIZE = 16384;

#define BEEG_SHADOWMAP_FORMAT VK_FORMAT_R8G8B8A8_UNORM

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

bool checkDeviceExtensionSupport(VkPhysicalDevice device)
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
	{ featureOffset(independentBlend), "multiple render targets" },
	//{ featureOffset(sampleRateShading), "MSAA texture sampling" }
};

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

VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
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

VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availableModes)
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

	if (vkCreateRenderPass(logicalDevice, &renderPassInfo, nullptr, &lightRenderPass) != VK_SUCCESS)
		throw std::runtime_error("failed to create light render pass!");
}

const uint32_t ResolutionScale = 1;

void VulkanBackend::CreateMainFrameBuffer()
{
	renderExtent.width = swapChainExtent.width * ResolutionScale;
	renderExtent.height = swapChainExtent.height * ResolutionScale;

	VkFormat depthFmt = findDepthStencilFormat();

	FullCreateImage(VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_2D, renderFormat, renderExtent.width, renderExtent.height, 1, 1, msaaSamples, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, &mainRenderTarget_C, false);
	FullCreateImage(VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_2D, GIFormat, renderExtent.width, renderExtent.height, 1, 1, msaaSamples, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, &mainRenderTarget_G, false);
	FullCreateImage(VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_2D, normalFormat, renderExtent.width, renderExtent.height, 1, 1, msaaSamples, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, &mainRenderTarget_N, false);
	FullCreateImage(VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_2D, positionFormat, renderExtent.width, renderExtent.height, 1, 1, msaaSamples, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_IMAGE_ASPECT_COLOR_BIT, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, &mainRenderTarget_P, false);
	FullCreateImage(VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_2D, depthFmt, renderExtent.width, renderExtent.height, 1, 1, msaaSamples, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, &mainRenderTarget_D, false);

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
		mainRenderTarget_C.View,
		mainRenderTarget_D.View,
		mainRenderTarget_N.View,
		mainRenderTarget_P.View,
		mainRenderTarget_G.View
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

	mainRenderTarget_C.theoreticalLayout = mainRenderTarget_D.theoreticalLayout = mainRenderTarget_G.theoreticalLayout = mainRenderTarget_N.theoreticalLayout = mainRenderTarget_P.theoreticalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
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

std::vector<VkDescriptorSetLayout> VulkanBackend::GetSetLayoutsFromZLSL(const char* zlsl, uint32_t* outAttachments)
{
	std::vector<VkDescriptorSetLayout> layouts;

	auto setLayoutNums = GetInfoFromZLSL(zlsl, outAttachments);
	for (const auto& nums : setLayoutNums)
		layouts.push_back(*GetDescriptorSetLayout(nums[1], nums[2], nums[0], nums[3]));

	return layouts;
}

void VulkanBackend::createDescriptorSetLayout(size_t vBuffers, size_t pBuffers, size_t numSamplers, size_t numStorageBuffers, VkDescriptorSetLayout* outLayout)
{
	VkDescriptorSetLayoutBinding* bindings = (VkDescriptorSetLayoutBinding*)malloc(sizeof(VkDescriptorSetLayoutBinding) * (vBuffers + pBuffers + numSamplers + numStorageBuffers));

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

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(vBuffers + pBuffers + numSamplers + numStorageBuffers);
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

void VulkanBackend::FullCreateImage(VkImageType imageType, VkImageViewType imageViewType, VkFormat imageFormat, int width, int height, int mipLevels, int arrayLayers, VkSampleCountFlagBits sampleCount, VkImageTiling imageTiling, VkImageUsageFlags usage, VkImageAspectFlags imageAspectFlags, VkFilter magFilter, VkFilter minFilter, VkSamplerAddressMode samplerAddressMode, Texture* out_texture, bool addSamplerToList)
{
	createImage(physicalDevice, imageType, width, height, mipLevels, arrayLayers, sampleCount, imageFormat, imageTiling, usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, out_texture->Image, out_texture->Memory);

	out_texture->View = NULL;
	// If the image is only used for transferring, there's no need to create an image view
	if (usage & ~(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT))
		createImageView(out_texture->Image, imageFormat, imageAspectFlags, mipLevels, imageViewType, 0, &out_texture->View);

	out_texture->Sampler = NULL;
	// No need to create a sampler if it won't be sampled
	if (usage & VK_IMAGE_USAGE_SAMPLED_BIT)
		GetTextureSampler(mipLevels, magFilter, minFilter, samplerAddressMode, out_texture->Sampler, addSamplerToList);

	out_texture->Width = width;
	out_texture->Height = height;
	out_texture->Aspect = imageAspectFlags;
	out_texture->freeFilename = false;
	out_texture->filename = NULL;
	out_texture->theoreticalLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	out_texture->format = imageFormat;
	out_texture->mipLevels = mipLevels;
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

void VulkanBackend::AllocateDescriptorSets(uint32_t numDescriptorSets, VkDescriptorSetLayout* pSetLayouts, VkDescriptorSet* out_sets)
{
	VkDescriptorSetAllocateInfo allocateInfo{};
	allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocateInfo.pNext = VK_NULL_HANDLE;
	allocateInfo.descriptorSetCount = numDescriptorSets;
	allocateInfo.descriptorPool = descriptorPool;
	allocateInfo.pSetLayouts = pSetLayouts;
	vkcheck(vkAllocateDescriptorSets(logicalDevice, &allocateInfo, out_sets), "Failed to AllocateDescriptorSets!");
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

void VulkanBackend::SaveTextureToPNG(Texture* texture, VkImageLayout currentLayout, const char* filename)
{
	std::vector<float4> fullData = CopyImageToBuffer(texture, currentLayout);
	stbi_write_png(filename, texture->Width, texture->Height, 4, fullData.data(), texture->Width * sizeof(float4));
}

VulkanMemory* allVertexBuffer;
VulkanMemory* allIndexBuffer;

void VulkanBackend::RunComputeShader()
{
	RTShader = new ComputeShader(this, "shaders/testcompute.comp", 1, 3, 1, 2);
	allComputeShaders.push_back(&RTShader);

	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		RTShader->uniformBuffers[i] = new VulkanMemory(this, sizeof(ComputeShaderConfig), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, "ComputeShader", false, NULL);

	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		AllocateDescriptorSets(1, &RTShader->setLayout, &RTShader->descriptorSets[i]);

	VkDescriptorBufferInfo bufferInfos[4];
	VkDescriptorImageInfo imageInfos[3];

	imageInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfos[0].imageView = mainRenderTarget_P.View;
	imageInfos[0].sampler = mainRenderTarget_P.Sampler;

	imageInfos[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfos[1].imageView = mainRenderTarget_N.View;
	imageInfos[1].sampler = mainRenderTarget_N.Sampler;

	imageInfos[2].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	imageInfos[2].imageView = RTTexture.View;
	imageInfos[2].sampler = RTTexture.Sampler;

	bufferInfos[0].offset = bufferInfos[1].offset = bufferInfos[2].offset = bufferInfos[3].offset = 0;

	bufferInfos[1].buffer = allVertexBuffer->buffer;
	bufferInfos[2].buffer = allIndexBuffer->buffer;
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

VkDescriptorSetLayout* VulkanBackend::GetDescriptorSetLayout(size_t numVBuffers, size_t numPBuffers, size_t numTextures, size_t numStorageBuffers)
{
	for (uint32_t i = 0; i < numSetLayouts; i++)
	{
		if (allSetLayouts[i].numPBuffers == numPBuffers &&
			allSetLayouts[i].numVBuffers == numVBuffers &&
			allSetLayouts[i].numSamplers == numTextures &&
			allSetLayouts[i].numStorageBuffers == numStorageBuffers) {
			return &allSetLayouts[i].setLayout;
		}
	}

	check(numSetLayouts < 100, "allSetLayouts is too small!");

	allSetLayouts[numSetLayouts].numVBuffers = numVBuffers;
	allSetLayouts[numSetLayouts].numPBuffers = numPBuffers;
	allSetLayouts[numSetLayouts].numSamplers = numTextures;
	allSetLayouts[numSetLayouts].numStorageBuffers = numStorageBuffers;
	createDescriptorSetLayout(numVBuffers, numPBuffers, numTextures, numStorageBuffers, &allSetLayouts[numSetLayouts].setLayout);
	return &allSetLayouts[numSetLayouts++].setLayout;
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



void VulkanBackend::createGraphicsPipeline(const char* vertfilename, const char* pixlfilename, VkRenderPass renderPass, VkDescriptorSetLayout* setLayouts, uint32_t numSetLayouts, int shader_type, VkExtent2D screen_size, VkCullModeFlags cullMode, VkPolygonMode polygonMode, VkSampleCountFlagBits sampleCount, BlendMode blendMode, bool depthTest, bool depthWrite, VkPushConstantRange* pushConstantRanges, uint32_t numPushConstantRanges, uint32_t numAttachments, uint32_t stencilWriteMask, VkCompareOp stencilTestOp, uint32_t stencilTestValue, float depthBias, VkPipelineLayout* outPipelineLayout, VkPipeline* outPipeline)
{
	std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
	VkShaderModule vertShaderModule = NULL;
	VkShaderModule pixlShaderModule = NULL;

	if (vertfilename)
	{
		auto vertShaderCode = readFile(vertfilename);
		vertShaderModule = createShaderModule(vertShaderCode);
		shaderStages.push_back({});
		shaderStages.back().sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStages.back().stage = VK_SHADER_STAGE_VERTEX_BIT;
		shaderStages.back().module = vertShaderModule;
		shaderStages.back().pName = "main";
	}

	if (pixlfilename)
	{
		auto pixlShaderCode = readFile(pixlfilename);
		pixlShaderModule = createShaderModule(pixlShaderCode);
		shaderStages.push_back({});
		shaderStages.back().sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStages.back().stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		shaderStages.back().module = pixlShaderModule;
		shaderStages.back().pName = "main";
	}

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	VkVertexInputBindingDescription bindingDescription;
	std::array<VkVertexInputAttributeDescription, 4> attributeDescriptions;

	if (shader_type != SF_POSTPROCESS && shader_type < SF_SUNSHADOWPASS)
	{
		bindingDescription = Vertex::getBindingDescription();
		attributeDescriptions = Vertex::getAttributeDescriptions();

		vertexInputInfo.vertexBindingDescriptionCount = 1;
		vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
		vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
		vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
	}
	else
	{
		vertexInputInfo.vertexBindingDescriptionCount = 0;
		vertexInputInfo.vertexAttributeDescriptionCount = 0;
		vertexInputInfo.pVertexBindingDescriptions = VK_NULL_HANDLE;
		vertexInputInfo.pVertexAttributeDescriptions = VK_NULL_HANDLE;
	}

	// The VkPipelineInputAssemblyStateCreateInfo struct describes two things: what kind of geometry will be drawn from the vertices and if primitive restart should be enabled.

	// The former is specified in the topology member and can have values like:
	// VK_PRIMITIVE_TOPOLOGY_POINT_LIST: points from vertices
	// VK_PRIMITIVE_TOPOLOGY_LINE_LIST : line from every 2 vertices without reuse
	// VK_PRIMITIVE_TOPOLOGY_LINE_STRIP : the end vertex of every line is used as start vertex for the next line
	// VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST : triangle from every 3 vertices without reuse
	// VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP : the second and third vertex of every triangle are used as first two vertices of the next triangle

	// Normally, the vertices are loaded from the vertex buffer by index in sequential order, but with an element buffer you can specify the indices to use yourself.
	// This allows you to perform optimizations like reusing vertices.
	// If you set the primitiveRestartEnable member to VK_TRUE, then it's possible to break up lines and triangles in the _STRIP topology modes by using a special index of 0xFFFF or 0xFFFFFFFF.
	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	// A viewport basically describes the region of the framebuffer that the output will be rendered to.
	// This will almost always be (0, 0) to (width, height)
	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = screen_size.width;
	viewport.height = screen_size.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	// While viewports define the transformation from the image to the framebuffer, scissor rectangles define in which regions pixels will actually be stored.
	// Any pixels outside the scissor rectangles will be discarded by the rasterizer.
	// They function like a filter rather than a transformation.
	// So if we wanted to draw to the entire framebuffer, we would specify a scissor rectangle that covers it entirely:
	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = screen_size;

	// When opting for dynamic viewport(s) and scissor rectangle(s) you need to enable the respective dynamic states for the pipeline:
	std::vector<VkDynamicState> dynamicStates = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicState.pDynamicStates = dynamicStates.data();

	// And then you only need to specify their count at pipeline creation time:
	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;

	// The rasterizer takes the geometry that is shaped by the vertices from the vertex shader and turns it into fragments to be colored by the fragment shader.
	// It also performs depth testing, face culling and the scissor test, and it can be configured to output fragments that fill entire polygons or just the edges (wireframe rendering).
	// All this is configured using the VkPipelineRasterizationStateCreateInfo structure.
	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;

	// If depthClampEnable is set to VK_TRUE, then fragments that are beyond the near and far planes are clamped to them as opposed to discarding them.
	// This is useful in some special cases like shadow maps. Using this requires enabling a GPU feature.
	rasterizer.depthClampEnable = VK_FALSE;

	// If rasterizerDiscardEnable is set to VK_TRUE, then geometry never passes through the rasterizer stage.This basically disables any output to the framebuffer.
	rasterizer.rasterizerDiscardEnable = VK_FALSE;

	// The polygonMode determines how fragments are generated for geometry. The following modes are available:
	// VK_POLYGON_MODE_FILL: fill the area of the polygon with fragments
	// VK_POLYGON_MODE_LINE : polygon edges are drawn as lines
	// VK_POLYGON_MODE_POINT : polygon vertices are drawn as points

	// Using any mode other than fill requires enabling a GPU feature.
	rasterizer.polygonMode = polygonMode;

	// The lineWidth member is straightforward, it describes the thickness of lines in terms of number of fragments.
	// The maximum line width that is supported depends on the hardware and any line thicker than 1.0f requires you to enable the wideLines GPU feature.
	rasterizer.lineWidth = 1.0f;

	// The cullMode variable determines the type of face culling to use.
	// You can disable culling, cull the front faces, cull the back faces or both.
	rasterizer.cullMode = cullMode;
	// The frontFace variable specifies the vertex order for faces to be considered front - facing and can be clockwise or counterclockwise.
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

	// The rasterizer can alter the depth values by adding a constant value or biasing them based on a fragment's slope.
	// Putting in my own comment, I use this to bias the depth in the depth-prepass, without it inaccuracies can cause pixels that are supposed to be drawn think they should be culled.
	rasterizer.depthBiasEnable = depthBias != 0;
	rasterizer.depthBiasConstantFactor = depthBias;
	rasterizer.depthBiasClamp = 0.0f;
	rasterizer.depthBiasSlopeFactor = 0.0f;

	// The VkPipelineMultisampleStateCreateInfo struct configures multisampling, which is one of the ways to perform anti-aliasing.
	// It works by combining the fragment shader results of multiple polygons that rasterize to the same pixel.
	// This mainly occurs along edges, which is also where the most noticeable aliasing artifacts occur.
	// Because it doesn't need to run the fragment shader multiple times if only one polygon maps to a pixel, it is significantly less expensive than simply rendering to a higher resolution and then downscaling.
	// Enabling it requires enabling a GPU feature.
	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = sampleCount > VK_SAMPLE_COUNT_1_BIT;
	multisampling.rasterizationSamples = sampleCount;
	multisampling.minSampleShading = 0.5f; // Optional
	multisampling.pSampleMask = VK_NULL_HANDLE; // Optional
	multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
	multisampling.alphaToOneEnable = VK_FALSE; // Optional

	// After a fragment shader has returned a color, it needs to be combined with the color that is already in the framebuffer.
	// This transformation is known as color blending and there are two ways to do it:
	// - Mix the old and new value to produce a final color
	// - Combine the old and new value using a bitwise operation

	// There are two types of structs to configure color blending.The first struct, VkPipelineColorBlendAttachmentState contains the configuration per attached framebuffer and the second struct, VkPipelineColorBlendStateCreateInfo contains the global color blending settings.
	// In our case we only have one framebuffer

	// The most common way to use color blending is to implement alpha blending, where we want the new color to be blended with the old color based on its opacity.
	// The finalColor should then be computed as follows:
	/*
		finalColor.rgb = newAlpha * newColor + (1 - newAlpha) * oldColor;
		finalColor.a = newAlpha.a;
	*/
	// You can find all of the possible operations in the VkBlendFactor and VkBlendOp enumerations in the specification.
	std::vector<VkPipelineColorBlendAttachmentState> blendAttachments(numAttachments);

	if (numAttachments)
	{
		blendAttachments[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

		if (blendMode != BM_OPAQUE)
		{
			blendAttachments[0].blendEnable = VK_TRUE;
			if (blendMode == BM_TRANSPARENT)
			{
				blendAttachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
				blendAttachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;

				blendAttachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
				blendAttachments[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
			}
			else
			{
				blendAttachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
				blendAttachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE;

				blendAttachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
				blendAttachments[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			}

			if (blendMode == BM_MAX)
			{
				blendAttachments[0].colorBlendOp = VK_BLEND_OP_MAX;
				blendAttachments[0].alphaBlendOp = VK_BLEND_OP_MAX;
			}
			else
			{
				blendAttachments[0].colorBlendOp = VK_BLEND_OP_ADD;
				blendAttachments[0].alphaBlendOp = VK_BLEND_OP_ADD;
			}
		}
		else
			blendAttachments[0].blendEnable = VK_FALSE;
	}

	for (uint32_t i = 1; i < numAttachments; i++)
	{
		blendAttachments[i].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		blendAttachments[i].blendEnable = VK_FALSE;
	}

	// The second structure references the array of structures for all of the framebuffers and allows you to set blend constants that you can use as blend factors in the aforementioned calculations.
	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional

	colorBlending.attachmentCount = numAttachments;
	colorBlending.pAttachments = blendAttachments.data();
	colorBlending.blendConstants[0] = 0.0f; // Optional
	colorBlending.blendConstants[1] = 0.0f; // Optional
	colorBlending.blendConstants[2] = 0.0f; // Optional
	colorBlending.blendConstants[3] = 0.0f; // Optional

	// You can use uniform values in shaders, which are globals similar to dynamic state variables that can be changed at drawing time to alter the behavior of your shaders without having to recreate them.
	// They are commonly used to pass the transformation matrix to the vertex shader, or to create texture samplers in the fragment shader.

	// These uniform values need to be specified during pipeline creation by creating a VkPipelineLayout object.

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	// Tell the pipeline to reference the layout containing shader variables
	pipelineLayoutInfo.setLayoutCount = numSetLayouts;
	pipelineLayoutInfo.pSetLayouts = setLayouts;

	pipelineLayoutInfo.pushConstantRangeCount = numPushConstantRanges;
	pipelineLayoutInfo.pPushConstantRanges = pushConstantRanges;

	if (vkCreatePipelineLayout(logicalDevice, &pipelineLayoutInfo, VK_NULL_HANDLE, outPipelineLayout) != VK_SUCCESS)
		throw std::runtime_error("failed to create pipeline layout!");

	VkPipelineDepthStencilStateCreateInfo depthStencil{};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	// The depthTestEnable field specifies if the depth of new fragments should be compared to the depth buffer to see if they should be discarded.
	depthStencil.depthTestEnable = depthTest;
	// The depthWriteEnable field specifies if the new depth of fragments that pass the depth test should actually be written to the depth buffer.
	depthStencil.depthWriteEnable = depthWrite;

	// The depthCompareOp field specifies the comparison that is performed to keep or discard fragments.
	// We're sticking to the convention of lower depth = closer, so the depth of new fragments should be less
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;

	// The depthBoundsTestEnable, minDepthBounds and maxDepthBounds fields are used for the optional depth bound test.
	// Basically, this allows you to only keep fragments that fall within the specified depth range.
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.minDepthBounds = 0.0f; // Optional
	depthStencil.maxDepthBounds = 1.0f; // Optional

	depthStencil.front = {};
	depthStencil.back = {};

	depthStencil.front.failOp = depthStencil.front.depthFailOp = VK_STENCIL_OP_KEEP;

	if (stencilTestValue || stencilWriteMask)
	{
		depthStencil.stencilTestEnable = VK_TRUE;
		depthStencil.front.writeMask = depthStencil.front.compareMask = (uint32_t)-1;

		if (stencilWriteMask)
		{
			depthStencil.front.passOp = VK_STENCIL_OP_REPLACE;
			depthStencil.front.compareOp = VK_COMPARE_OP_ALWAYS;
			depthStencil.front.reference = stencilWriteMask;
		}
		else
		{
			depthStencil.front.passOp = VK_STENCIL_OP_KEEP;
			depthStencil.front.compareOp = stencilTestOp;
			depthStencil.front.reference = stencilTestValue;
		}
	}
	else
	{
		depthStencil.stencilTestEnable = VK_FALSE;
		depthStencil.front.compareMask = depthStencil.front.reference = depthStencil.front.writeMask = 0;
		depthStencil.front.passOp = VK_STENCIL_OP_KEEP;
		depthStencil.front.compareOp = VK_COMPARE_OP_ALWAYS;
	}

	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	// We start by referencing the array of VkPipelineShaderStageCreateInfo structs.
	pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
	pipelineInfo.pStages = shaderStages.data();
	// Then we reference all of the structures describing the fixed-function stage.
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = &depthStencil;
	pipelineInfo.pColorBlendState = (shader_type == SF_SHADOW) ? VK_NULL_HANDLE : &colorBlending;
	pipelineInfo.pDynamicState = &dynamicState;
	// After that comes the pipeline layout, which is a Vulkan handle rather than a struct pointer.
	pipelineInfo.layout = *outPipelineLayout;

	// And finally we have the reference to the render pass and the index of the sub pass where this graphics pipeline will be used.
	// It is also possible to use other render passes with this pipeline instead of this specific instance, but they have to be compatible with renderPass.
	// The requirements for compatibility are described here, but we won't be using that feature in this tutorial.
	pipelineInfo.renderPass = renderPass;
	pipelineInfo.subpass = 0;

	// There are actually two more parameters: basePipelineHandle and basePipelineIndex. Vulkan allows you to create a new graphics pipeline by deriving from an existing pipeline.
	// The idea of pipeline derivatives is that it is less expensive to set up pipelines when they have much functionality in common with an existing pipeline and switching between pipelines from the same parent can also be done quicker.
	// You can either specify the handle of an existing pipeline with basePipelineHandle or reference another pipeline that is about to be created by index with basePipelineIndex.
	// These values are only used if the VK_PIPELINE_CREATE_DERIVATIVE_BIT flag is also specified in the flags field of VkGraphicsPipelineCreateInfo.
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
	pipelineInfo.basePipelineIndex = -1; // Optional

	// The vkCreateGraphicsPipelines function actually has more parameters than the usual object creation functions in Vulkan.
	// It is designed to take multiple VkGraphicsPipelineCreateInfo objects and create multiple VkPipeline objects in a single call.
	// The second parameter, for which we've passed the VK_NULL_HANDLE argument, references an optional VkPipelineCache object.
	// A pipeline cache can be used to store and reuse data relevant to pipeline creation across multiple calls to vkCreateGraphicsPipelines and even across program executions if the cache is stored to a file. This makes it possible to significantly speed up pipeline creation at a later time. We'll get into this in the pipeline cache chapter.
	if (vkCreateGraphicsPipelines(logicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo, VK_NULL_HANDLE, outPipeline) != VK_SUCCESS)
		throw std::runtime_error("failed to create graphics pipeline!");

	if (vertShaderModule)
		vkDestroyShaderModule(logicalDevice, vertShaderModule, VK_NULL_HANDLE);

	if (pixlShaderModule)
		vkDestroyShaderModule(logicalDevice, pixlShaderModule, VK_NULL_HANDLE);
}

// Helper function that takes care of error handling, size, and the mip levels calculation
static stbi_uc* LoadImageFromDisk(const char* filename, uint32_t* outWidth, uint32_t* outHeight, int* outChannels, int requiredChannels, VkDeviceSize* outSize, uint32_t* outMipLevels)
{
	stbi_uc* pixels = stbi_load(filename, (int*)outWidth, (int*)outHeight, outChannels, requiredChannels);

	if (!pixels)
	{
		printf(filename);
		printf("\n");
		throw std::runtime_error("failed to load texture image!");
	}

	*outSize = ((unsigned long long)*outWidth) * (*outHeight) * 4;
	*outMipLevels = static_cast<uint32_t>(std::floor(std::log2(MAX(*outWidth, *outHeight)))) + 1;

	return pixels;
}

void VulkanBackend::createImage(VkPhysicalDevice device, VkImageType imageType, uint32_t width, uint32_t height, uint32_t mipLevels, uint32_t arrayLayers, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory) {

	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = imageType;
	imageInfo.extent.width = width;
	imageInfo.extent.height = height;
	imageInfo.extent.depth = 1;
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
	allocInfo.memoryTypeIndex = findMemoryType(device, memRequirements.memoryTypeBits, properties);

	vkcheck(vkAllocateMemory(logicalDevice, &allocInfo, nullptr, &imageMemory), "failed to allocate image memory!");

	vkBindImageMemory(logicalDevice, image, imageMemory, 0);
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

void VulkanBackend::transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, VkImageAspectFlags aspectMask, uint32_t mipLevels, uint32_t layerCount) {
	VkCommandBuffer commandBuffer = beginSingleTimeCommands();

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
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = mipLevels;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = layerCount;

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

	endSingleTimeCommands(commandBuffer);
}

void VulkanBackend::generateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels, uint32_t layerCount)
{

	VkFormatProperties formatProperties;
	vkGetPhysicalDeviceFormatProperties(physicalDevice, imageFormat, &formatProperties);

	if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
		throw std::runtime_error("texture image format does not support linear blitting!");
	}

	VkCommandBuffer commandBuffer = beginSingleTimeCommands();

	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.image = image;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = layerCount;
	barrier.subresourceRange.levelCount = 1;
	// We're going to make several transitions, so we'll reuse this VkImageMemoryBarrier.The fields set above will remain the same for all barriers.subresourceRange.miplevel, oldLayout, newLayout, srcAccessMask, and dstAccessMask will be changed for each transition.

	int32_t mipWidth = texWidth;
	int32_t mipHeight = texHeight;

	for (uint32_t i = 1; i < mipLevels; i++) {
		barrier.subresourceRange.baseMipLevel = i - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

		vkCmdPipelineBarrier(commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
			0, nullptr,
			0, nullptr,
			1, &barrier);


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

		// This barrier transitions mip level i - 1 to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL. This transition waits on the current blit command to finish. All sampling operations will wait on this transition to finish.
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
			0, nullptr,
			0, nullptr,
			1, &barrier);

		if (mipWidth > 1) mipWidth /= 2;
		if (mipHeight > 1) mipHeight /= 2;
	}

	barrier.subresourceRange.baseMipLevel = mipLevels - 1;
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	vkCmdPipelineBarrier(commandBuffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
		0, nullptr,
		0, nullptr,
		1, &barrier);

	endSingleTimeCommands(commandBuffer);
}

Texture* VulkanBackend::CreateTextureArray(Texture* textures, uint32_t numTextures, uint32_t width, uint32_t height, VkFormat format)
{
	if (numTextures >= MAX_TEXTURES)
	{
		std::cout << "Ran out of room for more textures!\n";
		return allTextures[0];
	}

	auto tex = NEW(Texture);
	allTextures[numTextures++] = tex;
	FullCreateImage(VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_2D, format, width, height, 1, numTextures, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, tex, true);

	auto commandBuffer = beginSingleTimeCommands();
	Rect srcArea{};
	Rect dstArea = { 0, 0, width, height };
	for (uint32_t i = 0; i < numTextures; i++)
	{
		srcArea = { 0, 0, textures[i].Width, textures[i].Height };
		BlitImage(commandBuffer, &textures[i], srcArea, tex, dstArea, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_FILTER_LINEAR, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0, 0, i);
	}
	endSingleTimeCommands(commandBuffer);
	vkDeviceWaitIdle(logicalDevice);

	return tex;
}

void VulkanBackend::BlitImage(VkCommandBuffer commandBuffer, Texture* from, Rect& fromArea, Texture* to, Rect& toArea, VkImageLayout srcLayout, VkFilter filter, VkImageAspectFlags srcAspect, VkImageAspectFlags dstAspect, VkImageLayout srcFinalLayout, VkImageLayout dstFinalLayout, uint32_t srcMipLevel, uint32_t dstMipLevel, uint32_t srcLayer, uint32_t dstLayer, VkImageLayout dstInitialLayout)
{
	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;

	// If you are using the barrier to transfer queue family ownership, then these two fields should be the indices of the queue families. They must be set to VK_QUEUE_FAMILY_IGNORED if you don't want to do this (not the default value!).
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

	// The image and subresourceRange specify the image that is affected and the specific part of the image. Our image is not an array and does not have mipmapping levels, so only one level and layer are specified.
	barrier.subresourceRange.baseMipLevel = srcMipLevel;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = srcLayer;
	barrier.subresourceRange.layerCount = 1;

	VkPipelineStageFlags sourceStage;
	VkPipelineStageFlags destinationStage;

	if (srcLayout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
	{
		barrier.image = from->Image;
		barrier.subresourceRange.aspectMask = srcAspect;

		// The first two fields specify layout transition. It is possible to use VK_IMAGE_LAYOUT_UNDEFINED as oldLayout if you don't care about the existing contents of the image.
		barrier.oldLayout = srcLayout;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

		LayoutToAccessMaskAndSourceStage(barrier.oldLayout, &barrier.srcAccessMask, &sourceStage);
		LayoutToAccessMaskAndSourceStage(barrier.newLayout, &barrier.dstAccessMask, &destinationStage);

		vkCmdPipelineBarrier(
			commandBuffer,
			sourceStage, destinationStage,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier
		);
	}

	barrier.image = to->Image;
	barrier.subresourceRange.baseMipLevel = dstMipLevel;
	barrier.subresourceRange.aspectMask = dstAspect;
	barrier.subresourceRange.baseArrayLayer = dstLayer;
	barrier.oldLayout = dstInitialLayout;
	barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

	LayoutToAccessMaskAndSourceStage(barrier.oldLayout, &barrier.srcAccessMask, &sourceStage);
	LayoutToAccessMaskAndSourceStage(barrier.newLayout, &barrier.dstAccessMask, &destinationStage);

	vkCmdPipelineBarrier(
		commandBuffer,
		sourceStage, destinationStage,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier
	);

	VkImageBlit blit{};
	blit.srcSubresource.mipLevel = srcMipLevel;
	blit.srcSubresource.aspectMask = srcAspect;
	blit.srcSubresource.baseArrayLayer = 0;
	blit.srcSubresource.layerCount = 1;
	blit.srcOffsets[0].x = fromArea.x;
	blit.srcOffsets[0].y = fromArea.y;
	blit.srcOffsets[0].z = 0;
	blit.srcOffsets[1].x = fromArea.x + fromArea.width;
	blit.srcOffsets[1].y = fromArea.y + fromArea.height;
	blit.srcOffsets[1].z = 1;

	blit.dstSubresource.mipLevel = dstMipLevel;
	blit.dstSubresource.aspectMask = dstAspect;
	blit.dstSubresource.layerCount = 1;
	blit.dstSubresource.baseArrayLayer = 0;
	blit.dstOffsets[0].x = toArea.x;
	blit.dstOffsets[0].y = toArea.y;
	blit.dstOffsets[0].z = 0;
	blit.dstOffsets[1].x = toArea.x + toArea.width;
	blit.dstOffsets[1].y = toArea.y + toArea.height;
	blit.dstOffsets[1].z = 1;

	vkCmdBlitImage(commandBuffer, from->Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, to->Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, filter);

	if (dstFinalLayout != VK_IMAGE_LAYOUT_UNDEFINED)
	{
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		LayoutToAccessMaskAndSourceStage(barrier.oldLayout, &barrier.srcAccessMask, &sourceStage);

		barrier.newLayout = dstFinalLayout;
		LayoutToAccessMaskAndSourceStage(barrier.newLayout, &barrier.dstAccessMask, &destinationStage);

		vkCmdPipelineBarrier(
			commandBuffer,
			sourceStage, destinationStage,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier
		);
	}

	if (srcFinalLayout != VK_IMAGE_LAYOUT_UNDEFINED)
	{
		barrier.image = from->Image;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.subresourceRange.aspectMask = srcAspect;
		barrier.subresourceRange.baseMipLevel = srcMipLevel;
		barrier.subresourceRange.baseArrayLayer = srcLayer;

		LayoutToAccessMaskAndSourceStage(barrier.oldLayout, &barrier.srcAccessMask, &sourceStage);

		barrier.newLayout = srcFinalLayout;
		LayoutToAccessMaskAndSourceStage(barrier.newLayout, &barrier.dstAccessMask, &destinationStage);

		vkCmdPipelineBarrier(
			commandBuffer,
			sourceStage, destinationStage,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier
		);
	}
}

void VulkanBackend::OneTimeBlit(Texture* from, Rect& fromArea, Texture* to, Rect& toArea, VkImageLayout srcLayout, VkFilter filter, VkImageAspectFlags srcAspect, VkImageAspectFlags dstAspect, VkImageLayout srcFinalLayout, VkImageLayout dstFinalLayout)
{
	auto commandBuffer = beginSingleTimeCommands();
	BlitImage(commandBuffer, from, fromArea, to, toArea, srcLayout, filter, srcAspect, dstAspect, srcFinalLayout, dstFinalLayout);
	endSingleTimeCommands(commandBuffer);
}


void VulkanBackend::CreateCubemap(const char* filename, Texture* outTexture)
{
	int numChannels;
	VkDeviceSize size;

	std::vector<stbi_uc*> pixels = {};
	pixels.resize(6);

	ZEROMEM(strBuffer, 256);
	size_t length = strlen(filename);

	const char* sides[] = {
		"_Front",
		"_Back",
		"_Up",
		"_Down",
		"_Right",
		"_Left"
	};

	StringCopySafe(strBuffer, 256, filename);
	size_t sideLength;

	for (char i = 0; i < 6; i++)
	{
		StringConcatSafe(strBuffer, 256, sides[i]);
		StringConcatSafe(strBuffer, 256, ".png");
		pixels[i] = LoadImageFromDisk(strBuffer, &outTexture->Width, &outTexture->Height, &numChannels, STBI_rgb_alpha, &size, &outTexture->mipLevels);
		ZEROMEM((char*)((intptr_t)strBuffer + length), strlen(sides[i]));
	}

	//Calculate the image size and the layer size.
	const VkDeviceSize imageSize = outTexture->Width * outTexture->Height * 4 * 6; //4 since I always load my textures with an alpha channel, and multiply it by 6 because the image must have 6 layers.
	const VkDeviceSize layerSize = imageSize / 6; //This is just the size of each layer.

	createImage(physicalDevice, VK_IMAGE_TYPE_2D, outTexture->Width, outTexture->Height, outTexture->mipLevels, 6, VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, outTexture->Image, outTexture->Memory);

	transitionImageLayout(outTexture->Image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, outTexture->mipLevels, 6);

	//Set up the staging buffer.
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	//Create the staging buffer.
	createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
	//Map the memory.
	void* data;
	vkMapMemory(logicalDevice, stagingBufferMemory, 0, imageSize, 0, &data);

	//Copy the data into the staging buffer.
	for (uint8_t i = 0; i < 6; ++i)
		memcpy((uint8_t*)data + (layerSize * i), pixels[i], static_cast<size_t>(layerSize));

	vkUnmapMemory(logicalDevice, stagingBufferMemory);

	RecordBufferForCopyingToImage(stagingBuffer, outTexture->Image, static_cast<uint32_t>(outTexture->Width), static_cast<uint32_t>(outTexture->Height), 6);

	vkDestroyBuffer(logicalDevice, stagingBuffer, nullptr);
	vkFreeMemory(logicalDevice, stagingBufferMemory, nullptr);

	for (char i = 0; i < 6; i++)
		stbi_image_free(pixels[i]);

	generateMipmaps(outTexture->Image, VK_FORMAT_R8G8B8A8_SRGB, outTexture->Width, outTexture->Height, outTexture->mipLevels, 6);
	createImageView(outTexture->Image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, outTexture->mipLevels, VK_IMAGE_VIEW_TYPE_CUBE, 0, &outTexture->View);
	GetTextureSampler(outTexture->mipLevels, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, outTexture->Sampler, true);
}

void VulkanBackend::createTextureImage(const char* filename, bool isNormal, bool freeFilename, Texture* outTex)
{
	int texChannels;
	VkDeviceSize imageSize;
	stbi_uc* pixels = LoadImageFromDisk(filename, &outTex->Width, &outTex->Height, &texChannels, STBI_rgb_alpha, &imageSize, &outTex->mipLevels);

	VkFormat imageFormat = isNormal ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_R8G8B8A8_SRGB;

	createImage(physicalDevice, VK_IMAGE_TYPE_2D, outTex->Width, outTex->Height, outTex->mipLevels, 1, VK_SAMPLE_COUNT_1_BIT, imageFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, outTex->Image, outTex->Memory);
	transitionImageLayout(outTex->Image, imageFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, outTex->mipLevels, 1);

	CopyBufferToImage(pixels, imageSize, outTex);
	stbi_image_free(pixels);

	generateMipmaps(outTex->Image, imageFormat, outTex->Width, outTex->Height, outTex->mipLevels, 1);
	createImageView(outTex->Image, imageFormat, VK_IMAGE_ASPECT_COLOR_BIT, outTex->mipLevels, VK_IMAGE_VIEW_TYPE_2D, 0, &outTex->View);

	GetTextureSampler(outTex->mipLevels, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, outTex->Sampler, true);

	outTex->Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
	outTex->freeFilename = freeFilename;
	outTex->filename = filename;
	outTex->textureIndex = -1;
	outTex->format = imageFormat;
	outTex->theoreticalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

Shader* VulkanBackend::NewShader_Separate(const char* zlsl, const char* pixelShader, bool freePixelShader, const char* vertexShader, bool freeVertexShader, VkRenderPass renderPass, int shaderType, VkExtent2D screenSize, VkCullModeFlags cullMode, VkPolygonMode polygonMode, VkSampleCountFlagBits sampleCount, BlendMode blendMode, uint32_t stencilWriteMask, VkCompareOp stencilCompareOp, uint32_t stencilTestValue, float depthBias, bool depthTest, bool depthWrite, bool masked)
{
	auto pipeline = &allShaders[numShaders++];
	check(pipeline, "Out of memory, can't make a new pipeline!");

	//std::cout << "New Shader: " << zlsl << "\n";

	VkPushConstantRange pushConstants{};
	pushConstants.offset = 0;

	uint32_t numAttachments;

	pipeline->setLayouts = GetSetLayoutsFromZLSL(zlsl, &numAttachments);

	createGraphicsPipeline(vertexShader, pixelShader, renderPass, pipeline->setLayouts.data(), pipeline->setLayouts.size(), shaderType, screenSize, cullMode, polygonMode, sampleCount, blendMode, depthTest, depthWrite, &pushConstants, 0, numAttachments, stencilWriteMask, stencilCompareOp, stencilTestValue, depthBias, &pipeline->pipelineLayout, &pipeline->pipeline);
	pipeline->zlslFile = zlsl;
	pipeline->vertexShader = vertexShader;
	pipeline->freeVertexShader = freeVertexShader;
	pipeline->pixelShader = pixelShader;
	pipeline->freePixelShader = freePixelShader;

	pipeline->mtime = FileDate(zlsl);
	pipeline->shaderType = shaderType;
	pipeline->cullMode = cullMode;
	pipeline->polygonMode = polygonMode;
	pipeline->sampleCount = sampleCount;
	pipeline->alphaBlend = blendMode;
	pipeline->depthTest = depthTest;
	pipeline->depthWrite = depthWrite;
	pipeline->renderPass = renderPass;
	pipeline->masked = masked;
	pipeline->pushConstantRange = pushConstants;
	pipeline->numAttachments = numAttachments;
	pipeline->stencilWriteMask = stencilWriteMask;
	pipeline->stencilCompareOp = stencilCompareOp;
	pipeline->stencilTestValue = stencilTestValue;
	pipeline->depthBias = depthBias;

	return pipeline;
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
		vkcheck(vkCreateCommandPool(logicalDevice, &poolInfo, VK_NULL_HANDLE, &sunThreadCommandPools[i]), "Failed to create opaque command pool!");
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
		vkcheck(CreateFrameBuffer(attachments.data(), attachments.size(), &postProcRenderPass, swapChainExtent, 1, &swapChainFramebuffers[i]), "failed to create framebuffer!");
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

void VulkanBackend::createDescriptorPool()
{
	std::array<VkDescriptorPoolSize, 6> poolSizes{};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	// We will allocate one of these descriptors for every frame.
	poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	// We will allocate one of these descriptors for every frame.
	poolSizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
	poolSizes[2].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[2].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
	poolSizes[3].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[3].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
	poolSizes[4].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[4].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
	poolSizes[5].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[5].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolInfo.pPoolSizes = poolSizes.data();

	// Aside from the maximum number of individual descriptors that are available, we also need to specify the maximum number of descriptor sets that may be allocated:
	poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT) + 32768;

	// The structure has an optional flag similar to command pools that determines if individual descriptor sets can be freed or not: VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT.
	// We're not going to touch the descriptor set after creating it, so we don't need this flag. You can leave flags to its default value of 0.

	if (vkCreateDescriptorPool(logicalDevice, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
		throw std::runtime_error("failed to create descriptor pool!");
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

void VulkanBackend::AllocateDescriptorSet(VkDescriptorSetLayout* pSetLayouts, uint32_t descriptorSetCount, VkDescriptorPool descriptorPool, VkDescriptorSet* out_DescriptorSets)
{
	VkDescriptorSetAllocateInfo allocateInfo{};
	allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocateInfo.pNext = VK_NULL_HANDLE;
	allocateInfo.descriptorSetCount = descriptorSetCount;
	allocateInfo.descriptorPool = descriptorPool;
	allocateInfo.pSetLayouts = pSetLayouts;

	vkAllocateDescriptorSets(logicalDevice, &allocateInfo, out_DescriptorSets);
}


void VulkanBackend::createTextureSampler(int mipLevels, VkFilter magFilter, VkFilter minFilter, VkSamplerAddressMode addressMode, VkSampler& outSampler)
{
	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = magFilter;
	samplerInfo.minFilter = minFilter;

	samplerInfo.addressModeU = samplerInfo.addressModeV = samplerInfo.addressModeW = addressMode;

	samplerInfo.anisotropyEnable = VK_TRUE;

	VkPhysicalDeviceProperties properties{};
	vkGetPhysicalDeviceProperties(physicalDevice, &properties);

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
	samplerInfo.maxLod = static_cast<float>(mipLevels);

	if (vkCreateSampler(logicalDevice, &samplerInfo, nullptr, &outSampler) != VK_SUCCESS)
		throw std::runtime_error("failed to create texture sampler!");
}

void VulkanBackend::GetTextureSampler(int mipLevels, VkFilter magFilter, VkFilter minFilter, VkSamplerAddressMode addressMode, VkSampler& outSampler, bool addToList = true)
{
	if (addToList)
	{
		for (size_t i = 0; i < allSamplers.size(); i++)
		{
			if (
				allSamplers[i]->magFilter == magFilter &&
				allSamplers[i]->minFilter == minFilter &&
				allSamplers[i]->addressMode == addressMode &&
				allSamplers[i]->mipLevels == mipLevels
				)
			{
				outSampler = allSamplers[i]->sampler;
				return;
			}
		}

		FullSampler* s = NEW(FullSampler);
		check(s, "Failed to allocate memory for full sampler!");
		s->addressMode = addressMode;
		s->magFilter = magFilter;
		s->minFilter = minFilter;
		s->mipLevels = mipLevels;
		createTextureSampler(mipLevels, magFilter, minFilter, addressMode, s->sampler);
		allSamplers.push_back(s);
		outSampler = s->sampler;
	}
	else
		createTextureSampler(mipLevels, magFilter, minFilter, addressMode, outSampler);
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
	allocateInfo.descriptorPool = descriptorPool;
	allocateInfo.descriptorSetCount = 1;
	allocateInfo.pSetLayouts = GetDescriptorSetLayout(1, 0, 1);
	allocateInfo.pNext = VK_NULL_HANDLE;

	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		vkAllocateDescriptorSets(logicalDevice, &allocateInfo, &uniformBufferDescriptorSets[i][0]);

	allocateInfo.pSetLayouts = GetDescriptorSetLayout(1, 0, 0);
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
		imageInfo.imageView = beegShadowMap->View;
		imageInfo.sampler = beegShadowMap->Sampler;

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


VkViewport lightMapViewport = { 0.0f, 0.0f, SHADOWMAPSIZE, SHADOWMAPSIZE, 0.0f, 1.0f };
VkRect2D lightMapScissor = { { 0, 0 }, { SHADOWMAPSIZE, SHADOWMAPSIZE } };
SunLight* lightMapSun;
uint32_t lightMapImageIndex;
VkDeviceSize lightMapOffset = 0;
VkCommandBufferBeginInfo* lightMapBeginInfo;
// These have to be marked as volatile or the while loop that waits for the threads to finish will be optimized out for who knows what reason
volatile bool sunThreadGos[NUMCASCADES];
volatile bool sunThreadDones[NUMCASCADES];
RenderStageShaderGroup shadowRenderStageOpaque;
RenderStageShaderGroup shadowRenderStageMasked;
VkRenderPass sunOpaquePass;
Camera* activeCamera;

#define RAYCASTMETHOD
static bool MeshGroupOnCascade(RenderStageMeshGroup* meshGroup, float distance)
{
#ifdef ENABLE_CULLING
	#ifdef RAYCASTMETHOD
		float3 dir = normalize(meshGroup->boundingBoxCentre - activeCamera->position);
		float3 coords;
		if (!HitBoundingBox(meshGroup->boundingBoxMin, meshGroup->boundingBoxMax, activeCamera->position, dir, coords))
			return true;

		return (glm::distance(coords, activeCamera->position) / 4) < distance;
	#else
		float3 points[8];
		points[0] = meshGroup->boundingBoxMax;
		points[1] = meshGroup->boundingBoxMin;

		points[2] = float3(meshGroup->boundingBoxMax.x, meshGroup->boundingBoxMax.y, meshGroup->boundingBoxMin.z);
		points[3] = float3(meshGroup->boundingBoxMax.x, meshGroup->boundingBoxMin.y, meshGroup->boundingBoxMin.z);
		points[4] = float3(meshGroup->boundingBoxMax.x, meshGroup->boundingBoxMin.y, meshGroup->boundingBoxMax.z);

		points[5] = float3(meshGroup->boundingBoxMin.x, meshGroup->boundingBoxMax.y, meshGroup->boundingBoxMin.z);
		points[6] = float3(meshGroup->boundingBoxMin.x, meshGroup->boundingBoxMax.y, meshGroup->boundingBoxMin.z);
		points[7] = float3(meshGroup->boundingBoxMin.x, meshGroup->boundingBoxMin.y, meshGroup->boundingBoxMax.z);

		float dist = glm::distance(points[0], activeCamera->position);
		for (uint32_t i = 1; i < 8; i++)
			dist = MIN(dist, glm::distance(points[i], activeCamera->position));

		return (dist / 4) <= distance;
	#endif
#else
	return true;
#endif
}


static bool RecordSunPassThread(SunPassThreadInfo* threadInfo)
{
	while (!sunThreadGos[threadInfo->cascade])
		return false;

	auto commandBuffer = lightMapSun->commandBuffers[lightMapImageIndex][threadInfo->cascade];

	vkBeginCommandBuffer(commandBuffer, lightMapBeginInfo);
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, &allVertexBuffer->buffer, &lightMapOffset);
	vkCmdBindIndexBuffer(commandBuffer, allIndexBuffer->buffer, 0, VK_INDEX_TYPE_UINT32);
	vkCmdSetViewport(commandBuffer, 0, 1, &lightMapViewport);
	vkCmdSetScissor(commandBuffer, 0, 1, &lightMapScissor);
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, threadInfo->opaqueShader->pipeline);

	threadInfo->passInfo.renderPass = sunOpaquePass;
	threadInfo->passInfo.renderArea = { 0, 0, SHADOWMAPSIZE, SHADOWMAPSIZE };

	threadInfo->passInfo.framebuffer = lightMapSun->frameBuffers[threadInfo->cascade];

	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, threadInfo->opaqueShader->pipelineLayout, 0, 1, &lightMapSun->descriptorSetVS[lightMapImageIndex][threadInfo->cascade], 0, VK_NULL_HANDLE);
	vkCmdBeginRenderPass(commandBuffer, &threadInfo->passInfo, VK_SUBPASS_CONTENTS_INLINE);

	for (auto materialGroup : shadowRenderStageOpaque.materialGroups)
	{
		for (auto meshGroup : materialGroup->meshGroups)
		{
			if (MeshGroupOnCascade(meshGroup, lightMapSun->cascadeDistances[threadInfo->cascade]))
			{
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, threadInfo->opaqueShader->pipelineLayout, 1, 1, &meshGroup->descriptorSet, 0, VK_NULL_HANDLE);
				vkCmdDrawIndexed(commandBuffer, meshGroup->mexel->IndexBufferLength, meshGroup->numInstances, meshGroup->mexel->startingIndex, meshGroup->mexel->startingVertex, 0);
			}
		}
	}

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, threadInfo->maskedShader->pipeline);

	for (auto materialGroup : shadowRenderStageMasked.materialGroups)
	{
		for (auto meshGroup : materialGroup->meshGroups)
		{
			if (MeshGroupOnCascade(meshGroup, lightMapSun->cascadeDistances[threadInfo->cascade]))
			{
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, threadInfo->maskedShader->pipelineLayout, 1, 1, &meshGroup->descriptorSet, 0, VK_NULL_HANDLE);
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, threadInfo->maskedShader->pipelineLayout, 2, 1, &materialGroup->material->descriptorSets[1], 0, VK_NULL_HANDLE);
				vkCmdDrawIndexed(commandBuffer, meshGroup->mexel->IndexBufferLength, meshGroup->numInstances, meshGroup->mexel->startingIndex, meshGroup->mexel->startingVertex, 0);
			}
		}
	}

	vkCmdEndRenderPass(commandBuffer);
	vkEndCommandBuffer(commandBuffer);

	sunThreadGos[threadInfo->cascade] = false;
	sunThreadDones[threadInfo->cascade] = true;
	return false;
}

static bool MeshGroupOnScreen(RenderStageMeshGroup* meshGroup, float3& camPos, float3& camDir)
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

bool SpotLightThreadProc(SpotLightThread* data)
{
	if (data->go)
	{
		VkCommandBuffer commandBuffer = data->light->commandBuffers[lightMapImageIndex];

		float3 position = float3(data->light->position);
		float3 dir = float3(data->light->dir);

		vkBeginCommandBuffer(commandBuffer, &data->backend->beginInfo);

		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &allVertexBuffer->buffer, &lightMapOffset);
		vkCmdBindIndexBuffer(commandBuffer, allIndexBuffer->buffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdSetViewport(commandBuffer, 0, 1, &lightMapViewport);
		vkCmdSetScissor(commandBuffer, 0, 1, &lightMapScissor);

		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, data->backend->lightShaderOpaqueStatic->pipeline);

		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, data->backend->lightShaderOpaqueStatic->pipelineLayout, 0, 1, &data->light->descriptorSetVS[lightMapImageIndex], 0, VK_NULL_HANDLE);
		vkCmdBeginRenderPass(commandBuffer, &data->passInfo, VK_SUBPASS_CONTENTS_INLINE);

		for (auto materialGroup : shadowRenderStageOpaque.materialGroups)
		{
			for (auto meshGroup : materialGroup->meshGroups)
			{
				if (!meshGroup->isStatic || MeshGroupOnScreen(meshGroup, position, dir))
				{
					vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, data->backend->lightShaderOpaqueStatic->pipelineLayout, 1, 1, &meshGroup->descriptorSet, 0, VK_NULL_HANDLE);
					vkCmdDrawIndexed(commandBuffer, meshGroup->mexel->IndexBufferLength, meshGroup->numInstances, meshGroup->mexel->startingIndex, meshGroup->mexel->startingVertex, 0);
				}
			}
		}

		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, data->backend->lightShaderMaskedStatic->pipeline);

		for (auto materialGroup : shadowRenderStageMasked.materialGroups)
		{
			for (auto meshGroup : materialGroup->meshGroups)
			{
				if (MeshGroupOnScreen(meshGroup, position, dir))
				{
					vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, data->backend->lightShaderMaskedStatic->pipelineLayout, 1, 1, &meshGroup->descriptorSet, 0, VK_NULL_HANDLE);
					vkCmdDrawIndexed(commandBuffer, meshGroup->mexel->IndexBufferLength, meshGroup->numInstances, meshGroup->mexel->startingIndex, meshGroup->mexel->startingVertex, 0);
				}
			}
		}

		vkCmdEndRenderPass(commandBuffer);
		vkEndCommandBuffer(commandBuffer);
		data->done = true;
		data->go = false;
	}

	return false;
}

VulkanBackend::VulkanBackend(GLFWwindow* glWindow, void (*drawGUIFunc)(VkCommandBuffer))
{
	physicalDevice = VK_NULL_HANDLE;
	queuePriority = 1.0f;
	currentFrame = 0;
	gpuTime = 0.f;
	this->glWindow = glWindow;
	drawGUI = drawGUIFunc;

	numShaders = 0;
	numSetLayouts = 0;
	numSpotLights = 0;
	numTextures = 0;

	cullThreshold = 0.0;
	theSun = NULL;
	beegShadowMap = NULL;
	setup = false;

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

	VkPhysicalDeviceFeatures supportedFeatures{};
	vkGetPhysicalDeviceFeatures(physicalDevice, &supportedFeatures);
	check(supportedFeatures.samplerAnisotropy, "This GPU does not support sampler anisotropy!");
	VkPhysicalDeviceFeatures deviceFeatures{};
	deviceFeatures.samplerAnisotropy = VK_TRUE;

	check(supportedFeatures.independentBlend, "This GPU does not support independent blend, and thus multiple render targets");
	deviceFeatures.independentBlend = VK_TRUE;

	//check(supportedFeatures.sampleRateShading, "This GPU does not support sampleRateShading for MSAA");
	//deviceFeatures.sampleRateShading = VK_TRUE;

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

	lightShaderOpaqueStatic = NewShader_Separate("shaders/core-light-static.zlsl", NULL, false, "shaders/core-light-static_vert.spv", false, lightRenderPass, SF_SHADOW, swapChainExtent, VK_CULL_MODE_NONE, VK_POLYGON_MODE_FILL, VK_SAMPLE_COUNT_1_BIT, BM_OPAQUE, 0, VK_COMPARE_OP_EQUAL, 0, 0.0f, true, true, false);
	lightShaderMaskedStatic = NewShader_Separate("shaders/core-light-masked-static.zlsl", "shaders/core-light-masked-static_pixl.spv", false, "shaders/core-light-masked-static_vert.spv", false, lightRenderPass, SF_SHADOW, swapChainExtent, VK_CULL_MODE_NONE, VK_POLYGON_MODE_FILL, VK_SAMPLE_COUNT_1_BIT, BM_OPAQUE, 0, VK_COMPARE_OP_EQUAL, 0, 0.0f, true, true, true);

	createCommandPool();
	CreatePostProcessingRenderPass();
	createFrameBuffers();
	CreateMainFrameBuffer();

	UI2DPipeline = NewShader_Separate("shaders/core-debug2d.zlsl", "shaders/core-debug2d_pixl.spv", false, "shaders/core-debug2d_vert.spv", false, mainRenderPass, SF_DEFAULT, renderExtent, VK_CULL_MODE_NONE, VK_POLYGON_MODE_FILL, msaaSamples, BM_OPAQUE, 0, VK_COMPARE_OP_ALWAYS, 0, 0.0f, false, false, false);
	UI3DPipeline = NewShader_Separate("shaders/core-debug3d.zlsl", "shaders/core-debug3d_pixl.spv", false, "shaders/core-debug3d_vert.spv", false, mainRenderPass, SF_DEFAULT, renderExtent, VK_CULL_MODE_NONE, VK_POLYGON_MODE_FILL, msaaSamples, BM_OPAQUE, 0, VK_COMPARE_OP_ALWAYS, 0, 0.0f, false, false, false);

	depthPrepassStaticShader = NewShader_Separate("shaders/core-light-static.zlsl", NULL, false, "shaders/core-light-static_vert.spv", false, depthPrepassRenderPass, SF_SHADOW, swapChainExtent, VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_FILL, msaaSamples, BM_OPAQUE, 0, VK_COMPARE_OP_EQUAL, 0, DEPTH_PREPASS_BIAS, true, true, false);

	createUniformBuffers();
	createDescriptorPool();

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

	for (uint32_t i = 0; i < NUMCASCADES; i++)
	{
		sunThreadGos[i] = false;
		sunThreadDones[i] = false;
		sunThreadInfos[i] = { i, renderPassInfo, lightShaderOpaqueStatic, lightShaderMaskedStatic };
		sunThreads[i] = new Thread((zThreadFunc)RecordSunPassThread, (void*)&sunThreadInfos[i]);
	}

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

	FullCreateImage(VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_2D, storageFormat, swapChainExtent.width, swapChainExtent.height, 1, 1, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, &RTTexture, true);
	transitionImageLayout(RTTexture.Image, storageFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1);

	lightMapBeginInfo = &beginInfo;

	sunOpaquePass = lightRenderPass;

	depthPrepassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	depthPrepassBeginInfo.framebuffer = depthPrepassFrameBuffer;
	depthPrepassBeginInfo.renderPass = depthPrepassRenderPass;
	depthPrepassBeginInfo.clearValueCount = 1;
	depthPrepassBeginInfo.pClearValues = &clearValues[1];
	depthPrepassBeginInfo.renderArea = { { 0, 0 }, renderExtent };
	depthPrepassBeginInfo.pNext = VK_NULL_HANDLE;

	GUIBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	GUIBeginInfo.renderPass = guiRenderPass;
	GUIBeginInfo.clearValueCount = 0;
	GUIBeginInfo.renderArea = { { 0, 0 }, swapChainExtent };
	GUIBeginInfo.pNext = VK_NULL_HANDLE;

	vkDeviceWaitIdle(logicalDevice);
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

// It's more like CreateMesh, it does the vertex and index buffer
Mexel VulkanBackend::createVertexBuffer(void* vertices, size_t numVerts, void* indices, size_t numIndices, size_t indexSize)
{
	Mexel mesh{};

	mesh.startingVertex = AddVertexBuffer((Vertex*)vertices, numVerts);
	if (indexSize == 2)
		mesh.startingIndex = AddIndexBuffer16((uint16_t*)indices, numIndices);
	else
		mesh.startingIndex = AddIndexBuffer32((uint32_t*)indices, numIndices);

	mesh.IndexBufferLength = numIndices;

	//CreateStaticBuffer(indices, numIndices * indexSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, mesh.IndexBuffer, mesh.IndexBufferMemory);

	return mesh;
}

// Helper function to copy arbitrary data to a texture, which first involves copying to a staging buffer, and then to the texture.
void VulkanBackend::CopyBufferToImage(void* src, VkDeviceSize imageSize, Texture* dst)
{
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

	void* data;
	vkMapMemory(logicalDevice, stagingBufferMemory, 0, imageSize, 0, &data);
	memcpy(data, src, static_cast<size_t>(imageSize));
	vkUnmapMemory(logicalDevice, stagingBufferMemory);

	RecordBufferForCopyingToImage(stagingBuffer, dst->Image, static_cast<uint32_t>(dst->Width), static_cast<uint32_t>(dst->Height), 1);

	vkDestroyBuffer(logicalDevice, stagingBuffer, nullptr);
	vkFreeMemory(logicalDevice, stagingBufferMemory, nullptr);
}

void VulkanBackend::RecordBufferForCopyingToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, uint32_t layerCount)
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
		1
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
	sunShadowPassShader = NewShader_Separate("shaders/shadowPass-sun.zlsl", "shaders/shadowPass-sun_pixl.spv", false, "shaders/post_vert.spv", false, sunShadowPassRenderPass, SF_SUNSHADOWPASS, swapChainExtent, VK_CULL_MODE_NONE, VK_POLYGON_MODE_FILL, VK_SAMPLE_COUNT_1_BIT, BM_OPAQUE, 0, VK_COMPARE_OP_EQUAL, 0, 0.0f, false, false, false);
	spotShadowPassShader = NewShader_Separate("shaders/shadowPass-spot.zlsl", "shaders/shadowPass-spot_pixl.spv", false, "shaders/post_vert.spv", false, spotShadowPassRenderPass, SF_SPOTSHADOWPASS, swapChainExtent, VK_CULL_MODE_NONE, VK_POLYGON_MODE_FILL, VK_SAMPLE_COUNT_1_BIT, BM_MAX, 0, VK_COMPARE_OP_EQUAL, 0, 0.0f, false, false, false);
}

void VulkanBackend::DestroyTexture(Texture* ptr)
{
	vkDestroyImage(logicalDevice, ptr->Image, nullptr);
	if (ptr->View)
		vkDestroyImageView(logicalDevice, ptr->View, nullptr);

	vkFreeMemory(logicalDevice, ptr->Memory, nullptr);
	if (ptr->freeFilename)
		free((void*)ptr->filename);
}

void VulkanBackend::cleanupSwapChain()
{
	for (size_t i = 0; i < swapChainFramebuffers.size(); i++)
		vkDestroyFramebuffer(logicalDevice, swapChainFramebuffers[i], nullptr);

	for (size_t i = 0; i < swapChainImageViews.size(); i++)
		vkDestroyImageView(logicalDevice, swapChainImageViews[i], nullptr);

	vkDestroySwapchainKHR(logicalDevice, swapChain, nullptr);
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

	DestroyTexture(&mainRenderTarget_C);
	DestroyTexture(&mainRenderTarget_D);
	DestroyTexture(&mainRenderTarget_N);
	DestroyTexture(&mainRenderTarget_P);
	DestroyTexture(&mainRenderTarget_G);
	DestroyTexture(&RTTexture);
	DestroyTexture(beegShadowMap);
	vkDestroySampler(logicalDevice, mainRenderTarget_C.Sampler, VK_NULL_HANDLE);
	vkDestroySampler(logicalDevice, mainRenderTarget_D.Sampler, VK_NULL_HANDLE);
	vkDestroySampler(logicalDevice, mainRenderTarget_N.Sampler, VK_NULL_HANDLE);
	vkDestroySampler(logicalDevice, mainRenderTarget_P.Sampler, VK_NULL_HANDLE);
	vkDestroySampler(logicalDevice, mainRenderTarget_G.Sampler, VK_NULL_HANDLE);
	vkDestroyFramebuffer(logicalDevice, mainFrameBuffer, VK_NULL_HANDLE);
	vkDestroyFramebuffer(logicalDevice, depthPrepassFrameBuffer, VK_NULL_HANDLE);

	for (const auto& set : UI3DDescriptorSets)
		delete set.buffer;

	for (const auto& set : UI2DDescriptorSets)
		delete set.buffer;

	vkDestroyCommandPool(logicalDevice, commandPool, VK_NULL_HANDLE);
	for (uint32_t i = 0; i < NUMCASCADES; i++)
		vkDestroyCommandPool(logicalDevice, sunThreadCommandPools[i], VK_NULL_HANDLE);

	cleanupSwapChain();

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		vkDestroyBuffer(logicalDevice, uniformBuffers[i], nullptr);
		vkFreeMemory(logicalDevice, uniformBuffersMemory[i], nullptr);
		vkDestroyBuffer(logicalDevice, psBuffers[i], nullptr);
		vkFreeMemory(logicalDevice, psBuffersMemory[i], nullptr);
	}

	delete allIndexBuffer;
	delete allVertexBuffer;

	vkDestroyRenderPass(logicalDevice, postProcRenderPass, VK_NULL_HANDLE);
	vkDestroyRenderPass(logicalDevice, mainRenderPass, VK_NULL_HANDLE);
	vkDestroyRenderPass(logicalDevice, depthPrepassRenderPass, VK_NULL_HANDLE);
	vkDestroyRenderPass(logicalDevice, guiRenderPass, VK_NULL_HANDLE);
	vkDestroyRenderPass(logicalDevice, lightRenderPass, VK_NULL_HANDLE);

	vkDestroyDescriptorPool(logicalDevice, descriptorPool, nullptr);

	vkDestroyDevice(logicalDevice, nullptr);
	vkDestroySurfaceKHR(instance, surface, nullptr);
	vkDestroyInstance(instance, nullptr);
}

uint32_t VulkanBackend::GetGraphicsFamily()
{
	QueueFamilyIndices q = findQueueFamilies(physicalDevice);
	return q.graphicsFamily;
}

size_t VulkanBackend::AddIndexBuffer16(uint16_t* indices, size_t numIndices)
{
	size_t startingIndex = allIndices.size();
	allIndices.resize(startingIndex + numIndices);

	for (size_t i = 0; i < numIndices; i++)
		allIndices[i + startingIndex] = (uint32_t)indices[i];

	return startingIndex;
}

size_t VulkanBackend::AddIndexBuffer32(uint32_t* indices, size_t numIndices)
{
	size_t startingIndex = allIndices.size();
	allIndices.resize(startingIndex + numIndices);

	memcpy(&allIndices[startingIndex], indices, numIndices * sizeof(uint32_t));

	return startingIndex;
}

// Returns the starting index into the vertex buffer
size_t VulkanBackend::AddVertexBuffer(Vertex* vertices, size_t numVerts)
{
	size_t oldSize = allVertices.size();
	allVertices.resize(oldSize + numVerts);

	VkDeviceSize bufferSize = numVerts * sizeof(Vertex);

	memcpy(&allVertices[oldSize], vertices, bufferSize);

	return oldSize;
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

void VulkanBackend::CreateAllVertexBuffer()
{
	if (allVertexBuffer) delete allVertexBuffer;

	allVertexBuffer = new VulkanMemory(this, allVertices.size() * sizeof(Vertex), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, "VertexBuffer", true, allVertices.data());
}

void VulkanBackend::CreateAllIndexBuffer()
{
	if (allIndexBuffer) delete allIndexBuffer;

	allIndexBuffer = new VulkanMemory(this, allIndices.size() * sizeof(uint32_t), VK_BUFFER_USAGE_INDEX_BUFFER_BIT, "IndexBuffer", true, allIndices.data());
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
	int width = 0, height = 0;
	while (width == 0 || height == 0)
	{
		glfwGetFramebufferSize(glWindow, &width, &height);
		glfwWaitEvents();
	}

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

void VulkanBackend::DrawMexels(VkCommandBuffer commandBuffer, Mesh* mesh)
{
	for (auto mexel : mesh->mexels)
		vkCmdDrawIndexed(commandBuffer, mexel->IndexBufferLength, 1, mexel->startingIndex, mexel->startingVertex, 0);
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
	if (material->shader->alphaBlend)
		AddMexelToRenderStage(&mainRenderStageTransparency, thing, mexel, material);
	else
	{
		AddMexelToRenderStage(&mainRenderStage, thing, mexel, material);

		if (thing->castShadow)
		{
			if (material->masked)
				AddThingToShaderGroup(&shadowRenderStageMasked, thing, mexel, material);
			else
				AddThingToShaderGroup(&shadowRenderStageOpaque, thing, mexel, material);
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
		if (thing->shadowMap->Width == ref->size)
		{
			ref->objects.push_back(thing);
			return;
		}
	}

	auto newRef = new ShadowMapRef();
	newRef->size = thing->shadowMap->Width;
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

void VulkanBackend::AddThingToExistingBeegShadowMap(Thing* thing)
{
	uint32_t size = thing->shadowMap->Width;
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

	Rect oldImageArea = { 0, 0, beegShadowMap->Width, beegShadowMap->Height };

	Texture* oldImage = NULL;

	auto commandBuffer = beginSingleTimeCommands();
	if (resize)
	{
		oldImage = beegShadowMap;
		beegShadowMap = NEW(Texture);
		FullCreateImage(VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_2D, BEEG_SHADOWMAP_FORMAT, beegShadowMapSize, beegShadowMapSize, 1, 1, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_IMAGE_ASPECT_COLOR_BIT, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, beegShadowMap, true);
		BlitImage(commandBuffer, oldImage, oldImageArea, beegShadowMap, oldImageArea, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_FILTER_LINEAR, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	}
	BlitImage(commandBuffer, thing->shadowMap, srcArea, beegShadowMap, dstArea, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_FILTER_LINEAR, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	endSingleTimeCommands(commandBuffer);

	if (oldImage)
	{
		DestroyTexture(oldImage);
		free(oldImage);
	}
}



void VulkanBackend::SortAndMakeBeegShadowMap()
{
	std::cout << "Sorting Shadow Map Sizes...\n";

	bool sorted;
	uint32_t len = shadowMapRefs.size() - 1;
	ShadowMapRef* temp;
	do
	{
		sorted = true;

		for (uint32_t i = 0; i < len; i++)
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

	beegShadowMap = NEW(Texture);
	FullCreateImage(VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R8G8B8A8_SRGB, beegShadowMapSize, beegShadowMapSize, 1, 1, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_IMAGE_ASPECT_COLOR_BIT, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, beegShadowMap, true);

	auto commandBuffer = beginSingleTimeCommands();
	Rect srcArea, dstArea;
	VkImageLayout dstLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	for (const auto& spot : beegShadowMapSpots)
	{
		srcArea = { 0, 0, spot.size, spot.size };
		dstArea = { spot.x, spot.y, spot.size, spot.size };
		BlitImage(commandBuffer, spot.object->shadowMap, srcArea, beegShadowMap, dstArea, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_FILTER_LINEAR, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0, 0, 0, dstLayout);
		dstLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}
	endSingleTimeCommands(commandBuffer);
	vkDeviceWaitIdle(logicalDevice);

	std::cout << "Deleting old shadow maps\n";

	for (const auto& spot : beegShadowMapSpots)
	{
		if (spot.object->shadowMap)
		{
			DestroyTexture(spot.object->shadowMap);
			free(spot.object->shadowMap);
			spot.object->shadowMap = NULL;
		}
	}

	std::cout << "Done!\n";
}

void VulkanBackend::UpdateMeshGroupBufferDescriptorSet(RenderStageMeshGroup* meshGroup)
{
	VkWriteDescriptorSet write[2];

	for (uint32_t i = 0; i < 2; i++)
	{
		write[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write[i].descriptorCount = 1;
		write[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		write[i].dstBinding = i;
		write[i].dstArrayElement = 0;
		write[i].dstSet = meshGroup->descriptorSet;
		write[i].pNext = VK_NULL_HANDLE;
	}

	VkDescriptorBufferInfo bufferInfo = meshGroup->matrixMem->GetBufferInfo();
	write[0].pBufferInfo = &bufferInfo;

	VkDescriptorBufferInfo bufferInfo2 = meshGroup->shadowMapOffsetsMem->GetBufferInfo();
	write[1].pBufferInfo = &bufferInfo2;

	vkUpdateDescriptorSets(logicalDevice, 2, write, 0, VK_NULL_HANDLE);
}

void VulkanBackend::AllocateMeshGroupBuffers(RenderStageMeshGroup* meshGroup)
{
	if (meshGroup->matrixMem) delete meshGroup->matrixMem;
	if (meshGroup->shadowMapOffsetsMem) delete meshGroup->shadowMapOffsetsMem;

	meshGroup->matrixMem = new VulkanMemory(this, meshGroup->matrices.size() * sizeof(float4x4), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, "MeshGroupMatrices", meshGroup->isStatic, meshGroup->matrices.data());
	meshGroup->shadowMapOffsetsMem = new VulkanMemory(this, meshGroup->shadowMapOffsets.size() * sizeof(float4), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, "MeshGroupShadowOffsets", true, meshGroup->shadowMapOffsets.data());
}

void VulkanBackend::SetupMeshGroup(RenderStageMeshGroup* meshGroup)
{
	AllocateMeshGroupBuffers(meshGroup);
	meshGroup->boundingBoxCentre = ((meshGroup->boundingBoxMax - meshGroup->boundingBoxMin) * float3(0.5)) + meshGroup->boundingBoxMin;
	AllocateDescriptorSets(1, GetDescriptorSetLayout(0, 0, 0, 2), &meshGroup->descriptorSet);
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

void DestroyPipelineGroup(RenderStageShaderGroup* pipelineGroup)
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

void DestroyRenderStage(RenderStage* renderStage)
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
	DestroyPipelineGroup(&shadowRenderStageOpaque);
	DestroyPipelineGroup(&shadowRenderStageMasked);
	if (beegShadowMap)
	{
		DestroyTexture(beegShadowMap);
		free(beegShadowMap);
		beegShadowMap = NULL;
	}


	mainRenderStage.shaderGroups = {};
	mainRenderStageTransparency.shaderGroups = {};

	//computeObjects = {};
	shadowMapRefs.clear();

	for (size_t i = 0; i < allThingsLen; i++)
	{
		//if (allThings[i]->isStatic)
		AddThingToBeegShadowMap(allThings[i]);
	}

	SortAndMakeBeegShadowMap();

	std::cout << "Setting up render stage...\n";

	for (size_t i = 0; i < allThingsLen; i++)
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

	SetupPipelineGroup(&shadowRenderStageOpaque);
	SetupPipelineGroup(&shadowRenderStageMasked);

	std::cout << "Done!\n";

	//computeObjectsMem = new VulkanMemory(this, computeObjects.size() * sizeof(ComputeObject), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, true, computeObjects.data());

	setup = true;
}

void VulkanBackend::DrawRenderStage(VkCommandBuffer commandBuffer, VkCommandBuffer prepassCommandBuffer, RenderStage* renderStage, VkDescriptorSet* uniformBufferDescriptorSet)
{
	size_t len = renderStage->shaderGroups.size();
	RenderStageShaderGroup* pipelineGroup;
	RenderStageMaterialGroup* materialGroup;
	RenderStageMeshGroup* meshGroup;
	Shader* pipeline;

	bool boundMaterial;
	float3 camDir = glm::normalize(activeCamera->target - activeCamera->position);

	// Objects are grouped by the mesh they use, those groups are grouped by the textures they use, and those groups are grouped by the pipeline (shader) they use.
	// It's quite convoluted but it should mean that it only binds things when absolutely necessary, without having to check anything
	for (size_t i = 0; i < len; i++)
	{
		pipelineGroup = &renderStage->shaderGroups[i];

		pipeline = renderStage->shader ? renderStage->shader : pipelineGroup->shader;

		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipelineLayout, 0, 1, uniformBufferDescriptorSet, 0, nullptr);

		for (uint32_t m = 0; m < pipelineGroup->materialGroups.size(); m++)
		{
			materialGroup = pipelineGroup->materialGroups[m];

			boundMaterial = false;

			for (uint32_t j = 0; j < materialGroup->meshGroups.size(); j++)
			{
				meshGroup = materialGroup->meshGroups[j];

				if (!MeshGroupOnScreen(meshGroup, activeCamera->position, camDir))
					continue;

				if (!boundMaterial && materialGroup->material)
				{
					boundMaterial = true;
					vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipelineLayout, 2, 1, &materialGroup->material->descriptorSets[0], 0, nullptr);
				}

				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipelineLayout, 1, 1, &meshGroup->descriptorSet, 0, nullptr);
				vkCmdDrawIndexed(commandBuffer, meshGroup->mexel->IndexBufferLength, meshGroup->numInstances, meshGroup->mexel->startingIndex, meshGroup->mexel->startingVertex, 0);

				if (prepassCommandBuffer && !materialGroup->material->masked && !materialGroup->material->shader->alphaBlend)
				{
					vkCmdBindDescriptorSets(prepassCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, depthPrepassStaticShader->pipelineLayout, 1, 1, &meshGroup->descriptorSet, 0, nullptr);
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

		b.buffer = new VulkanMemory(this, sizeof(float4x4) + sizeof(float4), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, "DebugPoints", false, NULL);
		AllocateDescriptorSet(GetDescriptorSetLayout(1, 0, 1, 0), 1, descriptorPool, &b.descriptorSet);

		VkDescriptorBufferInfo bufferInfo{};
		VkDescriptorImageInfo imageInfo{};

		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(float4x4) + sizeof(float4);

		imageInfo.sampler = instance->texture->Sampler;
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = instance->texture->View;

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
		vkFreeDescriptorSets(logicalDevice, descriptorPool, 1, &descriptorSetList.back().descriptorSet);
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

	lightMapImageIndex = imageIndex;

	if (theSun)
	{
		lightMapSun = theSun;

		for (uint32_t i = 0; i < NUMCASCADES; i++)
		{
			sunThreadDones[i] = false;
			sunThreadGos[i] = true;
		}
	}

	for (size_t i = 0; i < numSpotLights; i++)
	{
		allSpotLights[i]->thread.done = false;
		allSpotLights[i]->thread.go = true;
	}

	static VkViewport passViewport = { 0, 0, renderExtent.width, renderExtent.height, 0.0f, 1.0f };
	VkCommandBuffer commandBuffer = commandBuffers[imageIndex];
	VkCommandBuffer depthCommands = commandBuffers_DepthPrepass[imageIndex];

	vkResetCommandBuffer(commandBuffer, 0);
	vkBeginCommandBuffer(commandBuffer, &beginInfo);
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, &allVertexBuffer->buffer, offsets);
	vkCmdBindIndexBuffer(commandBuffer, allIndexBuffer->buffer, 0, VK_INDEX_TYPE_UINT32);
	vkCmdSetViewport(commandBuffer, 0, 1, &passViewport);
	vkCmdSetScissor(commandBuffer, 0, 1, &renderPassInfo.renderArea);
	vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

	vkResetCommandBuffer(depthCommands, 0);
	vkBeginCommandBuffer(depthCommands, &beginInfo);
	vkCmdBindVertexBuffers(depthCommands, 0, 1, &allVertexBuffer->buffer, offsets);
	vkCmdBindIndexBuffer(depthCommands, allIndexBuffer->buffer, 0, VK_INDEX_TYPE_UINT32);
	vkCmdSetViewport(depthCommands, 0, 1, &passViewport);
	vkCmdSetScissor(depthCommands, 0, 1, &renderPassInfo.renderArea);
	vkCmdBindPipeline(depthCommands, VK_PIPELINE_BIND_POINT_GRAPHICS, depthPrepassStaticShader->pipeline);
	vkCmdBindDescriptorSets(depthCommands, VK_PIPELINE_BIND_POINT_GRAPHICS, depthPrepassStaticShader->pipelineLayout, 0, 1, &uniformBufferDescriptorSets[imageIndex][1], 0, nullptr);
	vkCmdBeginRenderPass(depthCommands, &depthPrepassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	DrawRenderStage(commandBuffer, depthCommands, &mainRenderStage, &uniformBufferDescriptorSets[imageIndex][0]);
	DrawRenderStage(commandBuffer, depthCommands, &mainRenderStageTransparency, &uniformBufferDescriptorSets[imageIndex][0]);

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
	{
		for (uint32_t i = 0; i < NUMCASCADES; i++)
			while (!sunThreadDones[i]);
	}

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
					thing->matrixIndices.push_back(group->matrices.size());

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

	uint32_t numPasses = Lua_Len(L, -1);
	for (uint32_t i = 0; i < numPasses; i++)
	{
		lua_geti(L, -1, i + 1);

		renderStages.push_back({});

		renderStages.back().stageType = (RenderStageType)IntFromTable(L, -1, 1, "passType");

		if (renderStages.back().stageType == RST_BLIT)
		{
			lua_geti(L, -1, 2);
			lua_getfield(L, -1, "texture");
			Texture* srcTexture = (Texture*)lua_touserdata(L, -1);
			Texture* dstTexture;
			renderStages.back().srcImage = srcTexture;
			lua_pop(L, 1);

			lua_getfield(L, -1, "width");
			renderStages.back().srcX = lua_tointeger(L, -1);
			lua_pop(L, 1);

			lua_getfield(L, -1, "height");
			renderStages.back().srcY = lua_tointeger(L, -1);
			lua_pop(L, 1);

			lua_getfield(L, -1, "aspect");
			renderStages.back().srcAspect = (VkImageAspectFlags)lua_tointeger(L, -1);
			lua_pop(L, 1);
			lua_pop(L, 1);


			lua_geti(L, -1, 3);
			lua_getfield(L, -1, "texture");
			dstTexture = (Texture*)lua_touserdata(L, -1);
			renderStages.back().dstImage = dstTexture;
			lua_pop(L, 1);

			lua_getfield(L, -1, "width");
			renderStages.back().dstX = lua_tointeger(L, -1);
			lua_pop(L, 1);

			lua_getfield(L, -1, "height");
			renderStages.back().dstY = lua_tointeger(L, -1);
			lua_pop(L, 1);

			lua_getfield(L, -1, "aspect");
			renderStages.back().dstAspect = (VkImageAspectFlags)lua_tointeger(L, -1);
			lua_pop(L, 1);
			lua_pop(L, 1);

			renderStages.back().blitFilter = (VkFilter)IntFromTable(L, -1, 4, "blitFilter");

			renderStages.back().transitionSrc = (VkImageLayout)IntFromTable_Default(L, -1, 5, 0);
			renderStages.back().transitionDst = (VkImageLayout)IntFromTable_Default(L, -1, 6, 0);

			renderStages.back().srcLayout = (VkImageLayout)IntFromTable(L, -1, 7, "srcLayout");

			if (srcTexture->theoreticalLayout != renderStages.back().srcLayout)
			{
				PrintF("Pass (%i): Source texture (%s) is not in the expected layout (%s)!\n", i, string_VkImageLayout(srcTexture->theoreticalLayout), string_VkImageLayout(renderStages.back().srcLayout));
				throw std::runtime_error("Error reading Blit Pass");
			}

			srcTexture->theoreticalLayout = renderStages.back().transitionSrc ? renderStages.back().transitionSrc : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			dstTexture->theoreticalLayout = renderStages.back().transitionDst ? renderStages.back().transitionDst : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

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
			int numTextures = Lua_Len(L, -1);
			for (int j = 0; j < numTextures; j++)
			{
				lua_geti(L, -1, j + 1);
				auto tex = (Texture*)lua_touserdata(L, -1);
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
			renderStages.back().extent.width = lua_tonumber(L, -1);
			lua_pop(L, 1);

			lua_getfield(L, -1, "height");
			renderStages.back().extent.height = lua_tonumber(L, -1);
			lua_pop(L, 1);
		}
		lua_pop(L, 1);

		renderStages.back().clearValues = {};
		lua_geti(L, -1, 4);
		int numClearValues = Lua_Len(L, -1);
		for (int j = 0; j < numClearValues; j++)
		{
			lua_geti(L, -1, j + 1);
			int numValues = Lua_Len(L, -1);
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
			renderStages.back().shader = NewShader_Separate(StringFromTable(-1, 1), StringFromTable(-1, 2), false, StringFromTable(-1, 3), false, renderStages.back().renderPass, IntFromTable(L, -1, 4, "shaderType"), *(VkExtent2D*)UDataFromTable(-1, 5), (VkCullModeFlagBits)IntFromTable(L, -1, 6, "Pipeline CullMode"), VK_POLYGON_MODE_FILL, (VkSampleCountFlagBits)IntFromTable(L, -1, 7, "SampleCount"), (BlendMode)IntFromTable(L, -1, 8, "BlendMode"), 0, compareOp, stencilTestValue, FloatFromTable(-1, 10), BoolFromTable(-1, 11), BoolFromTable(-1, 12), false);
		}
		else
			renderStages.back().shader = nullptr;
		lua_pop(L, 1);

		lua_geti(L, -1, 6);
		int idsLength = Lua_Len(L, -1);
		renderStages.back().meshIDs = {};
		for (int i = 0; i < idsLength; i++)
			renderStages.back().meshIDs.push_back(IntFromTable(L, -1, i + 1, "meshID"));
		lua_pop(L, 1);

		renderStages.back().descriptorSet.resize(MAX_FRAMES_IN_FLIGHT);

		VkDescriptorSetAllocateInfo allocateInfo{};
		allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocateInfo.descriptorPool = descriptorPool;
		allocateInfo.descriptorSetCount = 1;
		allocateInfo.pNext = nullptr;

		if (renderStages.back().stageType == RST_SHADOW)
			allocateInfo.pSetLayouts = GetDescriptorSetLayout(0, 1, 2);
		else
			allocateInfo.pSetLayouts = renderStages.back().shader ? &renderStages.back().shader->setLayouts[0] : GetDescriptorSetLayout(1, 1, 6);

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

			int numWrites = Lua_Len(L, writesDex);
			int imagesWritten = 0;
			int buffersWritten = 0;
			imageInfos.resize(numWrites);
			for (int j = 0; j < numWrites; j++)
			{
				lua_geti(L, writesDex, j + 1);
				int writeDex = lua_gettop(L);

				writes.push_back({});

				VkDescriptorType descriptorType = (VkDescriptorType)IntFromTable(L, -1, 1, "descriptorType");
				imageInfos[imagesWritten].imageLayout = (VkImageLayout)IntFromTable(L, -1, 3, "imageLayout");
				lua_geti(L, -1, 2);
					lua_getfield(L, -1, "texture");
						Texture* tex = (Texture*)lua_touserdata(L, -1);
				lua_pop(L, 2);
				assert(tex->View);
				imageInfos[imagesWritten].imageView = tex->View;
				assert(tex->Sampler);
				imageInfos[imagesWritten].sampler = tex->Sampler;

				writes.back().sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				writes.back().dstBinding = j + 1;
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

			vkUpdateDescriptorSets(logicalDevice, writes.size(), writes.data(), 0, NULL);
		}

		lua_pop(L, 1);
	}
	lua_pop(L, 1);
}



void VulkanBackend::DestroyRenderStages()
{
	size_t len = renderStages.size();

	for (size_t i = 0; i < len; i++)
		DestroyRenderStage(&renderStages[i]);

	DestroyRenderStage(&mainRenderStage);
	DestroyRenderStage(&mainRenderStageTransparency);
	DestroyPipelineGroup(&shadowRenderStageOpaque);
	DestroyPipelineGroup(&shadowRenderStageMasked);
}

struct StaticMesh
{
	float4x4 matrix;
	Mesh* mesh;
};
std::vector<StaticMesh> staticMeshes;

Mesh* VulkanBackend::MakeStaticMesh(float4x4& matrix, Mesh* sourceMesh)
{
	if (matrix == glm::mat4())
		return sourceMesh;

	for (const auto& staticMesh : staticMeshes)
	{
		if (staticMesh.matrix == matrix)
			return staticMesh.mesh;
	}

	allMeshes.push_back(NEW(Mesh));
	ZEROMEM(allMeshes.back(), sizeof(Mesh));
	allMeshes.back()->mexels.resize(sourceMesh->mexels.size());

	for (size_t i = 0; i < sourceMesh->mexels.size(); i++)
		allMeshes.back()->mexels[i] = MakeStaticMexel(matrix, sourceMesh->mexels[i]);

	return allMeshes.back();
}

struct StaticMexel
{
	float4x4 matrix;
	Mexel* mexel;
};
std::vector<StaticMexel> staticMexels;

struct SeenIndex
{
	uint32_t oldIndex;
	uint32_t newIndex;
};

Mexel* VulkanBackend::MakeStaticMexel(float4x4& matrix, Mexel* sourceMexel)
{
	// If the matrix does nothing, no need to make a whole new mexel
	if (matrix == glm::mat4())
		return sourceMexel;

	for (const auto& staticMesh : staticMexels)
	{
		if (staticMesh.matrix == matrix)
			return staticMesh.mexel;
	}

	allMexels.push_back(NEW(Mexel));
	allMexels.back()->Filename = sourceMexel->Filename;
	allMexels.back()->startingIndex = allIndices.size();
	allMexels.back()->startingVertex = allVertices.size();
	allMexels.back()->IndexBufferLength = sourceMexel->IndexBufferLength;

	std::vector<SeenIndex> seenIndices = {};
	Vertex v;
	float3 bbMin, bbMax;
	uint32_t newIndex;

	uint32_t index = allIndices[sourceMexel->startingIndex];
	bbMax = bbMin = allVertices[index].pos;

	for (uint32_t i = 0; i < sourceMexel->IndexBufferLength; i++)
	{
		index = allIndices[sourceMexel->startingIndex + i];

		for (const auto& seenIndex : seenIndices)
		{
			if (seenIndex.oldIndex == index)
			{
				allIndices.push_back(seenIndex.newIndex);
				continue;
			}
		}

		newIndex = allVertices.size();
		allIndices.push_back(newIndex);
		seenIndices.push_back({ index, newIndex });
		v.pos = matrix * float4(allVertices[index].pos, 1);
		v.nrm = matrix * float4(allVertices[index].nrm, 0);
		v.tangent = matrix * float4(allVertices[index].tangent, 0);
		v.uv = allVertices[index].uv;
		allVertices.push_back(v);
	}

	staticMexels.push_back({ matrix, allMexels.back() });
	return allMexels.back();
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
			Rect srcArea, dstArea;
			srcArea = { 0, 0, renderStages[i].srcX, renderStages[i].srcY };
			dstArea = { 0, 0, renderStages[i].dstX, renderStages[i].dstY };
			BlitImage(commandBuffer, renderStages[i].srcImage, srcArea, renderStages[i].dstImage, dstArea, renderStages[i].srcLayout, renderStages[i].blitFilter, renderStages[i].srcAspect, renderStages[i].dstAspect, renderStages[i].transitionSrc, renderStages[i].transitionDst);
			stats.blits++;
			continue;
		}

		renderPassInfo.clearValueCount = renderStages[i].clearValues.size();
		renderPassInfo.pClearValues = renderStages[i].clearValues.data();

		renderPassInfo.renderPass = renderStages[i].renderPass;
		renderArea.extent = renderStages[i].extent;
		renderPassInfo.renderArea = renderArea;
		passViewport.width = renderStages[i].extent.width;
		passViewport.height = renderStages[i].extent.height;

		vkCmdSetViewport(commandBuffer, 0, 1, &passViewport);
		vkCmdSetScissor(commandBuffer, 0, 1, &renderArea);

		bool isLast = renderStages[i].frameBuffer == NULL;
		renderPassInfo.framebuffer = isLast ? swapChainFramebuffers[imageIndex] : renderStages[i].frameBuffer;

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
						vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, sunShadowPassShader->pipeline);
						vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, sunShadowPassShader->pipelineLayout, 0, 1, &renderStages[i].descriptorSet[imageIndex], 0, NULL);
						vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, sunShadowPassShader->pipelineLayout, 1, 1, &theSun->descriptorSetPS[imageIndex], 0, VK_NULL_HANDLE);
						vkCmdDraw(commandBuffer, 3, 1, 0, 0);
						stats.drawcall_count++;
						stats.api_calls += 2;
					}

					if (numSpotLights)
					{
						vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, spotShadowPassShader->pipelineLayout, 0, 1, &renderStages[i].descriptorSet[imageIndex], 0, NULL);
						vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, spotShadowPassShader->pipeline);
						for (uint32_t j = 0; j < numSpotLights; j++)
						{
							vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, spotShadowPassShader->pipelineLayout, 1, 1, &allSpotLights[j]->descriptorSetPS[imageIndex], 0, VK_NULL_HANDLE);
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
	vkDeviceWaitIdle(logicalDevice);

	for (uint32_t i = 0; i < NUMCASCADES; i++)
		delete sunThreads[i];

	if (theSun)
		delete theSun;

	for (uint32_t i = 0; i < numSpotLights; i++)
		delete allSpotLights[i];

	numSpotLights = 0;

	for (size_t i = 0; i < numSetLayouts; i++)
		vkDestroyDescriptorSetLayout(logicalDevice, allSetLayouts[i].setLayout, nullptr);

	numSetLayouts = 0;

	for (size_t i = 0; i < numTextures; i++)
	{
		if (allTextures[i])
		{
			DestroyTexture(allTextures[i]);
			free(allTextures[i]);
		}
	}

	numTextures = 0;

	DestroyTexture(&cubemap);
	DestroyTexture(&skyCubeMap);

	for (size_t i = 0; i < allSamplers.size(); i++)
		vkDestroySampler(logicalDevice, allSamplers[i]->sampler, nullptr);

	allSamplers.clear();

	for (size_t i = 0; i < numShaders; i++)
	{
		if (allShaders[i].freePixelShader) free((void*)allShaders[i].pixelShader);
		if (allShaders[i].freeVertexShader) free((void*)allShaders[i].vertexShader);

		vkDestroyPipeline(logicalDevice, allShaders[i].pipeline, nullptr);
		vkDestroyPipelineLayout(logicalDevice, allShaders[i].pipelineLayout, nullptr);
	}

	numShaders = 0;
	numMaterials = 0;

	for (size_t i = 0; i < allMexels.size(); i++)
		free((void*)allMexels[i]->Filename);

	allMeshes.clear();

	allThingsLen = 0;

	DestroyRenderStages();
	renderStages.clear();
}

void VulkanBackend::AddToMainRenderStage(Thing* thing)
{
	for (uint32_t j = 0; j < thing->materials.size(); j++)
	{
		if (!thing->mesh->mexels[j]) continue;

		AddMexelToMainRenderStage(thing, thing->mesh->mexels[j], thing->materials[j]);
	}
}

void VulkanBackend::UpdateComputeBuffer()
{
	auto buffer = (ComputeShaderConfig*)RTShader->uniformBuffers[imageIndex]->Map();

	buffer->camDir = float4(normalize(activeCamera->target - activeCamera->position), 1);
	buffer->camPos = float4(activeCamera->position, 1);
	buffer->numIndices = allIndices.size();
	buffer->numVertices = allVertices.size();
	buffer->numObjects = computeObjects.size();
	buffer->height = swapChainExtent.height;
	buffer->width = swapChainExtent.width;
	buffer->viewProj = activeCamera->matrix;
	buffer->view = activeCamera->viewMatrix;

	RTShader->uniformBuffers[imageIndex]->UnMap();
}

void VulkanBackend::PerFrame()
{
	auto fenceStart = std::chrono::high_resolution_clock::now();
	VkResult vr = vkWaitForFences(logicalDevice, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
	auto fenceEnd = std::chrono::high_resolution_clock::now();

	if (vr == VK_ERROR_DEVICE_LOST)
	{
		std::cout << "\nERROR: Device was lost!\n\n";
		return;
	}

	gpuTime = std::chrono::duration_cast<std::chrono::microseconds>(fenceEnd - fenceStart).count();

	//UpdateComputeBuffer();
	RecordMainCommandBuffer(currentFrame);

	auto acquireStart = std::chrono::high_resolution_clock::now();
	vr = vkAcquireNextImageKHR(logicalDevice, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
	auto acquireEnd = std::chrono::high_resolution_clock::now();
	acquireTime = std::chrono::duration_cast<std::chrono::microseconds>(acquireEnd - acquireStart).count();

	// Checking if you've resized the window or otherwise need to re-create the frame-buffer
	if (vr == VK_ERROR_OUT_OF_DATE_KHR || vr == VK_SUBOPTIMAL_KHR)
	{
		return;
		/*
		EndShaderCompileThread();

		recreateSwapChain();
		currentFrame = 0;
		DestroyRenderStage();

		// The lua state will need to be redone so we can re-run the engine.lua with the new frame-buffer and swap chain size
		lua_close(L);
		InitLua();

		RecordPostStageCommandBuffers();

		StartShaderCompileThread();
		return;
		*/
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
}

void VulkanBackend::OnLevelLoad()
{
	CreateAllVertexBuffer();
	CreateAllIndexBuffer();
	SortThings();
}

Camera* VulkanBackend::GetActiveCamera()
{
	return activeCamera;
}

void VulkanBackend::SetActiveCamera(Camera* camera)
{
	activeCamera = camera;
}

const char* String_VkResult(VkResult vr)
{
	return string_VkResult(vr);
}

void VulkanBackend::AddSpotLight(float3& position, float3& dir, float fov, float attenuation)
{
	allSpotLights[numSpotLights++] = new SpotLight(position, dir, fov, attenuation, SHADOWMAPSIZE, SHADOWMAPSIZE, this);

	RefreshCommandBufferRefs();
	RecordPostProcessCommandBuffers();
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

Thing* VulkanBackend::AddThing(float3 position, float3 rotation, float3 scale, Mesh* mesh, std::vector<Material*>& materials, Texture*& shadowMap, bool isStatic, bool castsShadows, BYTE id, float shadowMapOffsetX, float shadowMapOffsetY, float shadowMapScale)
{
	Thing* thing = new Thing(position, rotation, scale, mesh, shadowMap, 1.0f, isStatic, castsShadows, id, NULL);

	thing->materials.resize(materials.size());
	memcpy(thing->materials.data(), materials.data(), materials.size() * sizeof(Material*));

	thing->shadowMapOffset = float2(shadowMapOffsetX, shadowMapOffsetY);
	thing->shadowMapScale = shadowMapScale;

	allThings[allThingsLen++] = thing;

	if (setup)
	{
		AddToMainRenderStage(thing);
		AddThingToExistingBeegShadowMap(thing);
	}

	return thing;
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

std::vector<float4> VulkanBackend::CopyImageToBuffer(Texture* src, VkImageLayout currentLayout)
{
	std::vector<float4> fullData = {};

	VkImageLayout oldLayout = currentLayout;
	transitionImageLayout(src->Image, src->format, oldLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, src->Aspect, 1, 1);

	fullData.resize(src->Width * src->Height);

	VulkanMemory* imageBuffer = new VulkanMemory(this, sizeof(float4) * src->Width, VK_BUFFER_USAGE_TRANSFER_DST_BIT, "Copying image to buffer", false, NULL);

	for (uint32_t y = 0; y < src->Height; y++)
	{
		VkBufferImageCopy region{};
		region.bufferImageHeight = 0;
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.imageOffset.x = 0;
		region.imageOffset.y = y;
		region.imageOffset.z = 0;
		region.imageExtent.width = src->Width;
		region.imageExtent.height = 1;
		region.imageExtent.depth = 1;
		region.imageSubresource.aspectMask = src->Aspect;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		region.imageSubresource.mipLevel = 0;
		auto commandBuffer = beginSingleTimeCommands();
		vkCmdCopyImageToBuffer(commandBuffer, src->Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, imageBuffer->buffer, 1, &region);
		endSingleTimeCommands(commandBuffer);
		vkDeviceWaitIdle(logicalDevice);

		void* data = imageBuffer->Map();
		memcpy(&fullData[y * src->Width], data, sizeof(float4) * src->Width);
		imageBuffer->UnMap();
	}
	delete imageBuffer;

	transitionImageLayout(src->Image, src->format, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, oldLayout, src->Aspect, 1, 1);

	return fullData;
}