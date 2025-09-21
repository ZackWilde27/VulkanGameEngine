#pragma once
#include "luafunctions.h"
#include "engine.h"
#include "luaUtils.h"
#include "backendUtils.h"
#include <iostream>

// A shortcut for getting the length and popping the value off the stack afterwards
int Lua_Len(lua_State* L, int idx)
{
	lua_len(L, idx);
	int length = lua_tointeger(L, -1);
	lua_pop(L, 1);
	return length;
}

int IntFromTable(lua_State* L, int tableDex, int intDex, const char* description)
{
	lua_geti(L, tableDex, intDex);

	if (lua_type(L, -1) != LUA_TNUMBER)
		std::cout << description << " is not a number!\n";

	int result = luaL_checkinteger(L, -1);
	lua_pop(L, 1);
	return result;
}

int IntFromTable_Default(lua_State* L, int tableDex, int intDex, int defaultVal)
{
	lua_geti(L, tableDex, intDex);

	int result;

	if (lua_type(L, -1) != LUA_TNUMBER)
		result = defaultVal;
	else
		result = luaL_checkinteger(L, -1);

	lua_pop(L, 1);
	return result;
}

float GetFloatFromTable(lua_State* L, int tableDex, int floatDex)
{
	lua_geti(L, tableDex, floatDex);
	float result = lua_tonumber(L, -1);
	lua_pop(L, 1);
	return result;
}

void* GetUDataFromTable(lua_State* L, int tableDex, int dataDex)
{
	char buffer[256];
	lua_geti(L, tableDex, dataDex);
	int type = lua_type(L, -1);
	if (type != LUA_TLIGHTUSERDATA)
	{
		ZEROMEM(buffer, 256);
		sprintf(buffer, "Argument #%i (%i) is not userdata (it\'s %i)!", tableDex, dataDex, type);
		throw std::runtime_error(buffer);
	}
	void* result = lua_touserdata(L, -1);
	lua_pop(L, 1);
	return result;
}

const char* GetStringFromTable(lua_State* L, int tableDex, int stringDex)
{
	const char* result;
	lua_geti(L, tableDex, stringDex);

	if (lua_type(L, -1) == LUA_TNIL)
		result = NULL;
	else
		result = lua_tostring(L, -1);

	lua_pop(L, 1);
	return result;
}

bool GetBoolFromTable(lua_State* L, int tableDex, int boolDex)
{
	lua_geti(L, tableDex, boolDex);
	bool result = lua_toboolean(L, -1);
	lua_pop(L, 1);
	return result;
}

