#version 450

#include "zutils.glsl"
layout(set = 0, binding = 0) uniform UniformBufferObject {
    float4x4 viewProj;
    float3 CAMERA; // position of the camera, in world space
    float time; // amount of time passed
} ubo;

layout(set = 1, binding = 0) readonly buffer MatrixArray{
    float4x4 data[];
} matrices;

layout(set = 1, binding = 1) readonly buffer ShadowMapOffsetArray{
    float4 data[];
} shadowMapOffsets;


#define worldMatrix (matrices.data[gl_InstanceIndex])

layout(location = 0) in float3 inPosition;
layout(location = 1) in float3 inNormal;
layout(location = 2) in float4 inUV;
layout(location = 3) in float3 inTangent;

layout(location = 0) out float3 nrm;
layout(location = 1) out float2 UVs;
layout(location = 2) out float2 lightmapUV;
layout(location = 3) out float3 pos;
layout(location = 4) out float3 tangent;
layout(location = 5) out float3 camPos;

void main()
{
	nrm = normalize(float3x3(worldMatrix) * inNormal);

    float4 worldPosition = worldMatrix * float4(inPosition, 1.0);

    gl_Position = ubo.viewProj * worldPosition;

    UVs = inUV.xy;
	lightmapUV = (inUV.zw * shadowMapOffsets.data[gl_InstanceIndex].zw) + shadowMapOffsets.data[gl_InstanceIndex].xy;
	tangent = normalize(float3x3(worldMatrix) * inTangent);
    pos = worldPosition.xyz;
    camPos = ubo.CAMERA;
}