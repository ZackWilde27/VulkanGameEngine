#version 450

#include "zutils.glsl"
layout(location = 0) in float2 UVs;


layout(set = 2, binding = 0) uniform sampler2D samplerColour;

void main()
{
	if (texture(samplerColour, UVs).a < 0.5)
		discard;
}