#version 450

#include "zutils.glsl"
layout(set = 0, binding = 0) uniform UniformBufferObject {
    float4x4 viewProj;
    float3 CAMERA; // position of the camera, in world space
    float time; // amount of time passed
} ubo;

/*
layout(set = 1, binding = 0) uniform PushBufferObject {
	float4x4 world;
	float3 tint;
	float scale;
} pbo;
*/

layout(set = 1, binding = 0) readonly buffer MatrixArray{
    float4x4 data[];
} matrices;

layout(set = 1, binding = 1) readonly buffer ShadowMapOffsetArray{
    float4 data[];
} shadowMapOffsets;

#define worldMatrix (matrices.data[gl_InstanceIndex])

layout(location = 0) in float3 inPosition;
layout(location = 1) in float3 inNormal;
layout(location = 2) in float2 inUV;

layout(location = 0) out float3 pos;
layout(location = 1) out float3 camPos;
layout(location = 2) out float timer;

void main()
{
	float4 worldPos = worldMatrix * float4(inPosition, 1);
    pos = worldPos.xyz;
    camPos = ubo.CAMERA;
    gl_Position = ubo.viewProj * worldPos;
    timer = ubo.time;
}