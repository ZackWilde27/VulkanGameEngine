#pragma once
#include "engineTypes.h"

float4x4 WorldMatrix(float3&pos, float3& rot, float3& scale);
bool HitBoundingBox(float3& minB, float3& maxB, float3& origin, float3& dir, float3& coord);
bool HitPlane(float3& rayOrigin, float3& rayDir, float3& planeOrigin, float3& planeNormal, float& outDistance);
bool HitTriangle(float3& rayOrigin, float3& rayDir, float3 p1, float3 p2, float3 p3, float& outDistance);