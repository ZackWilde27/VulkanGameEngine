#version 450

#include "zutils.glsl"
const float size = 0.1;

float2 coordinates[] = {
    float2(-size, -size),
    float2(size, -size),
    float2(size, size),

    float2(-size, -size),
    float2(size, size),
    float2(-size, size)
};

float2 uvs[] = {
    float2(0, 0),
    float2(1, 0),
    float2(1, 1),

    float2(0, 0),
    float2(1, 1),
    float2(0, 1)
};

layout(set = 0, binding = 0) uniform UniformBufferObject {
    float4x4 viewProj;
    float2 debugPoint;
} ubo;


layout(location = 0) out float2 UVs;

void main()
{
	gl_Position = float4(coordinates[gl_VertexIndex] + ubo.debugPoint, 0, 1);
    UVs = uvs[gl_VertexIndex];
}