#version 450

#include "zutils.glsl"
layout(location = 0) in float2 UVs;


layout(location = 0) out float4 outColour;

layout(binding = 0) uniform PostBuffer {
	float4x4 viewProj;
	float4x4 view;
	float3 camPos;
	float2 velocity;
} ubo;

layout(binding = 1) uniform sampler2D samplerColor;
layout(binding = 2) uniform sampler2D samplerShadowMap;
layout(binding = 3) uniform sampler2D samplerGI;
//sampler2D samplerRT;


const float3 skyColour = float3(0.65, 0.8, 1);
const float3 fogColour = float3(0.65, 0.8, 1) * 0.5;
const float blurDst = 0.0008f;
const float2 blurDist2 = float2(blurDst, -blurDst);
const float3 sunColour = float3(1, 0.9, 0.85);

void main()
{
	float4 col = texture(samplerColor, UVs);
	float3 gi = texture(samplerGI, UVs).rgb;
	float4 shadowVal = texture(samplerShadowMap, UVs);

	// Shows just the GI map to check for places that need re-baking
	//outColour = float4(gi, 0); return;

	float2 blurDist = blurDist2 * (1-shadowVal.a);

	float3 shadow1 = texture(samplerShadowMap, UVs + blurDist.xx).rgb;
	float3 shadow2 = texture(samplerShadowMap, UVs + blurDist.xy).rgb;
	float3 shadow3 = texture(samplerShadowMap, UVs + blurDist.yx).rgb;
	float3 shadow4 = texture(samplerShadowMap, UVs + blurDist.yy).rgb;

	shadow1 = (shadowVal.rgb + shadow1 + shadow2 + shadow3 + shadow4) / 5;

	gi += shadow1;

	outColour.rgb = col.rgb * min(gi, 1.0f);
	outColour.a = 0;
}