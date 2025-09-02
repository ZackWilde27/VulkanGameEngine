#version 450

#include "zutils.glsl"
layout(location = 7) in float timer;
layout(location = 6) in float3 binormal;
layout(location = 5) in float3 tangent;
layout(location = 4) in float3 camPos;
layout(location = 3) in float3 pos;
layout(location = 2) in float2 lightmapUV;
layout(location = 1) in float2 UVs;
layout(location = 0) in float3 nrm;


layout(location = 0) out float4 outColor;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out float4 outPosition;

layout(set = 1, binding = 1) uniform sampler2D aoSampler;

layout(set = 2, binding = 0) uniform sampler2D texSampler;
layout(set = 2, binding = 1) uniform sampler2D nrmSampler;
layout(set = 2, binding = 2) uniform samplerCUBE cubeSampler;

#define WATERCOLOUR float3(0.7, 0.85, 1)

void main()
{
	outPosition = float4(pos, 1000.f);

	float2 TIME = float2(timer * 0.1, timer * 0.1);
	float2 TIME2 = TIME * float2(1, -1) + float2(0.1, 0.2);

	float3 normal = (sqrt(texture(nrmSampler, UVs + TIME).rgb) - 0.5) * 2;
	float3 normal2 = (sqrt(texture(nrmSampler, UVs + TIME2).rgb) - 0.5) * 2;

	normal = normalize(normal + normal2);

	float3 worldNormal = TangentToWorld(tangent, binormal, nrm * 2, normal);

	outNormal = float4(worldNormal, 0);

	float3 view = normalize(pos - camPos);
	float3 reflection = reflect(view, worldNormal);

	float fresnel = pow(1-(-dot(worldNormal, view)), 12.0) * 0.9 + 0.1;
	fresnel *= 2.5;

	outColor = float4(texture(cubeSampler, reflection).rgb * WATERCOLOUR * fresnel, texture(aoSampler, lightmapUV).b);
}