#pragma once
#include "luaUtils.h"

#define Lua_New(type) (type*)lua_newuserdata(L, sizeof(type))
#define GetFloat3(index) *(float3*)lua_touserdata(L, index)

void Lua_PushFloat3(lua_State* L, float3* data);
void Lua_PushFloat3_idx(lua_State* L, int index);

static int LuaFN_GLMNormalize(lua_State* L)
{
	auto vec = Lua_New(float3);
	*vec = glm::normalize(*Lua_GetFloat3(L, 1));

	Lua_PushFloat3_idx(L, 2);
	return 1;
}

static int LuaFN_GLMCross(lua_State* L)
{
	float3* x, * y;
	Lua_GetFloat3_2(L, x, y);

	auto vec = Lua_New(float3);
	*vec = glm::cross(*x, *y);

	Lua_PushFloat3_idx(L, 3);
	return 1;
}

static int LuaFN_GLMAbs(lua_State* L)
{
	auto vec = Lua_New(float3);
	*vec = glm::abs(*Lua_GetFloat3(L, 1));

	Lua_PushFloat3_idx(L, 2);
	return 1;
}

static int LuaFN_GLMDistance(lua_State* L)
{
	float3* x, * y;
	Lua_GetFloat3_2(L, x, y);

	lua_pushnumber(L, glm::distance(*x, *y));
	return 1;
}

static int LuaFN_GLMDot(lua_State* L)
{
	float3* x, * y;
	Lua_GetFloat3_2(L, x, y);

	lua_pushnumber(L, glm::dot(*x, *y));
	return 1;
}

static int LuaFN_GLMLength(lua_State* L)
{
	lua_pushnumber(L, glm::length(*Lua_GetFloat3(L, 1)));
	return 1;
}

static int LuaFN_GLMReflect(lua_State* L)
{
	float3* x, * y;
	Lua_GetFloat3_2(L, x, y);

	auto vec = Lua_New(float3);
	*vec = glm::reflect(*x, *y);

	Lua_PushFloat3_idx(L, 3);
	return 1;
}

static int LuaFN_GLMRefract(lua_State* L)
{
	float3* x, * y;
	Lua_GetFloat3_2(L, x, y);

	auto vec = Lua_New(float3);
	*vec = glm::refract(*x, *y, (float)lua_tonumber(L, 3));

	Lua_PushFloat3_idx(L, 4);
	return 1;
}



void Lua_AddGLMLib(lua_State* L)
{
	lua_createtable(L, 0, 8);

	LuaPushFuncField(LuaFN_GLMAbs, "abs");
	LuaPushFuncField(LuaFN_GLMCross, "cross");
	LuaPushFuncField(LuaFN_GLMDistance, "distance");
	LuaPushFuncField(LuaFN_GLMDot, "dot");
	LuaPushFuncField(LuaFN_GLMLength, "length");
	LuaPushFuncField(LuaFN_GLMNormalize, "normalize");
	LuaPushFuncField(LuaFN_GLMReflect, "reflect");
	LuaPushFuncField(LuaFN_GLMRefract, "refract");

	lua_setglobal(L, "glm");
}