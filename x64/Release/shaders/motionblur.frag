#version 450

#include "zutils.glsl"
layout(location = 0) in float2 UVs;
layout(binding = 0) uniform PostBuffer {
	float4x4 viewProj;
    float4x4 view;
	float3 camPos;
	float2 velocity;
} ubo;

layout(binding = 1) uniform sampler2D samplerColor;

const float samples = 8;
const float dist = 0.4f;
const float sampleDist = dist / samples;

layout(location = 0) out float4 outColor;

void main()
{
	float3 col = texture(samplerColor, UVs).rgb;

	float2 vel = ubo.velocity * sampleDist;

	for (float i = 1; i < samples; i++)
		col += texture(samplerColor, saturate(UVs - (vel * i))).rgb;

	col /= samples;

	outColor = float4(col, 1.0f);
}