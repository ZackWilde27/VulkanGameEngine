#version 450

#include "zutils.glsl"
layout(location = 5) in float timer;
layout(location = 4) in float3 tangent;
layout(location = 3) in float3 camPos;
layout(location = 2) in float3 pos;
layout(location = 1) in float2 UVs;
layout(location = 0) in float3 nrm;


layout(location = 0) out float4 outColor;
layout(location = 1) out float4 outNormal;
layout(location = 2) out float4 outPosition;
layout(location = 3) out float4 outGI;

layout(set = 2, binding = 0) uniform sampler2D texSampler;
layout(set = 2, binding = 1) uniform sampler2D nrmSampler;
layout(set = 2, binding = 2) uniform samplerCUBE cubeSampler;

const float3 WATERCOLOUR = float3(0.75, 0.9, 1);

void main()
{
	outPosition = float4(pos, 1000.f);

	float2 TIME = float2(timer * 0.1, timer * 0.1);
	float2 TIME2 = TIME * float2(1, -1) + float2(0.1, 0.2);

	float3 normal = (sqrt(texture(nrmSampler, UVs + TIME).rgb) - 0.5) * 2;
	float3 normal2 = (sqrt(texture(nrmSampler, UVs + TIME2).rgb) - 0.5) * 2;

	normal = normalize(normal + normal2);

	float3 binormal = cross(nrm, tangent);

	float3 worldNormal = TangentToWorld(tangent, binormal, nrm * 2, normal);

	outNormal = float4(worldNormal, 0);

	float3 incident = normalize(pos - camPos);

	float fresnel = pow(1 - -dot(worldNormal, incident), 2.0);

	outColor = float4(lerp(WATERCOLOUR, float3(1), fresnel), 1);

	outGI = float4(1);
}