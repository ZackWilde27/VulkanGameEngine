#pragma once
#include "Light.h"
#include "../Backend.h"
#include "../engineTypes.h"

constexpr int NUMCASCADES = 4;

struct SunUniformBuffer
{
	float4x4 viewProjs[NUMCASCADES];
	float3 dir;
};

class SunLight : public Light
{
public:
	float3 dir;
	float3 offset;
	float4 orthoParams;
	Texture* renderTargets[NUMCASCADES];
	VkFramebuffer frameBuffers[NUMCASCADES];
	std::vector<std::array<VkCommandBuffer, NUMCASCADES>> commandBuffers;
	std::vector<std::array<DescriptorSet*, NUMCASCADES>> descriptorSetVS;
	float4x4 projectionMatrices[NUMCASCADES];
	float cascadeDistances[NUMCASCADES];

	static inline VkRenderPass sunShadowPassRenderPass;
	static inline Shader* shadowPassShader;

	// The sun's shadow maps are recorded by dispatching a thread for each cascade
	// I don't know why, but each thread needs its own command pool
	static inline VkCommandPool commandPools[NUMCASCADES];

public:
	SunLight(float3 dir, uint32_t width, uint32_t height, Backend* backend);
	~SunLight();

	void UpdateProjection();
	void UpdateMatrix(Camera* playerCamera, uint32_t imageIndex) override;

	void SetupSunThreads(uint32_t imageIndex);

	static void WaitForSunThreads();
	static void DeleteSunThreads();
};