#pragma once
#include "Light.h"

struct SpotLightThread
{
	Thread* thread;
	volatile bool go;
	volatile bool done;
	SpotLight* light;
	VulkanBackend* backend;
	VkRenderPassBeginInfo passInfo;
};

struct SpotLightBuffer
{
	float4x4 viewProj;
	float4 dir;
	float4 pos;
	float4 col;
};

bool SpotLightThreadProc(SpotLightThread* data);

class SpotLight : public Light
{
public:
	float4 position; // W component is attenuation
	float4 dir; // W component is FOV
	float4 colour; // W component is unused, but I don't want any alignment problems so it's a float4
	Texture* renderTarget;
	VkFramebuffer frameBuffer;
	std::vector<DescriptorSet*> descriptorSetVS;
	std::vector<VkCommandBuffer> commandBuffers;
	float4x4 viewProj;
	VkCommandPool commandPool;
	VkDevice device;
	SpotLightThread thread;
	BYTE updateTimer;

	static inline VkRenderPass spotShadowPassRenderPass;
	static inline Shader* shadowPassShader;

public:
	SpotLight(float3& position, float3& dir, float3& colour, float fov, float attenuation, uint32_t width, uint32_t height, VulkanBackend* backend);
	~SpotLight();

	void UpdateMatrix(Camera* activeCamera, uint32_t imageIndex) override;
};