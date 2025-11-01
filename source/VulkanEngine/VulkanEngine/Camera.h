#pragma once
#include "engineTypes.h"

class Camera
{
public:
	float3 position;
	float3 target;
	float3 up;
	float4x4 matrix;
	float4x4 viewMatrix;
	float3 velocityVec;
	float oldpitch, oldyaw;
	std::vector<Thing*> attachedThings;

	Camera();

	void TargetFromRotation(float pitch, float yaw);
	void UpdateMatrix(float4x4* perspectiveMatrix);
};