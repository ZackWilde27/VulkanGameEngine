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
layout(set = 2, binding = 1) uniform sampler2D rghSampler;
layout(set = 2, binding = 2) uniform sampler2D nrmSampler;
layout(set = 2, binding = 3) uniform samplerCUBE cubeSampler;

void main()
{
	float3 viewLine = pos - camPos;
	outPosition = float4(pos, length(viewLine));

	float3 view = normalize(viewLine);

	float3 worldNormal = nrm;

	float3 reflectionVector = reflect(view, worldNormal);

	outNormal = float4(worldNormal, 1.0);

	float3 reflection = texture(cubeSampler, reflectionVector).rgb;
	reflection = Desaturate(reflection, 0.4f);

	float fresnel = -dot(worldNormal, view);
	fresnel = pow(1-fresnel, 3.0);
	fresnel = mad(fresnel, 0.7, 0.3);

	float4 col = texture(texSampler, UVs);

	col.rgb = lerp(col.rgb, reflection, fresnel);

	outGI = texture(aoSampler, lightmapUV);
	outColor = float4(col.rgb, 1);
}