static int LuaFN_Float4x4Mul(lua_State* L)
{
	LuaData(mat, 1, float4x4);

	if (lua_isnumber(L, 2))
	{
		float4x4* newMatrix = Lua_New(float4x4);
		*newMatrix = (*mat) * (float)lua_tonumber(L, 2);
		Lua_PushFloat4x4_idx(L, 3);
	}
	else
	{
		LuaData(o, 2, void);

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
	lua_setfield(L, -2, "data");

	Lua_PushFloat4x4_TheRest(L);
}

void Lua_PushFloat4x4(lua_State* L, float4x4* matrix)
{
	lua_createtable(L, 0, 2);
	lua_pushlightuserdata(L, matrix);
	lua_setfield(L, -2, "data");

	Lua_PushFloat4x4_TheRest(L);
}

void Lua_PushTexture_NoGC(lua_State* L, Texture* tex, int width, int height)
{
	lua_createtable(L, 0, 3);

	lua_pushlightuserdata(L, tex);
	lua_setfield(L, -2, "texture");

	lua_pushinteger(L, width);
	lua_setfield(L, -2, "width");

	lua_pushinteger(L, height);
	lua_setfield(L, -2, "height");

	lua_pushinteger(L, tex->Aspect);
	lua_setfield(L, -2, "aspect");
}

static void Lua_PushTexture(lua_State* L, Texture* tex, int width, int height)
{
	Lua_PushTexture_NoGC(L, tex, width, height);

	lua_createtable(L, 0, 1);
	lua_pushcclosure(L, Lua_TextureGC, 0);
	lua_setfield(L, -2, "__gc");
	lua_setmetatable(L, -2);
}

static int Lua_ThingNewIndex(lua_State* L)
{
	LuaData(mo, 1, Thing);

	const char* key = lua_tolstring(L, 2, NULL);

	switch (*key)
	{
		case 'p':
			mo->position = *Lua_GetFloat3(L, 3);
			break;

		case 'r':
			mo->rotation = *Lua_GetFloat3(L, 3);
			break;

		default:
			mo->scale = *Lua_GetFloat3(L, 3);
			break;
	}	

	mo->UpdateMatrix();

	return 0;
}

unsigned short idAsShort =  'i' | ('d' << 8);

static int LuaFN_MaterialListIndex(lua_State* L)
{
	auto list = (std::vector<Material*>*)lua_touserdata(L, 1);

	int index = atoi(lua_tostring(L, 2));

	lua_pushlightuserdata(L, (*list)[index]);
	return 1;
}

static int LuaFN_MaterialListNewIndex(lua_State* L)
{
	auto list = (std::vector<Material*>*)lua_touserdata(L, 1);

	int index = atoi(lua_tostring(L, 2));

	(*list)[index] = (Material*)lua_touserdata(L, 3);

	return 0;
}

static void Lua_PushMaterialList(lua_State* L, Thing* mo)
{
	lua_pushlightuserdata(L, &mo->materials);
	
	lua_createtable(L, 0, 2);
	lua_pushcclosure(L, LuaFN_MaterialListIndex, 0);
	lua_setfield(L, -1, "__index");

	lua_pushcclosure(L, LuaFN_MaterialListNewIndex, 0);
	lua_setfield(L, -1, "__newindex");
	lua_setmetatable(L, -2);
}

static int Lua_ThingIndex(lua_State* L)
{
	LuaData(mo, 1, Thing);

	const char* key = lua_tolstring(L, 2, NULL);

	// To make indexing as fast as possible, it compares as few letters as possible
	// This does mean that 'thing.potato' is the same as 'thing.position', but that's probably worth it for the efficiency
	switch (*key)
	{
	case 'p':
		Lua_PushFloat3(L, &mo->position);
		break;

	case 'r':
		Lua_PushFloat3(L, &mo->rotation);
		break;

	case 's':
		Lua_PushFloat3(L, &mo->scale);
		break;

	case 'i':
		if (*(key + 1) == 'd')
			lua_pushinteger(L, mo->id);
		else
			lua_pushboolean(L, mo->isStatic);
		break;

	default:
		if (*(key + 3) == 'r')
		{
			auto matrix = Lua_New(float4x4);
			*matrix = WorldMatrix(mo->position, mo->rotation, mo->scale);
			Lua_PushFloat4x4_idx(L, 3);
		}
		else
			Lua_PushMaterialList(L, mo);
		break;
	}

	return 1;
}

static int Lua_ThingEq(lua_State* L)
{
	LuaData(mo1, 1, Thing);
	LuaData(mo2, 2, Thing);

	lua_pushboolean(L, mo1 == mo2);
	return 1;
}

static int LuaFN_ThingAttachThing(lua_State* L)
{
	auto self = (Thing*)lua_touserdata(L, lua_upvalueindex(1));
	LuaData(attached, 1, Thing);

	attached->parent = self;
	self->children.push_back(attached);

	return 0;
}

void Lua_PushThing(lua_State* L, Thing* mo)
{
	lua_createtable(L, 0, 2);

	lua_pushlightuserdata(L, mo);
	lua_setfield(L, -2, "data");

	lua_pushlightuserdata(L, mo);
	lua_pushcclosure(L, LuaFN_ThingAttachThing, 1);
	lua_setfield(L, -2, "AttachThing");

	lua_createtable(L, 0, 3);
	lua_pushcclosure(L, Lua_ThingNewIndex, 0);
	lua_setfield(L, -2, "__newindex");
	lua_pushcclosure(L, Lua_ThingIndex, 0);
	lua_setfield(L, -2, "__index");
	lua_pushcclosure(L, Lua_ThingEq, 0);
	lua_setfield(L, -2, "__eq");
	lua_setmetatable(L, -2);
}

int LuaFN_TraceRay(lua_State* L)
{
	float3* rayStart, * rayEnd;
	Lua_GetFloat3_2(L, rayStart, rayEnd);

	int id = lua_tointeger(L, 3);
	float3 rayDir = glm::normalize(*rayEnd - *rayStart);

	Thing* outThing;
	float outDist;

	bool hit = RayObjects(*rayStart, rayDir, id, &outThing, &outDist);

	lua_createtable(L, 0, 1);

	lua_pushboolean(L, hit);
	lua_setfield(L, -2, "hit");

	if (hit)
	{
		Lua_PushThing(L, outThing);
		lua_setfield(L, -2, "object");

		lua_pushnumber(L, outDist);
		lua_setfield(L, -2, "distance");
	}

	return 1;
}


int LuaFN_MoveThingTo(lua_State* L)
{
	LuaData(mo, 1, Thing);
	auto moveTo = *Lua_GetFloat3(L, 2);
	auto moveSpeed = lua_tonumber(L, 3);
	auto callback = lua_tostring(L, 4);

	AddMovingThing(mo, moveTo, moveSpeed, callback);
	return 0;
}

int LuaFN_CreateImage(lua_State* L)
{
	auto tex = NEW(Texture);

	int width = (int)lua_tonumber(L, 4);
	int height = (int)lua_tonumber(L, 5);
	int mipLevels = lua_tointeger(L, 6);

	VkImageAspectFlags aspect = lua_tointeger(L, 11);

	FullCreateImage((VkImageType)lua_tointeger(L, 1), (VkImageViewType)lua_tointeger(L, 2), (VkFormat)lua_tointeger(L, 3), width, height, mipLevels, lua_tointeger(L, 7), (VkSampleCountFlagBits)lua_tointeger(L, 8), (VkImageTiling)lua_tointeger(L, 9), (VkImageUsageFlags)lua_tointeger(L, 10), aspect, (VkFilter)lua_tointeger(L, 12), (VkFilter)lua_tointeger(L, 13), (VkSamplerAddressMode)lua_tointeger(L, 14), tex, false);

	Lua_PushTexture(L, tex, width, height);
	return 1;
}

constexpr float HALFPI = 3.14159 / 2;

int LuaFN_DirectionFromAngle(lua_State* L)
{
	lua_Number angle = lua_tonumber(L, 1) + HALFPI;

	auto vec = (float3*)lua_newuserdata(L, sizeof(float3));
	*vec = float3(sin(angle), cos(angle), 0.0);

	Lua_PushFloat3_idx(L, 2);
	return 1;
}

int LuaFN_GetThingsInRadius(lua_State* L)
{
	auto pos = Lua_GetFloat3(L, 1);
	auto radius = lua_tonumber(L, 2);
	auto id = lua_tonumber(L, 3);

	size_t numThings;
	Thing** list = GetThingList(numThings);

	lua_createtable(L, 0, 0);

	int index = 1;

	for (size_t i = 0; i < numThings; i++)
	{
		if (list[i]->id == id)
		{
			if (glm::distance(list[i]->position, *pos) < radius)
			{
				Lua_PushThing(L, list[i]);
				lua_seti(L, -2, index++);
			}
		}
	}

	return 1;
}

int LuaFN_GetAllThingsInRadius(lua_State* L)
{
	auto pos = Lua_GetFloat3(L, 1);
	auto radius = lua_tonumber(L, 2);

	size_t numThings;
	Thing** list = GetThingList(numThings);

	lua_createtable(L, 0, 0);

	int index = 1;

	for (size_t i = 0; i < numThings; i++)
	{
		if (glm::distance(list[i]->position, *pos) < radius)
		{
			Lua_PushThing(L, list[i]);
			lua_seti(L, -2, index++);
		}
	}

	return 1;
}

int LuaFN_GetThingsById(lua_State* L)
{
	int id = lua_tointeger(L, 1);
	int index = 1;

	size_t numThings;
	Thing** ptr = GetThingList(numThings);

	lua_createtable(L, 0, 0);

	for (size_t i = 0; i < numThings; i++)
	{
		if (ptr[i]->id == id)
		{
			Lua_PushThing(L, ptr[i]);
			lua_seti(L, -2, index++);
		}
	}

	return 1;
}

static float& GetFloat2Channel(float2* vec, const char* key)
{
	if (*key == 'x' || *key == 'r')
		return vec->x;

	return vec->y;
}

static int LuaFN_Float2NewIndex(lua_State* L)
{
	LuaData(vec, 1, float2);
	GetFloat2Channel(vec, lua_tostring(L, 2)) = lua_tonumber(L, 3);
	return 0;
}

static int LuaFN_Float2Index(lua_State* L)
{
	LuaData(vec, 1, float2);
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
	LuaOptionalNumberOperator(float2, /, Lua_PushFloat2_idx);
}

static int LuaFN_Float2Eq(lua_State* L)
{
	LuaData(v1, 1, float2);
	LuaData(v2, 2, float2);

	lua_pushboolean(L, *v1 == *v2);

	return 1;
}

static int LuaFN_Float2Neg(lua_State* L)
{
	LuaData(v1, 1, float2);
	auto v3 = Lua_New(float2);
	*v3 = -(*v1);
	Lua_PushFloat2_idx(L, 2);

	return 1;
}

static int LuaFN_Float2Str(lua_State* L)
{
	LuaData(v, 1, float2);
	char buffer[256];
	ZEROMEM(buffer, 256);

	sprintf(buffer, "{ %f, %f }", v->x, v->y);
	lua_pushstring(L, buffer);

	return 1;
}

void Lua_SetFloat2Metatable(lua_State* L)
{
	lua_createtable(L, 0, 9);

	lua_pushcclosure(L, LuaFN_Float2NewIndex, 0);
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

void Lua_PushFloat2_idx(lua_State* L, int idx)
{
	lua_createtable(L, 0, 1);

	lua_rotate(L, idx, -1);
	lua_setfield(L, -2, "data");

	lua_pushstring(L, "f2");
	lua_setfield(L, -2, "LGETYPE");

	Lua_SetFloat2Metatable(L);
}


int LuaFN_NewFloat2(lua_State* L)
{
	auto vec = Lua_New(float2);
	int top = lua_gettop(L);

	if (top == 2)
		*vec = float2(lua_tonumber(L, 1));
	else
		*vec = float2(lua_tonumber(L, 1), lua_tonumber(L, 2));
		
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
	LuaData(vec, 1, float3);
	Lua_GetFloat3Channel(vec, lua_tolstring(L, 2, NULL)) = lua_tonumber(L, 3);
	return 0;
}

static int LuaFN_Float3Index(lua_State* L)
{
	LuaData(vec, 1, float3);
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
	LuaOptionalNumberOperator(float3, /, Lua_PushFloat3_idx);
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

	*vec = -(*Lua_GetFloat3(L, 1));

	Lua_PushFloat3_idx(L, 2);
	return 1;
}

static int LuaFN_Float3Str(lua_State* L)
{
	LuaData(v, 1, float3);
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
	lua_setfield(L, -2, "data");

	lua_pushstring(L, "f3");
	lua_setfield(L, -2, "LGETYPE");

	Lua_SetFloat3Metatable(L);
}

void Lua_PushFloat3(lua_State* L, float3* data)
{
	lua_createtable(L, 0, 1);
	lua_pushlightuserdata(L, data);
	lua_setfield(L, -2, "data");

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
			*vec = float3(lua_tonumber(L, 1));
			break;

		case 3:
			lua_getfield(L, 1, "data");
			*vec = float3(*(float2*)lua_touserdata(L, -1), lua_tonumber(L, 2));
			lua_pop(L, 1);
			break;

		default:
			*vec = float3(lua_tonumber(L, 1), lua_tonumber(L, 2), lua_tonumber(L, 3));
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
	LuaData(vec, 1, float4);

	GetFloat4Channel(vec, lua_tostring(L, 2)) = lua_tonumber(L, 3);

	return 0;
}

static int LuaFN_Float4Index(lua_State* L)
{
	LuaData(vec, 1, float4);

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
	LuaOptionalNumberOperator(float4, /, Lua_PushFloat4_idx);
}

static int LuaFN_Float4Eq(lua_State* L)
{
	LuaData(v1, 1, float4);
	LuaData(v2, 2, float4);

	lua_pushboolean(L, *v1 == *v2);

	return 1;
}

static int LuaFN_Float4Neg(lua_State* L)
{
	LuaData(v1, 1, float4);

	auto v3 = (float4*)lua_newuserdata(L, sizeof(float4));
	*v3 = -(*v1);

	Lua_PushFloat4_idx(L, 2);

	return 1;
}

static int LuaFN_Float4Str(lua_State* L)
{
	LuaData(v, 1, float4);
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

void Lua_PushFloat4_idx(lua_State* L, int idx)
{
	lua_createtable(L, 0, 1);

	lua_rotate(L, idx, -1);
	lua_setfield(L, -2, "data");

	lua_pushstring(L, "f4");
	lua_setfield(L, -2, "LGETYPE");

	Lua_SetFloat4Metatable(L);
}

int LuaFN_NewFloat4(lua_State* L)
{
	const char* lgeType;

	auto vec = Lua_New(float4);
	int top = lua_gettop(L);

	switch (top)
	{
		case 2:
			*vec = float4(lua_tonumber(L, 1));
			break;

		case 3:
			if (Lua_IsLGEType(L, 1, "f2"))
			{
				lua_getfield(L, 1, "data");
				lua_getfield(L, 2, "data");
				*vec = float4(*(float2*)lua_touserdata(L, -2), *(float2*)lua_touserdata(L, -1));
				lua_pop(L, 2);
			}
			else
			{
				lua_getfield(L, 1, "data");
				*vec = float4(*(float3*)lua_touserdata(L, -1), lua_tonumber(L, 2));
				lua_pop(L, 1);
			}
			break;

		default:
			*vec = float4(lua_tonumber(L, 1), lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4));
			break;
	}
		

	Lua_PushFloat4_idx(L, top);

	return 1;
}

static int LuaFN_CameraIndex(lua_State* L)
{
	lua_getfield(L, 1, "data");
	Camera* cam = (Camera*)lua_touserdata(L, -1);
	lua_pop(L, 1);
	const char* key = lua_tolstring(L, 2, NULL);

	switch (*key)
	{
	case 'p':
		Lua_PushFloat3(L, &cam->position);
		break;

	case 'm':
		Lua_PushFloat4x4(L, &cam->matrix);
		break;

	case 'v':
		Lua_PushFloat4x4(L, &cam->viewMatrix);
		break;

	case 'u':
		Lua_PushFloat3(L, &cam->up);
		break;

	default:
		Lua_PushFloat3(L, &cam->target);
	}

	return 1;
}

static int LuaFN_CameraNewIndex(lua_State* L)
{
	lua_getfield(L, 1, "data");
	Camera* cam = (Camera*)lua_touserdata(L, -1);
	lua_pop(L, 1);
	const char* key = lua_tolstring(L, 2, NULL);

	float x, y, z;

	lua_getfield(L, 3, "data");
	float3* vec = (float3*)lua_touserdata(L, -1);
	lua_pop(L, 1);

	// For speed, it only compares the first character to know which field you are talking about
	switch (*key)
	{
	case 'p':
		cam->position = *vec;
		break;

	case 'u':
		cam->up = *vec;
		break;

	default:
		cam->target = *vec;
	}

	return 0;
}

static int LuaFN_CameraTargetFromRotation(lua_State* L)
{
	Camera* cam = (Camera*)lua_touserdata(L, lua_upvalueindex(1));

	cam->TargetFromRotation(lua_tonumber(L, 1), lua_tonumber(L, 2));
	return 0;
}

static int LuaFN_CameraAttachThing(lua_State* L)
{
	auto cam = (Camera*)lua_touserdata(L, lua_upvalueindex(1));
	LuaData(thing, 1, Thing);

	cam->attachedThings.push_back(thing);

	return 0;
}

void Lua_PushCamera(lua_State* L, Camera* cam)
{
	lua_createtable(L, 0, 3);

	lua_pushlightuserdata(L, cam);
	lua_setfield(L, -2, "data");

	lua_pushlightuserdata(L, cam);
	lua_pushcclosure(L, LuaFN_CameraTargetFromRotation, 1);
	lua_setfield(L, -2, "TargetFromRotation");

	lua_pushlightuserdata(L, cam);
	lua_pushcclosure(L, LuaFN_CameraAttachThing, 1);
	lua_setfield(L, -2, "AttachThing");

	lua_createtable(L, 0, 2);
	lua_pushcclosure(L, LuaFN_CameraNewIndex, 0);
	lua_setfield(L, -2, "__newindex");
	lua_pushcclosure(L, LuaFN_CameraIndex, 0);
	lua_setfield(L, -2, "__index");
	lua_setmetatable(L, -2);
}

int LuaFN_NewCamera(lua_State* L)
{
	Lua_PushCamera(L, new Camera());
	return 1;
}

int LuaFN_LoadLevelFromFile(lua_State* L)
{
	//MyProgram* app = (MyProgram*)lua_touserdata(L, lua_upvalueindex(1));
	LoadLevelFromFile(lua_tolstring(L, 1, NULL));
	return 0;
}

RenderPass* Lua_GetRenderPass(lua_State* L, int index)
{
	lua_getfield(L, index, "data");
	auto pass = (RenderPass*)lua_touserdata(L, -1);
	lua_pop(L, 1);
	return pass;
}

VkClearValue Lua_GetClearValue(lua_State* L, int index)
{
	int numValues = Lua_Len(L, index);
	VkClearValue v{};

	if (numValues == 2)
	{
		v.depthStencil.depth = FloatFromTable(index, 1);
		v.depthStencil.stencil = IntFromTable(L, index, 2, "stencil");
	}
	else
		v.color = { {(float)FloatFromTable(index, 1), (float)FloatFromTable(index, 2), (float)FloatFromTable(index, 3), (float)FloatFromTable(index, 4)} };

	return v;
}

#define MAKESHORT(string) string[0] << 8 | string[1]

static int LuaFN_PointerIndex(lua_State* L)
{
	void* ptr = lua_touserdata(L, 1);

	const char* key = lua_tostring(L, 2);

	switch (*(unsigned short*)key)
	{
	case MAKESHORT("fl"):
		lua_pushnumber(L, *(float*)ptr);
		break;

	case MAKESHORT("do"):
		lua_pushnumber(L, *(double*)ptr);
		break;

	case MAKESHORT("bo"):
		lua_pushboolean(L, *(bool*)ptr);
		break;

	case MAKESHORT("lo"):
		lua_pushinteger(L, *(long*)ptr);
		break;

	case MAKESHORT("in"):
		lua_pushinteger(L, *(int*)ptr);
		break;

	case MAKESHORT("ch"):
		lua_pushlstring(L, (char*)ptr, 1);
		break;

	case MAKESHORT("ui"):
		lua_pushinteger(L, *(unsigned int*)ptr);
		break;

	case MAKESHORT("uc"):
		lua_pushnumber(L, *(unsigned char*)ptr);
		break;

	case MAKESHORT("ul"):
		lua_pushnumber(L, *(unsigned long*)ptr);
		break;
	}

	return 1;
}

static int LuaFN_PointerNewIndex(lua_State* L)
{
	void* ptr = lua_touserdata(L, 1);

	const char* key = lua_tostring(L, 2);

	switch (*(unsigned short*)key)
	{
	case MAKESHORT("fl"):
		*(float*)ptr = lua_tonumber(L, 3);
		break;

	case MAKESHORT("do"):
		*(double*)ptr = lua_tonumber(L, 3);
		break;

	case MAKESHORT("bo"):
		*(bool*)ptr = lua_toboolean(L, 3);
		break;

	case MAKESHORT("lo"):
		*(long*)ptr = lua_tointeger(L, 3);
		break;

	case MAKESHORT("in"):
		*(int*)ptr = lua_tointeger(L, 3);
		break;

	case MAKESHORT("ch"):
		*(char*)ptr = *lua_tostring(L, 1);
		break;

	case MAKESHORT("ui"):
		*(unsigned int*)ptr = lua_tointeger(L, 3);
		break;

	case MAKESHORT("uc"):
		*(unsigned char*)ptr = lua_tointeger(L, 3);
		break;

	default:
		*(unsigned long*)ptr = lua_tointeger(L, 3);
		break;
	}

	return 1;
}

static int LuaFN_PointerAdd(lua_State* L)
{
	void* ptr = lua_touserdata(L, 1);

	const char* value = lua_tostring(L, 2);
	int base = StringCompare(value, "0x") ? 16 : 10;
	char* endPtr;

	lua_pushlightuserdata(L, (void*)((size_t)ptr + strtoull(value, &endPtr, base)));
	return 1;
}

static int LuaFN_PointerSub(lua_State* L)
{
	void* ptr = lua_touserdata(L, 1);

	const char* value = lua_tostring(L, 2);
	int base = StringCompare(value, "0x") ? 16 : 10;
	char* endPtr;

	lua_pushlightuserdata(L, (void*)((size_t)ptr - strtoull(value, &endPtr, base)));
	return 1;
}

void Lua_PushPointer(lua_State* L, void* ptr)
{
	lua_pushlightuserdata(L, ptr);

	lua_createtable(L, 0, 2);
	lua_pushcclosure(L, LuaFN_PointerIndex, 0);
	lua_setfield(L, -2, "__index");

	lua_pushcclosure(L, LuaFN_PointerNewIndex, 0);
	lua_setfield(L, -2, "__newindex");

	lua_pushcclosure(L, LuaFN_PointerAdd, 0);
	lua_setfield(L, -2, "__add");

	lua_pushcclosure(L, LuaFN_PointerSub, 0);
	lua_setfield(L, -2, "__sub");


	lua_setmetatable(L, -2);
}

struct LuaDelayThreadData
{
	const char* functionToCall;
	lua_State* L;
	timespec delay;
	bool once;
};

static timespec DelayToTimespec(float delay)
{
	timespec time{};
	time.tv_sec = (time_t)floorf(delay);
	time.tv_nsec = (time_t)fmod(delay, 1.0f) * (time_t)1000000000;

	return time;
}

static bool LuaDelayThreadFunc(LuaDelayThreadData* udata)
{
	thrd_sleep(&udata->delay, NULL);

	lua_getglobal(udata->L, udata->functionToCall);
	lua_call(udata->L, 0, 0);

	return udata->once;
}

std::vector<Thread*> luaDelayThreads = {};

static LuaDelayThreadData* Lua_SetUpDelayThreadData(lua_State* L)
{
	auto threadData = new LuaDelayThreadData();
	threadData->delay = DelayToTimespec(lua_tonumber(L, 1));
	threadData->functionToCall = lua_tostring(L, 2);
	threadData->once = lua_toboolean(L, 3);
	threadData->L = L;

	return threadData;
}

int LuaFN_SetTimer(lua_State* L)
{
	luaDelayThreads.push_back(new Thread((zThreadFunc)LuaDelayThreadFunc, Lua_SetUpDelayThreadData(L)));
	return 0;
}

void Lua_DeInitStuff()
{
	for (auto thread : luaDelayThreads)
		delete thread;

	luaDelayThreads.clear();
}