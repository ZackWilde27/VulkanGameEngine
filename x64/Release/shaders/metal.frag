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

	float3 normal = (texture(nrmSampler, UVs).rgb - 0.5) * 2;
	float3 view = normalize(viewLine);

	float3 binormal = cross(nrm, tangent);

	float3 worldNormal = TangentToWorld(-tangent, binormal, nrm, normal);

	float rgh = texture(rghSampler, UVs).r;

	outNormal = float4(worldNormal, rgh);

	float3 reflectionVector = reflect(view, worldNormal);

	float3 reflection = CubeLod(cubeSampler, reflectionVector, rgh).rgb;
	reflection = Desaturate(reflection, 0.3f) * 6.0f;

	reflection *= Fresnel(worldNormal, view, 3.0f);

	float3 col = texture(texSampler, UVs).rgb * reflection;

	//outColor = float4(instanceCol, 1); return;

	outGI = texture(aoSampler, lightmapUV);
	outColor = float4(col, 1);
}