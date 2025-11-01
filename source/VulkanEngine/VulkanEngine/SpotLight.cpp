#include "SpotLight.h"
#include "VulkanBackend.h"
#include "engine.h"
#include "Shader.h"
#include "Texture.h"
#include "DescriptorSet.h"
#include "VulkanMemory.h"
#include "Thread.h"

static bool SpotLightThreadProc(SpotLightThread* data)
{
	if (data->go)
	{
		VkCommandBuffer commandBuffer = data->light->commandBuffers[Light::lightMapImageIndex];

		float3 position = float3(data->light->position);
		float3 dir = float3(data->light->dir);

		vkBeginCommandBuffer(commandBuffer, &data->backend->beginInfo);

		BindVertexAndIndexBuffer(commandBuffer);
		vkCmdSetViewport(commandBuffer, 0, 1, &Light::viewport);
		vkCmdSetScissor(commandBuffer, 0, 1, &Light::scissor);

		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Light::lightShaderOpaqueStatic->pipeline);

		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Light::lightShaderOpaqueStatic->pipelineLayout, 0, 1, *data->light->descriptorSetVS[Light::lightMapImageIndex], 0, VK_NULL_HANDLE);
		vkCmdBeginRenderPass(commandBuffer, &data->passInfo, VK_SUBPASS_CONTENTS_INLINE);

		for (auto materialGroup : Light::shadowRenderStageOpaque.materialGroups)
		{
			for (auto meshGroup : materialGroup->meshGroups)
			{
				if (!meshGroup->isStatic || MeshGroupOnScreen(meshGroup, position, dir))
				{
					vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Light::lightShaderOpaqueStatic->pipelineLayout, 1, 1, *meshGroup->descriptorSet, 0, VK_NULL_HANDLE);
					vkCmdDrawIndexed(commandBuffer, meshGroup->mexel->IndexBufferLength, meshGroup->numInstances, meshGroup->mexel->startingIndex, meshGroup->mexel->startingVertex, 0);
				}
			}
		}

		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Light::lightShaderMaskedStatic->pipeline);

		for (auto materialGroup : Light::shadowRenderStageMasked.materialGroups)
		{
			for (auto meshGroup : materialGroup->meshGroups)
			{
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Light::lightShaderMaskedStatic->pipelineLayout, 2, 1, *materialGroup->material->descriptorSets[1], 0, VK_NULL_HANDLE);

				if (!meshGroup->isStatic || MeshGroupOnScreen(meshGroup, position, dir))
				{
					vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Light::lightShaderMaskedStatic->pipelineLayout, 1, 1, *meshGroup->descriptorSet, 0, VK_NULL_HANDLE);
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

SpotLight::SpotLight(float3& position, float3& dir, float3& colour, float fov, float attenuation, uint32_t width, uint32_t height, VulkanBackend* backend)
{
	this->position = float4(position, attenuation);
	this->dir = float4(dir, fov);
	this->colour = float4(colour, 1);

	device = backend->logicalDevice;

	renderTarget = new Texture(VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_2D, backend->findDepthFormat(), width, height, 1, 1, 1, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_DEPTH_BIT, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, true, backend);

	commandBuffers.resize(backend->MAX_FRAMES_IN_FLIGHT);

	auto queueFamily = backend->findQueueFamilies(backend->physicalDevice);
	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = queueFamily.graphicsFamily;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolInfo.pNext = VK_NULL_HANDLE;
	vkCreateCommandPool(backend->logicalDevice, &poolInfo, VK_NULL_HANDLE, &commandPool);

	VkCommandBufferAllocateInfo commandBufferInfo{};
	commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferInfo.commandBufferCount = backend->MAX_FRAMES_IN_FLIGHT;
	commandBufferInfo.commandPool = commandPool;
	commandBufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferInfo.pNext = VK_NULL_HANDLE;
	vkAllocateCommandBuffers(backend->logicalDevice, &commandBufferInfo, commandBuffers.data());

	VkFramebufferCreateInfo framebufferInfo{};
	framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	// We first need to specify with which renderPass the framebuffer needs to be compatible.
	// You can only use a framebuffer with the render passes that it is compatible with, which roughly means that they use the same number and type of attachments.
	framebufferInfo.renderPass = Light::renderPass;
	// The attachmentCount and pAttachments parameters specify the VkImageView objects that should be bound to the respective attachment descriptions in the render pass pAttachment array.
	framebufferInfo.attachmentCount = 1;

	// The width and height parameters are self-explanatory and layers refers to the number of layers in image arrays.
	framebufferInfo.width = width;
	framebufferInfo.height = height;

	// Our swap chain images are single images, so the number of layers is 1.
	framebufferInfo.layers = 1;

	framebufferInfo.pAttachments = &renderTarget->view;
	vkCreateFramebuffer(backend->logicalDevice, &framebufferInfo, nullptr, &frameBuffer);

	viewProjBuffer.resize(backend->MAX_FRAMES_IN_FLIGHT);
	descriptorSetVS.resize(backend->MAX_FRAMES_IN_FLIGHT);
	descriptorSetPS.resize(backend->MAX_FRAMES_IN_FLIGHT);

	DescriptorSetCreateInfo VSDescriptorSetInfo{ 0 };
	VSDescriptorSetInfo.numUniformBuffers = 1;

	DescriptorSetCreateInfo PSDescriptorSetInfo{ 0 };
	PSDescriptorSetInfo.numPUniformBuffers = 1;
	PSDescriptorSetInfo.numSamplers = 1;

	VkDescriptorBufferInfo bufferInfo{};
	bufferInfo.offset = 0;

	VkDescriptorImageInfo imageInfo = renderTarget->GetImageInfo(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	for (size_t i = 0; i < backend->MAX_FRAMES_IN_FLIGHT; i++)
	{
		viewProjBuffer[i] = new VulkanMemory(sizeof(float4x4) + sizeof(float4) * 3, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, "SpotLight", false, NULL);

		descriptorSetVS[i] = new DescriptorSet(VSDescriptorSetInfo);
		descriptorSetPS[i] = new DescriptorSet(PSDescriptorSetInfo);

		bufferInfo.buffer = *viewProjBuffer[i];
		bufferInfo.range = sizeof(float4x4);
		descriptorSetVS[i]->Update(&bufferInfo, NULL, NULL, NULL, NULL);

		bufferInfo.range = sizeof(float4x4) + (sizeof(float4) * 3);
		descriptorSetPS[i]->Update(NULL, NULL, &bufferInfo, &imageInfo, NULL);
		UpdateMatrix(NULL, i);
	}

	thread.backend = backend;
	thread.done = false;
	thread.go = false;
	thread.light = this;

	thread.passInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	thread.passInfo.renderPass = Light::renderPass;
	thread.passInfo.framebuffer = frameBuffer;
	thread.passInfo.clearValueCount = 1;
	thread.passInfo.pClearValues = &backend->clearValues[1];
	thread.passInfo.renderArea = { 0, 0, SHADOWMAPSIZE, SHADOWMAPSIZE };
	thread.passInfo.pNext = VK_NULL_HANDLE;

	thread.thread = new Thread((zThreadFunc)SpotLightThreadProc, &thread);
}

SpotLight::~SpotLight()
{
	delete thread.thread;

	vkFreeCommandBuffers(device, commandPool, (uint32_t)commandBuffers.size(), commandBuffers.data());
	vkDestroyCommandPool(device, commandPool, VK_NULL_HANDLE);

	vkDestroyFramebuffer(device, frameBuffer, VK_NULL_HANDLE);

	delete renderTarget;

	for (size_t i = 0; i < viewProjBuffer.size(); i++)
		delete viewProjBuffer[i];
}

void SpotLight::UpdateMatrix(Camera* activeCamera, uint32_t imageIndex)
{
	float4x4 viewMatrix = glm::lookAt((float3)position, (float3)position + (float3)dir, float3(0.0f, 0.0f, 1.0f));

	SpotLightBuffer* block = (SpotLightBuffer*)viewProjBuffer[imageIndex]->Map();

	block->viewProj = glm::perspective(dir.a, 1.0f, 0.1f, position.a);
	block->viewProj[1][1] *= -1;
	block->viewProj *= viewMatrix;
	block->dir = dir;
	block->pos = position;
	block->col = colour;

	viewProjBuffer[imageIndex]->UnMap();
}