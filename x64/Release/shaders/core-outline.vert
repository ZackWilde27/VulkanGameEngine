#version 450

#include "zutils.glsl"
layout(binding = 0) uniform UniformBufferObject {
    float4x4 viewProj;
    float3 CAMERA; // position of the camera, in world space
    float time; // amount of time passed
    float sinetime; // oscilates between -1 and 1 based on time, just so the shader doesn't have to do it
} ubo;

layout(push_constant) uniform PushBufferObject {
	float4x4 world;
	float3 tint;
	float scale;
} pbo;

layout(location = 0) in float3 inPosition;
layout(location = 1) in float3 inNormal;
layout(location = 2) in float4 inUV;
layout(location = 3) in float3 inTangent;

void main()
{
	gl_Position = ubo.viewProj * pbo.world * float4(inPosition * 1.02, 1);
}