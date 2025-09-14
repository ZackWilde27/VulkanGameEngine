#pragma once
#include "engineTypes.h"

float4x4 WorldMatrix(float3 pos, float3 rot, float3 scale);
bool HitBoundingBox(float3 minB, float3 maxB, float3 origin, float3 dir, float3& coord);