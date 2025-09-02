#version 450

#include "zutils.glsl"
layout(location = 0) in float2 UVs;
layout(location = 0) out float4 outColour;

layout(binding = 0) uniform PostBuffer {
	float4x4 viewProj;
    float3 camPos;
	float2 velocity;
} ubo;

layout(binding = 1) uniform sampler2D samplerAverage;
layout(binding = 2) uniform sampler2D samplerColour;

float targetBrightness = 1.0f;

void main()
{
	float brightness = dot(texture(samplerAverage, UVs).rgb, float3(0.333f));

    float newBrightness = targetBrightness / brightness;

    newBrightness = lerp(newBrightness, 1.0f, 0.999f);

    //outColour = float4(newBrightness); return;

    outColour = texture(samplerColour, UVs);// * newBrightness;
}