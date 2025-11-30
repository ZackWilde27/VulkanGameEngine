#pragma once
#include "../engineTypes.h"
#include "VulkanBackend.h"

class Thing
{
public:
	float3 position;
	float3 rotation;
	float3 scale;

	float4x4 matrix;

	GPUMemory* boundingBoxBuffer;
	DescriptorSet* boundingBoxDescriptorSet;

	CollisionType collisionType;

	// The offset into the level file, for updating it
	size_t fileOffset;

	// For attaching things to things, when updating an object's matrix it will go up the chain of parents combining their matrices.
	Thing* parent;
	// It needs the children too for attaching things to the camera, since it works in reverse there, starting at the camera and going down the list of children to make sure everything is updated along with it
	std::vector<Thing*> children;

	std::vector<RenderStageMeshGroup*> meshGroups; // Pointer to its mesh group for movable objects to update their matrix
	std::vector<uint32_t> matrixIndices; // Index into the matrix array in the meshGroup for updating the matrix

	Texture*& shadowMap;
	Mesh* mesh;
	std::vector<Material*> materials;

	float2 shadowMapOffset;
	float shadowMapScale;

	BYTE id;
	VkBool32 isStatic;
	VkBool32 castShadow;

private:
	void UpdateMatrixForReal();

public:
	Thing(float3 position, float3 rotation, float3 scale, Mesh* mesh, Texture*& shadowmap, float texScale, VkBool32 isStatic, VkBool32 castShadow, CollisionType collision, BYTE id, const char* scriptFilename);
	~Thing();

	void UpdateMatrix();
	void UpdateMatrix(float4x4* overrideMatrix);
};