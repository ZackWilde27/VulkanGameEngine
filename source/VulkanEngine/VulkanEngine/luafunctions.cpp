#pragma once
#include "luafunctions.h"

#include "Camera.h"
#include "SpotLight.h"
#include "Texture.h"
#include "engine.h"
#include "BackendUtils.h"

#include "luaVectorLib.h"
#include "luaUtils.h"
#include "luaVectorLib.h"

#include <iostream>

// A shortcut for getting the length and popping the value off the stack afterwards
lua_Integer Lua_Len(lua_State* L, int idx)
{
	lua_len(L, idx);
	lua_Integer length = lua_tointeger(L, -1);
	lua_pop(L, 1);
	return length;
}

int LuaFN_Round(lua_State* L)
{
	lua_pushinteger(L, (lua_Integer)round(lua_tonumber(L, 1)));
	return 1;
}

int IntFromTable(lua_State* L, int tableDex, lua_Integer intDex, const char* description)
{
	lua_geti(L, tableDex, intDex);

	if (lua_type(L, -1) != LUA_TNUMBER)
		std::cout << description << " is not a number!\n";

	int result = (int)luaL_checkinteger(L, -1);
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
		result = (int)luaL_checkinteger(L, -1);

	lua_pop(L, 1);
	return result;
}

