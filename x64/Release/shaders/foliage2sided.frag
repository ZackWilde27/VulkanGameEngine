#version 450

#include "zutils.glsl"
layout(location = 5) in float3 camPos;
layout(location = 4) in float3 tangent;
layout(location = 3) in float3 pos;
layout(location = 2) in float2 lightmapUV;
layout(location = 1) in float2 UVs;
layout(location = 0) in float3 nrm;
layout(location = 0) out float4 outColor;
layout(location = 1) out float4 outNormal;
layout(location = 2) out float4 outPosition;
layout(location = 3) out float4 outGI;


layout(set = 0, binding = 1) uniform sampler2D aoSampler;

layout(set = 2, binding = 0) uniform sampler2D texSampler;
layout(set = 2, binding = 1) uniform samplerCUBE cubeSampler;

void main()
{
	float4 col = texture(texSampler, UVs);
	if (col.a < 0.5) discard;

	outPosition = float4(pos, distance(pos, camPos));

	outNormal = float4(nrm, 1);

	outGI = texture(aoSampler, lightmapUV);
	outColor = float4(col.rgb, 1);
}