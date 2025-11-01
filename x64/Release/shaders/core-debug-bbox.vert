#version 450

#include "zutils.glsl"
layout(set = 0, binding = 0) uniform UniformBufferObject {
    float4x4 viewProj;
    float3 CAMERA; // position of the camera, in world space
    float time; // amount of time passed
} ubo;

layout(set = 1, binding = 0) uniform PerObject {
    float4x4 mat;
} perObject;

layout(location = 0) in float3 inPosition;
layout(location = 1) in float3 inNormal;
layout(location = 2) in float4 inUV;
layout(location = 3) in float3 inTangent;

void main()
{
	gl_Position = ubo.viewProj * perObject.mat * float4(inPosition, 1);
}