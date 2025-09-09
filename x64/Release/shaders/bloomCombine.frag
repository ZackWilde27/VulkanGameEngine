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

layout(binding = 1) uniform sampler2D samplerBloom;
layout(binding = 2) uniform sampler2D samplerColour;

float bloomStrength = 0.5f;

void main()
{
	float4 bloom = texture(samplerBloom, UVs);
    outColour = texture(samplerColour, UVs) + (bloom * bloomStrength);
}