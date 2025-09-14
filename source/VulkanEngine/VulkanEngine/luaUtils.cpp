#pragma once
#include "luaUtils.h"

float3* Lua_GetFloat3(lua_State* L, int index)
{
	LuaData(v, index, float3);
	return v;
}

void Lua_GetFloat3_2(lua_State* L, float3*& v1, float3*& v2)
{
	lua_getfield(L, 1, "data");
	lua_getfield(L, 2, "data");
	v1 = (float3*)lua_touserdata(L, -2);
	v2 = (float3*)lua_touserdata(L, -1);
	lua_pop(L, 2);
}