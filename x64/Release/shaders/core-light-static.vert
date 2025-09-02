#version 450

#include "zutils.glsl"
layout(set = 0, binding = 0) uniform UniformBufferObject {
    float4x4 viewProj;
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
layout(location = 2) in float2 inUV;
layout(location = 3) in float3 inTangent;

void main()
{
	gl_Position = ubo.viewProj * worldMatrix * float4(inPosition, 1.0);
}