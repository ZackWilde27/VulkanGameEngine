#include "luaVectorLib.h"
#include "backendUtils.h"

static int LuaFN_Float2Index(lua_State* L)
{
	auto vec = LuaData<float2>(L, 1);
	lua_pushnumber(L, GetFloat2Channel(vec, lua_tostring(L, 2)));
	return 1;
}

static int LuaFN_Float2Add(lua_State* L)
{
	LuaBasicOperator(float2, +, Lua_PushFloat2_idx);
}

static int LuaFN_Float2Sub(lua_State* L)
{
	LuaBasicOperator(float2, -, Lua_PushFloat2_idx);
}

static int LuaFN_Float2Mul(lua_State* L)
{
	LuaOptionalNumberOperator(float2, *, Lua_PushFloat2_idx);
}

static int LuaFN_Float2Div(lua_State* L)
{
	LuaOptionalNumberOperator(float2, / , Lua_PushFloat2_idx);
}

static int LuaFN_Float2Eq(lua_State* L)
{
	auto v1 = LuaData<float2>(L, 1);
	auto v2 = LuaData<float2>(L, 1);

	lua_pushboolean(L, *v1 == *v2);

	return 1;
}

static int LuaFN_Float2Neg(lua_State* L)
{
	auto v1 = LuaData<float2>(L, 1);
	auto v3 = Lua_New(float2);
	*v3 = -(*v1);
	Lua_PushFloat2_idx(L, 2);

	return 1;
}

static int LuaFN_Float2Str(lua_State* L)
{
	auto v = LuaData<float2>(L, 1);
	char buffer[256];
	ZEROMEM(buffer, 256);

	sprintf(buffer, "{ %f, %f }", v->x, v->y);
	lua_pushstring(L, buffer);

	return 1;
}

void Lua_SetFloat2Metatable(lua_State* L)
{
	lua_createtable(L, 0, 9);

	lua_pushcclosure(L, LuaFN_Float2NewIndex<float2>, 0);
	lua_setfield(L, -2, "__newindex");

	lua_pushcclosure(L, LuaFN_Float2Index, 0);
	lua_setfield(L, -2, "__index");

	lua_pushcclosure(L, LuaFN_Float2Add, 0);
	lua_setfield(L, -2, "__add");

	lua_pushcclosure(L, LuaFN_Float2Sub, 0);
	lua_setfield(L, -2, "__sub");

	lua_pushcclosure(L, LuaFN_Float2Mul, 0);
	lua_setfield(L, -2, "__mul");

	lua_pushcclosure(L, LuaFN_Float2Div, 0);
	lua_setfield(L, -2, "__div");

	lua_pushcclosure(L, LuaFN_Float2Eq, 0);
	lua_setfield(L, -2, "__eq");

	lua_pushcclosure(L, LuaFN_Float2Neg, 0);
	lua_setfield(L, -2, "__unm");

	lua_pushcclosure(L, LuaFN_Float2Str, 0);
	lua_setfield(L, -2, "__tostring");

	lua_setmetatable(L, -2);
}

void Lua_PushFloat2(lua_State* L, float2* v)
{
	lua_createtable(L, 0, 2);

	lua_pushlightuserdata(L, v);
	lua_setfield(L, -2, LUA_DATA_NAME);

	lua_pushstring(L, "f2");
	lua_setfield(L, -2, "LGETYPE");

	Lua_SetFloat2Metatable(L);
}

void Lua_PushFloat2_idx(lua_State* L, int idx)
{
	lua_createtable(L, 0, 2);

	lua_rotate(L, idx, -1);
	lua_setfield(L, -2, LUA_DATA_NAME);

	lua_pushstring(L, "f2");
	lua_setfield(L, -2, "LGETYPE");

	Lua_SetFloat2Metatable(L);
}


int LuaFN_NewFloat2(lua_State* L)
{
	auto vec = Lua_New(float2);
	int top = lua_gettop(L);

	if (top == 2)
		*vec = float2((float)lua_tonumber(L, 1));
	else
		*vec = float2((float)lua_tonumber(L, 1), (float)lua_tonumber(L, 2));

	Lua_PushFloat2_idx(L, top);

	return 1;
}

