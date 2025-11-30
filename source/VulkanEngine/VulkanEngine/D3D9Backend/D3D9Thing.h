#pragma once
#include <windows.h>
#include <vector>
#include "../engineTypes.h"
#include <d3d9.h>

class Mesh;

class Thing
{
	float3 position;
	float3 rotation;
	float3 scale;

	float4x4 matrix;

	GPUMemory* boundingBoxBuffer;

	CollisionType collisionType;

	// The offset into the level file, for updating it
	size_t fileOffset;

	// For attaching things to things, when updating an object's matrix it will go up the chain of parents combining their matrices.
	Thing* parent;
	// It needs the children too for attaching things to the camera, since it works in reverse there, starting at the camera and going down the list of children to make sure everything is updated along with it
	std::vector<Thing*> children;

	Texture* shadowMap;
	Mesh* mesh;
	std::vector<Material*> materials;

	float2 shadowMapOffset;
	float shadowMapScale;

	BYTE id;
	BOOL isStatic;
	BOOL castShadow;

public:
	Thing();
	~Thing();

	void Draw(LPDIRECT3DDEVICE9 device);
};