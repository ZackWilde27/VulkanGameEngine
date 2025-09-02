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
layout(binding = 2) uniform sampler2D samplerPos;

const float4 fogColour = float4(0.65, 0.8, 1, 1) * 0.5;
const float fogDistance = 0.001;

layout(location = 0) out float4 outColour;

void main()
{
	outColour = texture(samplerColor, UVs);
	/*
	float4 pos = texture(samplerPos, UVs);

	if (pos.w < 5000.f)
		outColour = lerp(outColour, fogColour, min(distance(pos.xyz, ubo.camPos) * fogDistance, 1.0));
	*/
}