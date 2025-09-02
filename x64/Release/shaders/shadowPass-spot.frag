#version 450

#include "zutils.glsl"
layout(location = 0) in float2 UVs;
layout(binding = 0) uniform PostBuffer {
	float4x4 viewProj;
	float4x4 view;
	float3 camPos;
	float2 velocity;
} ubo;

layout(set = 0, binding = 1) uniform sampler2D samplerPosition;
layout(set = 0, binding = 2) uniform sampler2D samplerNormal;

layout(set = 1, binding = 0) uniform PushBufferObject {
	float4x4 viewProj;
	float4 lightDir; // a = fov
	float4 lightPos; // a = attenuation
} pbo;

layout(set = 1, binding = 1) uniform sampler2D samplerShadowMap;

layout(location = 0) out float4 outColour;

bool OutsideCascade(float2 coord)
{
	return coord.x < 0.0f || coord.x > 1.0f || coord.y < 0.0f || coord.y > 1.0f;
}

const float shadowBias = 0.00001f;
const float halfPI = 3.14159 / 2;

void main()
{
	float4 worldPos = float4(texture(samplerPosition, UVs).rgb, 1);
	float3 dir = normalize(worldPos.xyz - pbo.lightPos.xyz);
	float dst = distance(worldPos.xyz, pbo.lightPos.xyz);

	if (dst >= pbo.lightPos.w || dot(dir, pbo.lightDir.xyz) < 1-(saturate(pbo.lightDir.w / 3.142)))
		discard;

	float4 screenPos = pbo.viewProj * worldPos;
	screenPos.xyz /= screenPos.w;

	float distance_from_centre = dot(screenPos.xy, screenPos.xy);

	if (distance_from_centre > 0.5)
		discard;

	distance_from_centre = 1-distance_from_centre;
	distance_from_centre -= 0.5f;
	distance_from_centre *= 2;

	float brightness = pow(distance_from_centre, 2.0);

	screenPos.xy *= 0.5f;
	screenPos.xy += 0.5f;

	if (OutsideCascade(screenPos.xy))
		discard;

	float shadowDepth = texture(samplerShadowMap, screenPos.xy).r;

	if ((screenPos.z - shadowBias) >= shadowDepth)
		discard;

	float4 normal = texture(samplerNormal, UVs);

	float coef = lerp(500.0f, 25.0f, normal.a);

	float3 viewDir = normalize(worldPos.xyz - ubo.camPos);
	float3 reflection = reflect(viewDir, normal.xyz);

	float d = saturate(-dot(normal.xyz, dir));

	outColour.g = pow(saturate(-dot(reflection, dir)), coef) * (1-normal.a);
	outColour.r = d * brightness;
	outColour.b = 0;
	outColour.a = outColour.r;
}