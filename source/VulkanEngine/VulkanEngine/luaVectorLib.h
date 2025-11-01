#pragma once
#include "engineTypes.h"
#include "luaUtils.h"

template<typename T>
static float& GetFloat2Channel(T* vec, const char* key)
{
	if (*key == 'x' || *key == 'r')
		return vec->x;

	return vec->y;
}

template<typename T>
static int LuaFN_Float2NewIndex(lua_State* L)
{
	auto vec = LuaData<T>(L, 1);
	GetFloat2Channel<T>(vec, lua_tostring(L, 2)) = lua_tonumber(L, 3);
	return 0;
}

void Lua_PushFloat2(lua_State* L, float2* v);
void Lua_PushFloat2_idx(lua_State* L, int idx);
void Lua_PushFloat3(lua_State* L, float3* data);
void Lua_PushFloat3_idx(lua_State* L, int index);
void Lua_PushFloat4(lua_State* L, float4* v);
void Lua_PushFloat4_idx(lua_State* L, int idx);
void Lua_PushFloat4x4(lua_State* L, float4x4* matrix);
void Lua_PushFloat4x4_idx(lua_State* L, int index);

int LuaFN_WorldMatrix(lua_State* L);