#version 450

#include "zutils.glsl"
layout(location = 0) out float4 outCol;
layout(location = 1) out float4 outNormal;
layout(location = 2) out float4 outPosition;
layout(location = 3) out float4 outGI;

layout(set = 0, binding = 1) uniform sampler2D aoSampler;

void main()
{
	outCol = float4(1, 0, 0, 1);
    outGI = float4(1);
}