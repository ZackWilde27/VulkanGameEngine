#include "Thing.h"
#include "../BackendUtils.h"
#include "VulkanMemory.h"
#include "DescriptorSet.h"
#include "../engine.h"

Thing::Thing(float3 position, float3 rotation, float3 scale, Mesh* mesh, Texture*& shadowmap, float texScale, VkBool32 isStatic, VkBool32 castShadow, CollisionType collision, BYTE id, const char* scriptFilename) : shadowMap(shadowmap)
{
	this->mesh = mesh;

	this->position = position;
	this->rotation = rotation;
	this->scale = scale;

	this->collisionType = collision;

	matrix = WorldMatrix(position, rotation, scale);

	this->shadowMap = shadowmap;
	this->isStatic = isStatic;
	this->castShadow = castShadow;
	this->id = id;

	parent = NULL;
	children = {};

	shadowMapOffset = float2(0);
	shadowMapScale = 0;

	fileOffset = 0;

	boundingBoxBuffer = new GPUMemory(sizeof(float4x4), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, "Thing bounding box", false, NULL);

	DescriptorSetCreateInfo info{};
	info.numUniformBuffers = 1;
	boundingBoxDescriptorSet = new DescriptorSet(info);

	auto bufferInfo = boundingBoxBuffer->GetBufferInfo();
	boundingBoxDescriptorSet->Update(&bufferInfo, NULL, NULL, NULL, NULL);
}

Thing::~Thing()
{
	GetEngine()->RemoveThing(this);
	delete boundingBoxBuffer;
	delete boundingBoxDescriptorSet;
}

void Thing::UpdateMatrixForReal()
{
	if (isStatic)
	{
		PrintF("Cannot update matrix on a static object!\n");
		return;
	}

	size_t len = meshGroups.size();
	for (size_t i = 0; i < len; i++)
	{
		float4x4* data = (float4x4*)meshGroups[i]->matrixMem->Map(matrixIndices[i] * sizeof(float4x4), sizeof(float4x4));
		*data = matrix;
		meshGroups[i]->matrixMem->UnMap();
	}
}

void Thing::UpdateMatrix()
{
	matrix = WorldMatrix(position, rotation, scale);

	const Thing* thing = this;
	while (thing->parent)
	{
		matrix = WorldMatrix(thing->parent->position, thing->parent->rotation, thing->parent->scale) * matrix;
		thing = thing->parent;
	}

	UpdateMatrixForReal();
}

void Thing::UpdateMatrix(float4x4* overrideMatrix)
{
	matrix = *overrideMatrix;
	UpdateMatrixForReal();
}
