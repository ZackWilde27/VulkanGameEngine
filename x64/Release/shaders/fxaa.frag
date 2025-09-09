#version 450

#include "zutils.glsl"
layout(location = 0) in float2 UVs;
layout(location = 0) out float4 outColor;

layout(binding = 0) uniform PostBuffer {
	float4x4 viewProj;
	float4x4 view;
	float3 camPos;
	float2 velocity;
} ubo;

layout(binding = 1) uniform sampler2D samplerColor;

const float2 inverseScreenSize = float2(1.0/1706.0, 1.0/960.0);

#define DISABLE_FXAA

void main()
{
	#ifdef DISABLE_FXAA
        outColor = texture(samplerColor, UVs);
    #else
        float4 texCorners = float4(UVs + (inverseScreenSize * 0.5), UVs - (inverseScreenSize * 0.5));

        float3 col = texture(samplerColor, UVs).rgb;
        float3 col2 = texture(samplerColor, texCorners.xy).rgb;
        float3 col3 = texture(samplerColor, texCorners.zw).rgb;
        float3 col4 = texture(samplerColor, texCorners.xw).rgb;
        float3 col5 = texture(samplerColor, texCorners.zy).rgb;

        outColor.rgb = (col + col2 + col3 + col4 + col5) / 5;
        outColor.a = 1;
    #endif
}