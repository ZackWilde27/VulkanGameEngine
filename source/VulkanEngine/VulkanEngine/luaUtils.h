#pragma once
#include "engineTypes.h"

void* Lua_GetRawData(lua_State* L, int index);
float3* Lua_GetFloat3(lua_State* L, int index);
void Lua_GetFloat3_2(lua_State* L, float3*& v1, float3*& v2);