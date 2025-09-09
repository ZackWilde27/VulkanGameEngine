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
layout(binding = 2) uniform sampler2D samplerDepth;
layout(binding = 3) uniform sampler2D samplerNrm;

#include "dofconstants.hlsl"

void main()
{
	float3 col = texture(samplerColor, UVs).rgb;

    // Depth of field
	float d = texture(samplerDepth, UVs).r;

	float near = (d - dof_near) * (1/(dof_far - dof_near));

	near = saturate(near);

	float far = 1-near;

	float focal = saturate(near * far * 3);

	float blurdist = blurDist / (d * 2);

	float3 b1 = texture(samplerColor, UVs + float2(blurdist, -blurdist)).rgb;
	float3 b2 = texture(samplerColor, UVs + float2(blurdist * 2, -blurdist * 2)).rgb;
	float3 b3 = texture(samplerColor, UVs + float2(blurdist * 3, -blurdist * 3)).rgb;
	float3 b4 = texture(samplerColor, UVs + float2(blurdist * 4, -blurdist * 4)).rgb;
	float3 b5 = texture(samplerColor, UVs + float2(-blurdist, blurdist)).rgb;
	float3 b6 = texture(samplerColor, UVs + float2(-blurdist * 2, blurdist * 2)).rgb;
	float3 b7 = texture(samplerColor, UVs + float2(-blurdist * 3, blurdist * 3)).rgb;
	float3 b8 = texture(samplerColor, UVs + float2(-blurdist * 4, blurdist * 4)).rgb;

	float3 blurred = col + b1 + b2 + b3 + b4 + b5 + b6 + b7 + b8;
	blurred *= 0.1f;

	//outColor = float4(focal); return;

    outColor = float4(lerp(blurred, col, focal), 1.0f);
    //outColor = float4(focal);
}