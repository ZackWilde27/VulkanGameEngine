#version 450

#include "zutils.glsl"
layout(location = 0) in float2 UVs;

layout(binding = 0) uniform PostBuffer {
	float4x4 viewProj;
	float4x4 view;
	float3 camPos;
	float2 velocity;
} ubo;


layout(binding = 1) uniform sampler2D samplerNrm;
layout(binding = 2) uniform sampler2D samplerDepth;

layout(location = 0) out float4 outColour;

const float blurDist = 3.0f;
const float3 blurDist2 = float3(blurDist, -blurDist, 0);

void main()
{
	float3 normal = texture(samplerNrm, UVs).xyz;

	float depth = ConvertDepth(texture(samplerDepth, UVs).r);

	float3 blurDst = blurDist2 / depth;

	float3 normal1 = texture(samplerNrm, UVs + blurDst.xz).xyz;
	float3 normal2 = texture(samplerNrm, UVs + blurDst.yz).xyz;
	float3 normal3 = texture(samplerNrm, UVs + blurDst.xz * 2).xyz;
	float3 normal4 = texture(samplerNrm, UVs + blurDst.yz * 2).xyz;
	float3 normal5 = texture(samplerNrm, UVs + blurDst.xz * 3).xyz;
	float3 normal6 = texture(samplerNrm, UVs + blurDst.yz * 3).xyz;

	float3 normal7 = texture(samplerNrm, UVs + blurDst.zx).xyz;
	float3 normal8 = texture(samplerNrm, UVs + blurDst.zy).xyz;
	float3 normal9 = texture(samplerNrm, UVs + blurDst.zx * 2).xyz;
	float3 normal10 = texture(samplerNrm, UVs + blurDst.zy * 2).xyz;
	float3 normal11 = texture(samplerNrm, UVs + blurDst.zx * 3).xyz;
	float3 normal12 = texture(samplerNrm, UVs + blurDst.zy * 3).xyz;

	float AO = dot(normal, normal1);
	AO += dot(normal, normal2);
	AO += dot(normal, normal3);
	AO += dot(normal, normal4);
	AO += dot(normal, normal5);
	AO += dot(normal, normal6);
	AO += dot(normal, normal7);
	AO += dot(normal, normal8);
	AO += dot(normal, normal9);
	AO += dot(normal, normal10);
	AO += dot(normal, normal11);
	AO += dot(normal, normal12);
	AO /= 12;
	AO = saturate(AO);

	outColour = float4(pow(AO, 8.0));
}