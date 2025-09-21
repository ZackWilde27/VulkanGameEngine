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

const char* Lua_GetLGEType(lua_State* L, int index)
{
	lua_getfield(L, index, "LGETYPE");
	auto type = lua_tostring(L, -1);
	lua_pop(L, 1);

	return type;
}

bool Lua_IsLGEType(const char* type1, const char* type2)
{
	return *(unsigned short*)type1 == *(unsigned short*)type2;
}

bool Lua_IsLGEType(lua_State* L, int index, const char* type)
{
	lua_getfield(L, index, "LGETYPE");
	auto t = lua_tostring(L, -1);
	lua_pop(L, 1);

	// LGETYPEs can only be either 1 character or 2, so this compares the entire string in 1 comparison
	// As long as the 1 character types have a terminator there should be no problem with this method
	return Lua_IsLGEType(t, type);
}