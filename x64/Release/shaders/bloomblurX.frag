#version 450

#include "zutils.glsl"
layout(location = 0) in float2 UVs;
layout(location = 0) out float4 outColour;

layout(binding = 0) uniform PostBuffer {
	float4x4 viewProj;
    float3 camPos;
	float2 velocity;
} ubo;

layout(binding = 1) uniform sampler2D samplerColour;
const float blurDistance = 0.03f;

void main()
{
	float3 s1 = texture(samplerColour, UVs).rgb;

    float3 s2 = texture(samplerColour, UVs + float2(blurDistance, 0)).rgb;
    float3 s3 = texture(samplerColour, UVs + float2(blurDistance * 2, 0)).rgb;
    float3 s4 = texture(samplerColour, UVs + float2(blurDistance * 3, 0)).rgb;

    float3 s5 = texture(samplerColour, UVs + float2(-blurDistance, 0)).rgb;
    float3 s6 = texture(samplerColour, UVs + float2(-blurDistance * 2, 0)).rgb;
    float3 s7 = texture(samplerColour, UVs + float2(-blurDistance * 3, 0)).rgb;

    outColour.rgb = (s1 + s2 + s3 + s4 + s5 + s6 + s7) * 0.142;
    outColour.a = 0;
}