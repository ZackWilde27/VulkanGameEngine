#pragma once
#include "VulkanTypes.h"

class DescriptorSet;
class GPUMemory;
class Shader;
class Camera;

class Light
{
public:
	std::vector<GPUMemory*> viewProjBuffer;
	std::vector<DescriptorSet*> descriptorSetPS;

	static inline RenderStageShaderGroup shadowRenderStageOpaque;
	static inline RenderStageShaderGroup shadowRenderStageMasked;

	static inline Shader* lightShaderOpaqueStatic;
	static inline Shader* lightShaderMaskedStatic;

	static inline VkRenderPass renderPass;

	static inline VkViewport viewport = { 0.0f, 0.0f, SHADOWMAPSIZE, SHADOWMAPSIZE, 0.0f, 1.0f };
	static inline VkRect2D scissor = { { 0, 0 }, { SHADOWMAPSIZE, SHADOWMAPSIZE } };

	static inline uint32_t lightMapImageIndex;

	virtual void RecordCommandBuffer(Camera* activeCamera, uint32_t imageIndex) {}
	virtual void UpdateMatrix(Camera* activeCamera, uint32_t imageIndex) {}
};