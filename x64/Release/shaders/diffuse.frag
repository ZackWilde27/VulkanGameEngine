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
layout(set = 2, binding = 1) uniform sampler2D nrmSampler;
layout(set = 2, binding = 2) uniform samplerCUBE cubeSampler;

void main()
{
	float3 viewLine = pos - camPos;
	outPosition = float4(pos, length(viewLine));

	float3 normal = (sqrt(texture(nrmSampler, UVs).rgb) - 0.5) * 2.0;

	float3 view = normalize(viewLine);

	float3 reflectionVector = reflect(view, nrm);

	float3 binormal = cross(nrm, tangent);

	float3 worldNormal = TangentToWorld(tangent, binormal, nrm, normal);

	float rgh = 1.0;

	outNormal = float4(worldNormal, rgh);

	float4 col = texture(texSampler, UVs);

	float3 reflection = CubeLod(cubeSampler, reflectionVector, rgh).rgb;

	reflection = Desaturate(reflection, 0.4f) * Fresnel(nrm, view, 3.0f) * 0.6f;

	outGI = texture(aoSampler, lightmapUV);
	outColor = float4(col.rgb + reflection, 1);
}