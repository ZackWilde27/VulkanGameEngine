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
layout(binding = 2) uniform sampler2D samplerSSR;
layout(binding = 3) uniform sampler2D samplerShading;

const float4 fogColour = float4(0.65, 0.8, 1, 1) * 0.5;
const float fogDistance = 0.001;

layout(location = 0) out float4 outColour;

void main()
{
	outColour = texture(samplerColor, UVs);
	float4 ssr = texture(samplerSSR, UVs);

	if (ssr.w < 1)
		outColour *= ssr;

	float fogVal = texture(samplerShading, UVs).b;
	outColour = lerp(outColour, fogColour, fogVal);
}