static float& Lua_GetFloat3Channel(float3* vec, const char* key)
{
	switch (*key)
	{
	case 'x':
	case 'r':
		return vec->x;

	case 'y':
	case 'g':
		return vec->y;

	default:
		return vec->z;
	}
}

static int LuaFN_Float3NewIndex(lua_State* L)
{
	auto vec = LuaData<float3>(L, 1);
	Lua_GetFloat3Channel(vec, lua_tolstring(L, 2, NULL)) = (float)lua_tonumber(L, 3);
	return 0;
}

static int LuaFN_Float3Index(lua_State* L)
{
	auto vec = LuaData<float3>(L, 1);
	lua_pushnumber(L, Lua_GetFloat3Channel(vec, lua_tolstring(L, 2, NULL)));
	return 1;
}

static int LuaFN_Float3Add(lua_State* L)
{
	LuaBasicOperator(float3, +, Lua_PushFloat3_idx);
}

static int LuaFN_Float3Sub(lua_State* L)
{
	LuaBasicOperator(float3, -, Lua_PushFloat3_idx);
}

static int LuaFN_Float3Mul(lua_State* L)
{
	LuaOptionalNumberOperator(float3, *, Lua_PushFloat3_idx);
}

static int LuaFN_Float3Div(lua_State* L)
{
	LuaOptionalNumberOperator(float3, / , Lua_PushFloat3_idx);
}

static int LuaFN_Float3Eq(lua_State* L)
{
	float3* x, * y;
	Lua_GetFloat3_2(L, x, y);

	lua_pushboolean(L, *x == *y);
	return 1;
}

static int LuaFN_Float3Neg(lua_State* L)
{
	auto vec = Lua_New(float3);

	*vec = -*LuaData<float3>(L, 1);

	Lua_PushFloat3_idx(L, 2);
	return 1;
}

static int LuaFN_Float3Str(lua_State* L)
{
	auto v = LuaData<float3>(L, 1);
	char buffer[256];
	ZEROMEM(buffer, 256);

	sprintf(buffer, "{ %f, %f, %f }", v->x, v->y, v->z);
	lua_pushstring(L, buffer);

	return 1;
}

void Lua_SetFloat3Metatable(lua_State* L)
{
	lua_createtable(L, 0, 9);

	lua_pushcclosure(L, LuaFN_Float3NewIndex, 0);
	lua_setfield(L, -2, "__newindex");

	lua_pushcclosure(L, LuaFN_Float3Index, 0);
	lua_setfield(L, -2, "__index");

	lua_pushcclosure(L, LuaFN_Float3Add, 0);
	lua_setfield(L, -2, "__add");

	lua_pushcclosure(L, LuaFN_Float3Sub, 0);
	lua_setfield(L, -2, "__sub");

	lua_pushcclosure(L, LuaFN_Float3Mul, 0);
	lua_setfield(L, -2, "__mul");

	lua_pushcclosure(L, LuaFN_Float3Div, 0);
	lua_setfield(L, -2, "__div");

	lua_pushcclosure(L, LuaFN_Float3Eq, 0);
	lua_setfield(L, -2, "__eq");

	lua_pushcclosure(L, LuaFN_Float3Neg, 0);
	lua_setfield(L, -2, "__unm");

	lua_pushcclosure(L, LuaFN_Float3Str, 0);
	lua_setfield(L, -2, "__tostring");

	lua_setmetatable(L, -2);
}

void Lua_PushFloat3_idx(lua_State* L, int index)
{
	lua_createtable(L, 0, 1);

	lua_rotate(L, index, -1);
	lua_setfield(L, -2, LUA_DATA_NAME);

	lua_pushstring(L, "f3");
	lua_setfield(L, -2, "LGETYPE");

	Lua_SetFloat3Metatable(L);
}

void Lua_PushFloat3(lua_State* L, float3* data)
{
	lua_createtable(L, 0, 1);
	lua_pushlightuserdata(L, data);
	lua_setfield(L, -2, LUA_DATA_NAME);

	lua_pushstring(L, "f3");
	lua_setfield(L, -2, "LGETYPE");

	Lua_SetFloat3Metatable(L);
}

