#version 450

#include "zutils.glsl"
layout(location = 0) out float4 outColor;
//layout(location = 1) out vec4 outNormal;

layout(binding = 1) uniform PSBuffer {
	float3 tint;
	float texScale;
} pxbo;

void main()
{
	outColor = float4(0.1f, 0.4f, 0.8f, 1.0f);
}