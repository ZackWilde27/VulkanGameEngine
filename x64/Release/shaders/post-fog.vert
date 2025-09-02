#version 450

#include "zutils.glsl"
layout(location = 0) out float2 UVs;


float2 positions[] = {
	float2(-1.0, -1.0),
	float2(-1.0, 3.0),
	float2(3.0, -1.0)
};

float2 uvs[] = {
	float2(0.0, 0.0),
	float2(0.0, 2.0),
	float2(2.0, 0.0)
};

void main()
{
	UVs =  uvs[gl_VertexIndex];
	gl_Position = float4(positions[gl_VertexIndex], 0.0f, 1.0f);
}