float GetFloatFromTable(lua_State* L, int tableDex, int floatDex)
{
	lua_geti(L, tableDex, floatDex);
	float result = (float)lua_tonumber(L, -1);
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

zstring<CHAR_T>* GetWStringFromTable(lua_State* L, int tableDex, int stringDex)
{
	zstring<CHAR_T>* result;
	lua_geti(L, tableDex, stringDex);

	if (lua_type(L, -1) == LUA_TNIL)
		result = NULL;
	else
		result = Lua_ToString(L, -1);

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

void Lua_PushTexture_NoGC(lua_State* L, Texture** tex, int width, int height)
{
	lua_createtable(L, 0, 3);

	lua_pushlightuserdata(L, tex);
	lua_setfield(L, -2, "texture");

	lua_pushinteger(L, width);
	lua_setfield(L, -2, "width");

	lua_pushinteger(L, height);
	lua_setfield(L, -2, "height");

	if (*tex)
	{
		lua_pushinteger(L, (*tex)->aspect);
		lua_setfield(L, -2, "aspect");

		lua_pushinteger(L, (*tex)->layout[0]);
		lua_setfield(L, -2, "layout");
	}
}

void Lua_PushTexture(lua_State* L, Texture** tex, int width, int height)
{
	Lua_PushTexture_NoGC(L, tex, width, height);

	lua_createtable(L, 0, 1);
	lua_pushcclosure(L, Lua_TextureGC, 0);
	lua_setfield(L, -2, "__gc");
	lua_setmetatable(L, -2);
}

static int Lua_ThingNewIndex(lua_State* L)
{
	auto thing = LuaData<Thing>(L, 1);

	const char* key = lua_tolstring(L, 2, NULL);

	switch (*key)
	{
		case 'p':
			thing->position = *LuaData<float3>(L, 3);
			break;

		case 'r':
			thing->rotation = *LuaData<float3>(L, 3);
			break;

		default:
			thing->scale = *LuaData<float3>(L, 3);
			break;
	}

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
	auto thing = LuaData<Thing>(L, 1);

	const char* key = lua_tolstring(L, 2, NULL);

	// To make indexing as fast as possible, it compares as few letters as possible
	// This does mean that 'thing.potato' is the same as 'thing.position', but that's probably worth it for the efficiency
	switch (*key)
	{
	case 'p':
		Lua_PushFloat3(L, &thing->position);
		break;

	case 'r':
		Lua_PushFloat3(L, &thing->rotation);
		break;

	case 's':
		Lua_PushFloat3(L, &thing->scale);
		break;

	case 'i':
		if (*(key + 1) == 'd')
			lua_pushinteger(L, thing->id);
		else
			lua_pushboolean(L, thing->isStatic);
		break;

	default:
		if (*(key + 3) == 'r')
		{
			auto matrix = Lua_New(float4x4);
			*matrix = WorldMatrix(thing->position, thing->rotation, thing->scale);
			Lua_PushFloat4x4_idx(L, 3);
		}
		else
			Lua_PushMaterialList(L, thing);
		break;
	}

	return 1;
}

static int Lua_ThingEq(lua_State* L)
{
	auto thing1 = LuaData<Thing>(L, 1);
	auto thing2 = LuaData<Thing>(L, 2);

	lua_pushboolean(L, thing1 == thing2);
	return 1;
}

static int LuaFN_ThingAttachThing(lua_State* L)
{
	auto self = (Thing*)lua_touserdata(L, lua_upvalueindex(1));
	auto attached = LuaData<Thing>(L, 1);

	attached->parent = self;
	self->children.push_back(attached);

	return 0;
}

static int LuaFN_ThingUpdateMatrix(lua_State* L)
{
	auto thing = (Thing*)lua_touserdata(L, lua_upvalueindex(1));

	if (lua_gettop(L) == 0)
		thing->UpdateMatrix();
	else
		thing->UpdateMatrix(LuaData<float4x4>(L, 1));

	return 0;
}

void Lua_PushThing(lua_State* L, Thing* mo)
{
	lua_createtable(L, 0, 2);

	lua_pushlightuserdata(L, mo);
	lua_setfield(L, -2, LUA_DATA_NAME);

	lua_pushlightuserdata(L, mo);
	lua_pushcclosure(L, LuaFN_ThingAttachThing, 1);
	lua_setfield(L, -2, "AttachThing");

	lua_pushlightuserdata(L, mo);
	lua_pushcclosure(L, LuaFN_ThingUpdateMatrix, 1);
	lua_setfield(L, -2, "UpdateMatrix");

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

	BYTE id = (BYTE)lua_tointeger(L, 3);
	float3 rayDir = glm::normalize(*rayEnd - *rayStart);

	Thing* thing;
	double distance;
	float3 normal;

	bool hit = RayObjects(*rayStart, rayDir, id, &thing, &distance, &normal);

	lua_createtable(L, 0, 1);

	if (hit)
	{
		if (distance <= glm::length(*rayEnd - *rayStart))
		{
			Lua_PushThing(L, thing);
			lua_setfield(L, -2, "object");

			lua_pushnumber(L, distance);
			lua_setfield(L, -2, "distance");

			auto nrm = Lua_New(float3);
			*nrm = normal;
			Lua_PushFloat3_idx(L, lua_gettop(L));
			lua_setfield(L, -2, "normal");
		}
		else
			hit = false;
	}

	lua_pushboolean(L, hit);
	lua_setfield(L, -2, "hit");

	return 1;
}

int LuaFN_TraceStep(lua_State* L)
{
	auto start = LuaData<float3>(L, 1);
	auto dir = LuaData<float3>(L, 2);

	lua_Number stepLength = lua_tonumber(L, 3);
	lua_Number spacing = lua_tonumber(L, 4);
	lua_Number rayLength = spacing + lua_tonumber(L, 5);

	BYTE id = (BYTE)lua_tointeger(L, 6);

	Thing* thing;
	double distance;
	float3 normal;

	auto newPoint = Lua_New(float3);

	if (RayObjects(*start, *dir, id, &thing, &distance, &normal) && distance <= rayLength)
	{
		*newPoint = *start + (*dir * (float)distance) + (normal * (float)spacing);

		if (glm::distance(*start, *newPoint) > stepLength)
			*newPoint = (glm::normalize(*newPoint - *start) * (float)stepLength) + *start;
	}
	else
		*newPoint = *start + (*dir * (float)stepLength);

	Lua_PushFloat3_idx(L, -2);
	return 1;
}


int LuaFN_MoveThingTo(lua_State* L)
{
	auto thing = LuaData<Thing>(L, 1);
	auto moveTo = *LuaData<float3>(L, 2);
	auto moveSpeed = (float)lua_tonumber(L, 3);
	auto callback = lua_tostring(L, 4);

	AddMovingThing(thing, moveTo, moveSpeed, callback);
	return 0;
}

constexpr float HALFPI = 3.14159f / 2;

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
	auto pos = LuaData<float3>(L, 1);
	auto radius = lua_tonumber(L, 2);
	auto id = lua_tointeger(L, 3);

	auto lists = GetThingList();

	lua_createtable(L, 0, 0);

	lua_Integer index = 1;

	for (auto thing : lists[id])
	{
		if (glm::distance(thing->position, *pos) < radius)
		{
			Lua_PushThing(L, thing);
			lua_seti(L, -2, index++);
		}
	}

	return 1;
}

int LuaFN_GetAllThingsInRadius(lua_State* L)
{
	auto pos = LuaData<float3>(L, 1);
	auto radius = lua_tonumber(L, 2);

	auto lists = GetThingList();

	lua_createtable(L, 0, 0);

	lua_Integer index = 1;

	for (const auto& list : lists)
	{
		for (auto thing : list)
		{
			if (glm::distance(thing->position, *pos) < radius)
			{
				Lua_PushThing(L, thing);
				lua_seti(L, -2, index++);
			}
		}
	}

	return 1;
}

int LuaFN_GetThingsById(lua_State* L)
{
	BYTE id = (BYTE)lua_tointeger(L, 1);
	lua_Integer index = 1;

	std::vector<std::vector<Thing*>> lists = GetThingList();

	lua_createtable(L, 0, 0);

	for (auto thing : lists[id])
	{
		Lua_PushThing(L, thing);
		lua_seti(L, -2, index++);
	}

	return 1;
}

static int LuaFN_CameraIndex(lua_State* L)
{
	auto cam = LuaData<Camera>(L, 1);
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
	auto cam = LuaData<Camera>(L, 1);
	const char* key = lua_tolstring(L, 2, NULL);
	auto vec = LuaData<float3>(L, 3);

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

	cam->TargetFromRotation((float)lua_tonumber(L, 1), (float)lua_tonumber(L, 2));
	return 0;
}

static int LuaFN_CameraAttachThing(lua_State* L)
{
	auto cam = (Camera*)lua_touserdata(L, lua_upvalueindex(1));
	auto thing = LuaData<Thing>(L, 1);

	cam->attachedThings.push_back(thing);

	return 0;
}

void Lua_PushCamera(lua_State* L, Camera* cam)
{
	lua_createtable(L, 0, 3);

	lua_pushlightuserdata(L, cam);
	lua_setfield(L, -2, LUA_DATA_NAME);

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
	LoadLevelFromFile(lua_tolstring(L, 1, NULL));
	return 0;
}

RenderPass* Lua_GetRenderPass(lua_State* L, int index)
{
	return LuaData<RenderPass>(L, index);
}

VkClearValue Lua_GetClearValue(lua_State* L, int index)
{
	lua_Integer numValues = Lua_Len(L, index);
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
		*(float*)ptr = (float)lua_tonumber(L, 3);
		break;

	case MAKESHORT("do"):
		*(double*)ptr = lua_tonumber(L, 3);
		break;

	case MAKESHORT("bo"):
		*(bool*)ptr = lua_toboolean(L, 3);
		break;

	case MAKESHORT("lo"):
		*(long long*)ptr = (long long)lua_tointeger(L, 3);
		break;

	case MAKESHORT("in"):
		*(int*)ptr = (int)lua_tointeger(L, 3);
		break;

	case MAKESHORT("ch"):
		*(char*)ptr = *lua_tostring(L, 1);
		break;

	case MAKESHORT("ui"):
		*(unsigned int*)ptr = (unsigned int)lua_tointeger(L, 3);
		break;

	case MAKESHORT("uc"):
		*(unsigned char*)ptr = (unsigned char)lua_tointeger(L, 3);
		break;

	default:
		*(unsigned long long*)ptr = (unsigned long long)lua_tointeger(L, 3);
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

static int LuaFN_SpotLightIndex(lua_State* L)
{
	auto light = LuaData<SpotLight>(L, 1);

	switch (*lua_tostring(L, 2))
	{
		case 'p':
			Lua_PushFloat3(L, (float3*)&light->position);
			break;

		case 'd':
			Lua_PushFloat3(L, (float3*)&light->dir);
			break;

		case 'a':
			lua_pushnumber(L, light->position.w);
			break;

		case 'f':
			lua_pushnumber(L, light->dir.w);
			break;

		case 'm':
			Lua_PushFloat4x4(L, &light->viewProj);
			break;

		default:
			Lua_PushFloat3(L, (float3*)&light->colour);
			break;
	}

	return 1;
}

void Lua_PushSpotLight(lua_State* L, SpotLight* light)
{
	Lua_PushDataWithGCIndexNewIndex(L, light, NULL, LuaFN_SpotLightIndex, LuaFN_SpotLightNewIndex);
}

void Lua_PushDataWithGC(lua_State* L, void* data, lua_CFunction gc)
{
	lua_createtable(L, 0, 1);
	lua_pushlightuserdata(L, data);
	lua_setfield(L, -2, LUA_DATA_NAME);

	lua_createtable(L, 0, 1);
	lua_pushcclosure(L, gc, 0);
	lua_setfield(L, -2, "__gc");

	lua_setmetatable(L, -2);
}

void Lua_PushDataWithGCIndexNewIndex(lua_State* L, void* data, lua_CFunction gc, lua_CFunction index, lua_CFunction newIndex)
{
	lua_createtable(L, 0, 1);
	lua_pushlightuserdata(L, data);
	lua_setfield(L, -2, LUA_DATA_NAME);

	int numElements = (bool)gc + (bool)index + (bool)newIndex;

	lua_createtable(L, 0, numElements);

	if (gc)
	{
		lua_pushcclosure(L, gc, 0);
		lua_setfield(L, -2, "__gc");
	}

	if (index)
	{
		lua_pushcclosure(L, index, 0);
		lua_setfield(L, -2, "__index");
	}

	if (newIndex)
	{
		lua_pushcclosure(L, newIndex, 0);
		lua_setfield(L, -2, "__newindex");
	}

	lua_setmetatable(L, -2);
}

zstring<CHAR_T>* Lua_ToString(lua_State* L, lua_Integer index)
{
#ifdef WIDE_STRINGS
	zstring<wchar_t>* string = new zstring(L"%hs", lua_tostring(L, index));
	return string;
#else
	return new zstring(lua_tostring(L, index));
#endif
}