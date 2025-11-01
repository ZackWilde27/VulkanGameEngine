#pragma once
#include "engineTypes.h"

float4x4 RotateMatrix(float4x4& matrix, float3& rotation);
float4x4 RotationMatrix(float3& rotation);

float4x4 WorldMatrix(float3& pos);
float4x4 WorldMatrix(float3& pos, float3& scale);
float4x4 WorldMatrix(float3&pos, float3& rot, float3& scale);

bool HitBoundingBox(float3& minB, float3& maxB, float3& origin, float3& dir, float3& coord);
bool HitMesh(float3& rayOrigin, float3& rayDir, Mesh* mesh, double& out_distance, float3& out_normal);
VkBool32 intersect_triangle(float3 orig, float3 dir, float3 vert0, float3 vert1, float3 vert2, double* t, float3* normal);