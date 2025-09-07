#pragma once
#include "luaUtils.h"

void* Lua_GetRawData(lua_State* L, int index)
{
	lua_getfield(L, index, "data");
	void* data = lua_touserdata(L, -1);
	lua_pop(L, 1);
	return data;
}

float3* Lua_GetFloat3(lua_State* L, int index)
{
	return (float3*)Lua_GetRawData(L, index);
}

void Lua_GetFloat3_2(lua_State* L, float3*& v1, float3*& v2)
{
	lua_getfield(L, 1, "data");
	lua_getfield(L, 2, "data");
	v1 = (float3*)lua_touserdata(L, -2);
	v2 = (float3*)lua_touserdata(L, -1);
	lua_pop(L, 2);
}