#version 450

#include "zutils.glsl"
layout(location = 0) in float2 UVs;
layout(binding = 0) uniform PostBuffer {
	float4x4 viewProj;
	float4x4 view;
	float3 camPos;
	float2 velocity;
} ubo;

layout(binding = 1) uniform sampler2D samplerColor;

layout(location = 0) out float4 outColor;


// AAGX parameters
const float3 luminanceWeights = float3(0.4);
const float desatStrength = 2.0;
const float desatStrength2 = 0.5;
const float contrast = 1.2;

// Colour correction
const float exposure = 1.8;
const float3 tint = float3(1);//float3(1, 0.95, 0.85);


const float chromatic_strength = 0.3f;
const float chromatic_distance = 0.0015f;

void main()
{
	float3 col = texture(samplerColor, UVs).rgb * exposure * tint;


	float border = distance(UVs, float2(0.5f, 0.5f));
	border = pow(1.0f - border, 1.5f);


	// Chromatic Aberration
	float2 dir = normalize(UVs - 0.5f) * chromatic_distance;

	float rd = col.r;
	float gr = texture(samplerColor, UVs + dir).g;
	float bl = texture(samplerColor, UVs - dir).b;

	col = lerp(col, float3(rd, gr, bl), (1-border) * chromatic_strength);


	// My own tone-mapper based on AGX used in Blender, I call it 'Approximate AGX'
	// I guess it could be shortened to AAGX
	float lum = saturate(dot(col, luminanceWeights));
	col = Desaturate(col, pow(lum, desatStrength) * desatStrength2);

	col = pow(col * contrast, contrast);


	// Vignette
	col *= border;

	outColor = float4(col, 1);
}