#include "SunLight.h"
#include "VulkanBackend.h"
#include "engine.h"
#include "Thread.h"
#include "Texture.h"
#include "VulkanMemory.h"
#include "DescriptorSet.h"
#include "Shader.h"
#include "BackendUtils.h"

VkCommandBufferBeginInfo* lightMapBeginInfo;
SunLight* lightMapSun;
VkRenderPass sunOpaquePass;
Thread* sunThreads[NUMCASCADES];
SunPassThreadInfo sunThreadInfos[NUMCASCADES];

// These have to be marked as volatile or the while loop that waits for the threads to finish will be optimized out for who knows what reason
volatile bool sunThreadGos[NUMCASCADES];
volatile bool sunThreadDones[NUMCASCADES];

#define RAYCASTMETHOD
static bool MeshGroupOnCascade(RenderStageMeshGroup* meshGroup, float distance)
{
#ifdef ENABLE_CULLING
#ifdef RAYCASTMETHOD
	float3 dir = normalize(meshGroup->boundingBoxCentre - GetActiveCamera()->position);
	float3 coords;
	if (!HitBoundingBox(meshGroup->boundingBoxMin, meshGroup->boundingBoxMax, GetActiveCamera()->position, dir, coords))
		return true;

	return (glm::distance(coords, GetActiveCamera()->position) / 4) < distance;
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

	auto commandBuffer = lightMapSun->commandBuffers[Light::lightMapImageIndex][threadInfo->cascade];

	vkBeginCommandBuffer(commandBuffer, lightMapBeginInfo);
	BindVertexAndIndexBuffer(commandBuffer);
	vkCmdSetViewport(commandBuffer, 0, 1, &Light::viewport);
	vkCmdSetScissor(commandBuffer, 0, 1, &Light::scissor);
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, threadInfo->opaqueShader->pipeline);

	threadInfo->passInfo.renderPass = sunOpaquePass;
	threadInfo->passInfo.renderArea = { 0, 0, SHADOWMAPSIZE, SHADOWMAPSIZE };

	threadInfo->passInfo.framebuffer = lightMapSun->frameBuffers[threadInfo->cascade];

	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, threadInfo->opaqueShader->pipelineLayout, 0, 1, *lightMapSun->descriptorSetVS[Light::lightMapImageIndex][threadInfo->cascade], 0, VK_NULL_HANDLE);
	vkCmdBeginRenderPass(commandBuffer, &threadInfo->passInfo, VK_SUBPASS_CONTENTS_INLINE);

	for (auto materialGroup : Light::shadowRenderStageOpaque.materialGroups)
	{
		for (auto meshGroup : materialGroup->meshGroups)
		{
			if (MeshGroupOnCascade(meshGroup, lightMapSun->cascadeDistances[threadInfo->cascade]))
			{
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, threadInfo->opaqueShader->pipelineLayout, 1, 1, &meshGroup->descriptorSet->set, 0, VK_NULL_HANDLE);
				vkCmdDrawIndexed(commandBuffer, meshGroup->mexel->IndexBufferLength, meshGroup->numInstances, meshGroup->mexel->startingIndex, meshGroup->mexel->startingVertex, 0);
			}
		}
	}

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, threadInfo->maskedShader->pipeline);

	for (auto materialGroup : Light::shadowRenderStageMasked.materialGroups)
	{
		for (auto meshGroup : materialGroup->meshGroups)
		{
			if (MeshGroupOnCascade(meshGroup, lightMapSun->cascadeDistances[threadInfo->cascade]))
			{
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, threadInfo->maskedShader->pipelineLayout, 1, 1, *meshGroup->descriptorSet, 0, VK_NULL_HANDLE);
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, threadInfo->maskedShader->pipelineLayout, 2, 1, *materialGroup->material->descriptorSets[1], 0, VK_NULL_HANDLE);
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


SunLight::SunLight(float3 dir, uint32_t width, uint32_t height, VulkanBackend* backend)
{
	this->dir = dir;
	this->offset = dir * -2500.f;
	this->orthoParams = float4(2.0f, 4.0f, 0.0f, 0.0f);

	commandBuffers.resize(backend->MAX_FRAMES_IN_FLIGHT);
	VkCommandBufferAllocateInfo cmdInfo{};
	cmdInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdInfo.pNext = VK_NULL_HANDLE;
	cmdInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cmdInfo.commandBufferCount = 1;
	for (uint32_t j = 0; j < NUMCASCADES; j++)
	{
		cmdInfo.commandPool = SunLight::commandPools[j];
		for (uint32_t i = 0; i < backend->MAX_FRAMES_IN_FLIGHT; i++)
			vkAllocateCommandBuffers(backend->logicalDevice, &cmdInfo, &commandBuffers[i][j]);
	}

	for (uint32_t i = 0; i < NUMCASCADES; i++)
		renderTargets[i] = new Texture(VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_2D, backend->findDepthFormat(), width, height, 1, 1, 1, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_DEPTH_BIT, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, true, backend);

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

	for (uint32_t i = 0; i < NUMCASCADES; i++)
	{
		framebufferInfo.pAttachments = &renderTargets[i]->view;
		vkCreateFramebuffer(backend->logicalDevice, &framebufferInfo, nullptr, &frameBuffers[i]);
	}

	viewProjBuffer.resize(backend->MAX_FRAMES_IN_FLIGHT);

	for (size_t i = 0; i < backend->MAX_FRAMES_IN_FLIGHT; i++)
		viewProjBuffer[i] = new VulkanMemory((sizeof(float4x4) * NUMCASCADES) + sizeof(float4), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, "SunLight", false, NULL);

	descriptorSetVS.resize(backend->MAX_FRAMES_IN_FLIGHT);
	descriptorSetPS.resize(backend->MAX_FRAMES_IN_FLIGHT);

	DescriptorSetCreateInfo info{ 0 };

	info.numUniformBuffers = 1;
	for (size_t i = 0; i < backend->MAX_FRAMES_IN_FLIGHT; i++)
	{
		for (size_t c = 0; c < NUMCASCADES; c++)
			descriptorSetVS[i][c] = new DescriptorSet(info);
	}

	info.numUniformBuffers = 0;
	info.numPUniformBuffers = 1;
	info.numSamplers = NUMCASCADES;
	for (size_t i = 0; i < backend->MAX_FRAMES_IN_FLIGHT; i++)
		descriptorSetPS[i] = new DescriptorSet(info);

	VkDescriptorBufferInfo bufferInfos[NUMCASCADES];
	size_t offset = 0;
	VkWriteDescriptorSet writes[NUMCASCADES + 1];
	for (size_t j = 0; j < backend->MAX_FRAMES_IN_FLIGHT; j++)
	{
		offset = 0;
		for (uint32_t i = 0; i < NUMCASCADES; i++)
		{
			bufferInfos[i].buffer = *viewProjBuffer[j];
			bufferInfos[i].offset = offset;
			bufferInfos[i].range = sizeof(float4x4);
			offset += sizeof(float4x4);

			writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[i].pNext = nullptr;
			writes[i].dstSet = *descriptorSetVS[j][i];
			writes[i].dstBinding = 0;
			writes[i].dstArrayElement = 0;
			writes[i].descriptorCount = 1;
			writes[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			writes[i].pBufferInfo = &bufferInfos[i];
		}

		vkUpdateDescriptorSets(backend->logicalDevice, NUMCASCADES, writes, 0, nullptr);
	}

	VkDescriptorImageInfo imageInfos[NUMCASCADES];

	for (size_t j = 0; j < backend->MAX_FRAMES_IN_FLIGHT; j++)
	{
		writes[0].dstSet = *descriptorSetPS[j];
		bufferInfos[0].buffer = *viewProjBuffer[j];
		bufferInfos[0].range = sizeof(float4x4) * NUMCASCADES + sizeof(float4);

		for (uint32_t i = 1; i < (NUMCASCADES + 1); i++)
		{
			writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[i].dstSet = *descriptorSetPS[j];
			writes[i].dstBinding = i;
			writes[i].dstArrayElement = 0;
			writes[i].descriptorCount = 1;
			writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writes[i].pBufferInfo = VK_NULL_HANDLE;
			writes[i].pNext = VK_NULL_HANDLE;

			imageInfos[i - 1] = renderTargets[i - 1]->GetImageInfo(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			writes[i].pImageInfo = &imageInfos[i - 1];
		}

		vkUpdateDescriptorSets(backend->logicalDevice, NUMCASCADES + 1, writes, 0, nullptr);
	}

	cascadeDistances[0] = 2.f;
	cascadeDistances[1] = 6.f;
	cascadeDistances[2] = 25.f;
	cascadeDistances[3] = 50.f;

	UpdateProjection();

	lightMapSun = this;

	VkRenderPassBeginInfo beginInfo;

	beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	beginInfo.clearValueCount = 1;
	beginInfo.pClearValues = &backend->clearValues[1];
	beginInfo.renderArea = { 0, 0, SHADOWMAPSIZE, SHADOWMAPSIZE };
	beginInfo.renderPass = Light::renderPass;
	beginInfo.pNext = VK_NULL_HANDLE;

	for (uint32_t i = 0; i < NUMCASCADES; i++)
	{
		sunThreadGos[i] = false;
		sunThreadDones[i] = false;
		sunThreadInfos[i] = { i, beginInfo, Light::lightShaderOpaqueStatic, Light::lightShaderMaskedStatic };
		sunThreads[i] = new Thread((zThreadFunc)RecordSunPassThread, (void*)&sunThreadInfos[i]);
	}

	lightMapBeginInfo = &backend->beginInfo;
	sunOpaquePass = Light::renderPass;
}

SunLight::~SunLight()
{
	LastGenEngine* engine = GetEngine();

	for (uint32_t i = 0; i < engine->backend->MAX_FRAMES_IN_FLIGHT; i++)
		delete viewProjBuffer[i];

	for (uint32_t i = 0; i < NUMCASCADES; i++)
	{
		vkDestroyFramebuffer(engine->backend->logicalDevice, frameBuffers[i], VK_NULL_HANDLE);
		delete renderTargets[i];
	}
}

void SunLight::UpdateProjection()
{
	for (uint32_t i = 0; i < NUMCASCADES; i++)
		projectionMatrices[i] = glm::ortho(-cascadeDistances[i], cascadeDistances[i], cascadeDistances[i], -cascadeDistances[i], 0.1f, 5000.f);
}

void SunLight::UpdateMatrix(Camera* playerCamera, uint32_t imageIndex)
{
	float4x4 viewMatrix = glm::lookAt(playerCamera->position + offset, playerCamera->position, float3(0.0f, 0.0f, 1.0f));

	SunUniformBuffer* sub = (SunUniformBuffer*)this->viewProjBuffer[imageIndex]->Map();

	for (uint32_t i = 0; i < NUMCASCADES; i++)
		sub->viewProjs[i] = projectionMatrices[i] * viewMatrix;

	sub->dir = dir;

	this->viewProjBuffer[imageIndex]->UnMap();
}

void SunLight::WaitForSunThreads()
{
	for (uint32_t i = 0; i < NUMCASCADES; i++)
		while (!sunThreadDones[i]);
}

void SunLight::DeleteSunThreads()
{
	for (uint32_t i = 0; i < NUMCASCADES; i++)
		delete sunThreads[i];
}

void SunLight::SetupSunThreads(uint32_t imageIndex)
{
	lightMapSun = this;

	for (uint32_t i = 0; i < NUMCASCADES; i++)
	{
		sunThreadDones[i] = false;
		sunThreadGos[i] = true;
	}
}