int LuaFN_NewFloat3(lua_State* L)
{
	auto vec = Lua_New(float3);
	int top = lua_gettop(L);

	switch (top)
	{
	case 2:
		*vec = float3((float)lua_tonumber(L, 1));
		break;

	case 3:
		*vec = float3(*LuaData<float2>(L, 1), (float)lua_tonumber(L, 2));
		break;

	default:
		*vec = float3((float)lua_tonumber(L, 1), (float)lua_tonumber(L, 2), (float)lua_tonumber(L, 3));
		break;
	}

	Lua_PushFloat3_idx(L, top);

	return 1;
}

static float& GetFloat4Channel(float4* vec, const char* channel)
{
	switch (*channel)
	{
	case 'x':
	case 'r':
		return vec->x;

	case 'y':
	case 'g':
		return vec->y;

	case 'z':
	case 'b':
		return vec->z;

	default:
		return vec->w;
	}
}

static int LuaFN_Float4NewIndex(lua_State* L)
{
	auto vec = LuaData<float4>(L, 1);

	GetFloat4Channel(vec, lua_tostring(L, 2)) = (float)lua_tonumber(L, 3);

	return 0;
}

static int LuaFN_Float4Index(lua_State* L)
{
	auto vec = LuaData<float4>(L, 1);

	lua_pushnumber(L, GetFloat4Channel(vec, lua_tostring(L, 2)));

	return 1;
}

static int LuaFN_Float4Add(lua_State* L)
{
	LuaBasicOperator(float4, +, Lua_PushFloat4_idx);
}

static int LuaFN_Float4Sub(lua_State* L)
{
	LuaBasicOperator(float4, -, Lua_PushFloat4_idx);
}

static int LuaFN_Float4Mul(lua_State* L)
{
	LuaOptionalNumberOperator(float4, *, Lua_PushFloat4_idx);
}

static int LuaFN_Float4Div(lua_State* L)
{
	LuaOptionalNumberOperator(float4, / , Lua_PushFloat4_idx);
}

static int LuaFN_Float4Eq(lua_State* L)
{
	auto v1 = LuaData<float4>(L, 1);
	auto v2 = LuaData<float4>(L, 1);

	lua_pushboolean(L, *v1 == *v2);

	return 1;
}

static int LuaFN_Float4Neg(lua_State* L)
{
	auto v1 = LuaData<float4>(L, 1);

	auto v3 = (float4*)lua_newuserdata(L, sizeof(float4));
	*v3 = -(*v1);

	Lua_PushFloat4_idx(L, 2);

	return 1;
}

static int LuaFN_Float4Str(lua_State* L)
{
	auto v = LuaData<float4>(L, 1);
	char buffer[256];
	ZEROMEM(buffer, 256);

	sprintf(buffer, "{ %f, %f, %f, %f }", v->x, v->y, v->z, v->w);
	lua_pushstring(L, buffer);

	return 1;
}

static void Lua_SetFloat4Metatable(lua_State* L)
{
	lua_createtable(L, 0, 9);

	lua_pushcclosure(L, LuaFN_Float4NewIndex, 0);
	lua_setfield(L, -2, "__newindex");

	lua_pushcclosure(L, LuaFN_Float4Index, 0);
	lua_setfield(L, -2, "__index");

	lua_pushcclosure(L, LuaFN_Float4Add, 0);
	lua_setfield(L, -2, "__add");

	lua_pushcclosure(L, LuaFN_Float4Sub, 0);
	lua_setfield(L, -2, "__sub");

	lua_pushcclosure(L, LuaFN_Float4Mul, 0);
	lua_setfield(L, -2, "__mul");

	lua_pushcclosure(L, LuaFN_Float4Div, 0);
	lua_setfield(L, -2, "__div");

	lua_pushcclosure(L, LuaFN_Float4Eq, 0);
	lua_setfield(L, -2, "__eq");

	lua_pushcclosure(L, LuaFN_Float4Neg, 0);
	lua_setfield(L, -2, "__unm");

	lua_pushcclosure(L, LuaFN_Float4Str, 0);
	lua_setfield(L, -2, "__tostring");

	lua_setmetatable(L, -2);
}

