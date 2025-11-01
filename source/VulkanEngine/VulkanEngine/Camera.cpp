#include "Camera.h"

Camera::Camera()
{
	position = float3(0);
	target = float3(1, 0, 0);
	up = float3(0, 0, 1);
	matrix = {};
	viewMatrix = {};
	velocityVec = float3(0);
	oldpitch = 0;
	oldyaw = 0;
	attachedThings = {};
}

void Camera::TargetFromRotation(float pitch, float yaw)
{
	float4x4 m = glm::rotate(float4x4(1.0f), pitch, float3(0.0f, 1.0f, 0.0f));
	m = glm::rotate(m, yaw, float3(0.0f, 0.0f, 1.0f));
	target = float4(1.0f, 0.0f, 0.0f, 0.0f) * m;
	target += position;

	velocityVec.y = (pitch - oldpitch) * 0.3f;
	velocityVec.x = (oldyaw - yaw) * 0.3f;
	velocityVec.y *= velocityVec.y * SIGN(velocityVec.y);
	velocityVec.x *= velocityVec.x * SIGN(velocityVec.x);

	oldpitch = pitch;
	oldyaw = yaw;
}

void Camera::UpdateMatrix(float4x4* perspectiveMatrix)
{
	viewMatrix = glm::lookAt(position, target, up);
	matrix = (*perspectiveMatrix) * viewMatrix;
}