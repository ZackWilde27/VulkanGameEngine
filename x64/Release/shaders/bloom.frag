#version 450

#include "zutils.glsl"
layout(location = 0) in float2 UVs;
layout(location = 0) out float4 outColour;

layout(binding = 0) uniform PostBuffer {
	float4x4 viewProj;
	float4x4 view;
	float3 camPos;
	float2 velocity;
} ubo;

layout(binding = 1) uniform sampler2D samplerColour;
const float bloomThreshold = 1.2f;

void main()
{
	float3 bloom = texture(samplerColour, UVs).rgb;

    outColour.rgb = dot(bloom, float3(1.0f)) > bloomThreshold ? bloom : float3(0);
    outColour.a = 0;
}