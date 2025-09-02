#version 450

#include "zutils.glsl"
layout(location = 1) in float2 screenPos;
layout(location = 0) in float2 UVs;


layout(location = 0) out float4 outColor;

layout(binding = 0) uniform sampler2D samplerDepth;

void main()
{
	outColor = float4(tex2D(samplerDepth, UVs).r);
}