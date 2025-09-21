#pragma once
#include "luaUtils.h"
#include "luafunctions.h"

#define LuaGLMSingleton(type, glmfunc, pushfunc)	auto vec = Lua_New(type); \
																			LuaData(v, 1, type); \
																			*vec = glmfunc(*v); \
																			pushfunc(L, 2)

#define LuaGLMSingletonFloat(type, glmfunc, pushfunc) LuaData(v, 1, type); \
																				  lua_pushnumber(L, glmfunc(*v))

#define LuaGLM2(type, glmfunc, pushfunc) auto vec = Lua_New(type); \
																LuaData(v1, 1, type); \
																LuaData(v2, 2, type); \
																*vec = glmfunc(*v1, *v2); \
																pushfunc(L, 3)

#define LuaGLM2Float(type, glmfunc, pushfunc)	LuaData(v1, 1, type); \
																		LuaData(v2, 2, type); \
																		lua_pushnumber(L, glmfunc(*v1, *v2))

#define LuaGLMMultiVec234(macro, glmfunc)	const char* vecType = Lua_GetLGEType(L, 1); \
																	if (Lua_IsLGEType(vecType, "f2")) { \
																		macro(float2, glmfunc, Lua_PushFloat2_idx); \
																	} \
																	else if (Lua_IsLGEType(vecType, "f3")) { \
																		macro(float3, glmfunc, Lua_PushFloat3_idx); \
																	} \
																	else { \
																		macro(float4, glmfunc, Lua_PushFloat4_idx); \
																	} \
																	return 1

#define LuaGLMMultiVec34(macro, glmfunc)		const char* vecType = Lua_GetLGEType(L, 1); \
																	if (Lua_IsLGEType(vecType, "f3")) { \
																		macro(float3, glmfunc, Lua_PushFloat3_idx); \
																	} \
																	else { \
																		macro(float4, glmfunc, Lua_PushFloat4_idx); \
																	} \
																	return 1

static int LuaFN_GLMNormalize(lua_State* L)
{
	LuaGLMMultiVec234(LuaGLMSingleton, glm::normalize);
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
	LuaGLMMultiVec234(LuaGLMSingleton, glm::abs);
}

static int LuaFN_GLMDistance(lua_State* L)
{
	LuaGLMMultiVec234(LuaGLM2Float, glm::distance);
}

static int LuaFN_GLMDot(lua_State* L)
{
	LuaGLMMultiVec234(LuaGLM2Float, glm::dot);
}

static int LuaFN_GLMLength(lua_State* L)
{
	LuaGLMMultiVec234(LuaGLMSingletonFloat, glm::length);
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

static int LuaFN_GLMInverse(lua_State* L)
{
	LuaData(mat, 1, float4x4);

	auto newMatrix = Lua_New(float4x4);
	*newMatrix = glm::inverse(*mat);

	Lua_PushFloat4x4_idx(L, 2);
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

	LuaPushFuncField(LuaFN_GLMInverse, "inverse");

	lua_setglobal(L, "glm");
}