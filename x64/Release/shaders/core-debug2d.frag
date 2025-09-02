#version 450

#include "zutils.glsl"
layout(location = 0) in float2 UVs;


layout(location = 0) out float4 outColour;
layout(location = 1) out float4 outNormal;
layout(location = 2) out float4 outPosition;
layout(location = 3) out float4 outGI;

layout(set = 0, binding = 1) uniform sampler2D samplerTex;

void main()
{
	float4 col = texture(samplerTex, UVs);
    if (col.a < 0.5) discard;

    outColour = col;
    outGI = float4(1);
}