void Lua_PushFloat4(lua_State* L, float4* v)
{
	lua_createtable(L, 0, 2);

	lua_pushlightuserdata(L, v);
	lua_setfield(L, -2, LUA_DATA_NAME);

	lua_pushstring(L, "f4");
	lua_setfield(L, -2, "LGETYPE");

	Lua_SetFloat4Metatable(L);
}

void Lua_PushFloat4_idx(lua_State* L, int idx)
{
	lua_createtable(L, 0, 2);

	lua_rotate(L, idx, -1);
	lua_setfield(L, -2, LUA_DATA_NAME);

	lua_pushstring(L, "f4");
	lua_setfield(L, -2, "LGETYPE");

	Lua_SetFloat4Metatable(L);
}

int LuaFN_NewFloat4(lua_State* L)
{
	auto vec = Lua_New(float4);
	int top = lua_gettop(L);

	switch (top)
	{
	case 2:
		*vec = float4((float)lua_tonumber(L, 1));
		break;

	case 3:
		if (Lua_IsLGEType(L, 1, "f2"))
		{
			lua_getfield(L, 1, LUA_DATA_NAME);
			lua_getfield(L, 2, LUA_DATA_NAME);
			*vec = float4(*(float2*)lua_touserdata(L, -2), *(float2*)lua_touserdata(L, -1));
			lua_pop(L, 2);
		}
		else
			*vec = float4(*LuaData<float3>(L, 1), lua_tonumber(L, 2));

		break;

	default:
		*vec = float4(lua_tonumber(L, 1), lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4));
		break;
	}


	Lua_PushFloat4_idx(L, top);

	return 1;
}

static int LuaFN_Float4x4Mul(lua_State* L)
{
	auto mat = LuaData<float4x4>(L, 1);

	if (lua_isnumber(L, 2))
	{
		float4x4* newMatrix = Lua_New(float4x4);
		*newMatrix = (*mat) * (float)lua_tonumber(L, 2);
		Lua_PushFloat4x4_idx(L, 3);
	}
	else
	{
		auto o = LuaData<void>(L, 2);

		const char* type = Lua_GetLGEType(L, 2);
		if (Lua_IsLGEType(type, "f4"))
		{
			float4* vec = Lua_New(float4);
			*vec = (*mat) * (*(float4*)o);
			Lua_PushFloat4_idx(L, 3);
		}
		else
		{
			float4x4* newMatrix = Lua_New(float4x4);
			*newMatrix = (*mat) * (*(float4x4*)o);
			Lua_PushFloat4x4_idx(L, 3);
		}
	}

	return 1;
}

static void Lua_PushFloat4x4_TheRest(lua_State* L)
{
	lua_pushstring(L, "m4");
	lua_setfield(L, -2, "LGETYPE");

	lua_createtable(L, 0, 1);

	lua_pushcclosure(L, LuaFN_Float4x4Mul, 0);
	lua_setfield(L, -2, "__mul");

	lua_setmetatable(L, -2);
}

void Lua_PushFloat4x4_idx(lua_State* L, int index)
{
	lua_createtable(L, 0, 2);

	lua_rotate(L, index, -1);
	lua_setfield(L, -2, LUA_DATA_NAME);

	Lua_PushFloat4x4_TheRest(L);
}

void Lua_PushFloat4x4(lua_State* L, float4x4* matrix)
{
	lua_createtable(L, 0, 2);
	lua_pushlightuserdata(L, matrix);
	lua_setfield(L, -2, LUA_DATA_NAME);

	Lua_PushFloat4x4_TheRest(L);
}

int LuaFN_WorldMatrix(lua_State* L)
{
	auto matrix = Lua_New(float4x4);
	*matrix = WorldMatrix(*LuaData<float3>(L, 1), *LuaData<float3>(L, 2), *LuaData<float3>(L, 3));
	Lua_PushFloat4x4_idx(L, 4);

	return 1;
}