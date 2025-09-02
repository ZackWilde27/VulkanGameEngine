#version 450

#include "zutils.glsl"
layout(set = 0, binding = 0) uniform UniformBufferObject {
    float4x4 viewProj;
    float3 CAMERA; // position of the camera, in world space
    float time; // amount of time passed
} ubo;

layout(set = 1, binding = 0) uniform PushBufferObject {
	float4x4 world;
	float3 tint;
	float scale;
} pbo;

#define WAVESCALEX 0.7
#define WAVESCALEY 0.8

#define WAVEHEIGHTX 0.1
#define WAVEHEIGHTY 0.1

layout(location = 0) in float3 inPosition;
layout(location = 1) in float3 inNormal;
layout(location = 2) in float4 inUV;
layout(location = 3) in float3 inTangent;

layout(location = 0) out float3 nrm;
layout(location = 1) out float2 UVs;
layout(location = 2) out float2 lightmapUV;
layout(location = 3) out float3 pos;
layout(location = 4) out float3 camPos;
layout(location = 5) out float3 tangent;
layout(location = 6) out float3 binormal;
layout(location = 7) out float timer;

void main()
{
	nrm = normalize((pbo.world * float4(inNormal, 0)).xyz);

    float4 worldPosition = pbo.world * float4(inPosition, 1.0);

	//worldPosition.z += sin(ubo.time + (worldPosition.x * WAVESCALEX)) * WAVEHEIGHTX;
	//worldPosition.z += sin(ubo.time + (worldPosition.y * WAVESCALEY)) * WAVEHEIGHTY;

    gl_Position = ubo.viewProj * worldPosition;

    UVs = inUV.xy;
	lightmapUV = inUV.zw;
	tangent = normalize((pbo.world * float4(inTangent, 0)).xyz);
    binormal = cross(nrm, tangent);
    pos = worldPosition.xyz;
	camPos = ubo.CAMERA;
	timer = ubo.time;
}