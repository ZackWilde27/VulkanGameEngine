#pragma once
#include "luafunctions.h"
#include "engine.h"
#include "luaUtils.h"
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

static int LuaFN_AddWorldLocation(lua_State* L)
{
	MeshObject* object = (MeshObject*)lua_touserdata(L, 1);

	double x = lua_tonumber(L, 2);
	double y = lua_tonumber(L, 3);
	double z = lua_tonumber(L, 4);

	object->position.x += x;
	object->position.y += y;
	object->position.z += z;

	return 0;
}

static int LuaFN_SetWorldLocation(lua_State* L)
{
	MeshObject* object = (MeshObject*)lua_touserdata(L, 1);

	double x = lua_tonumber(L, 2);
	double y = lua_tonumber(L, 3);
	double z = lua_tonumber(L, 4);

	object->position.x = x;
	object->position.y = y;
	object->position.z = z;

	return 0;
}


static int LuaFN_AddWorldRotation(lua_State* L)
{
	MeshObject* object = (MeshObject*)lua_touserdata(L, 1);

	double x = lua_tonumber(L, 2);
	double y = lua_tonumber(L, 3);
	double z = lua_tonumber(L, 4);

	object->rotation.x += x;
	object->rotation.y += y;
	object->rotation.z += z;

	return 0;
}

static int LuaFN_SetWorldRotation(lua_State* L)
{
	MeshObject* object = (MeshObject*)lua_touserdata(L, 1);

	double x = lua_tonumber(L, 2);
	double y = lua_tonumber(L, 3);
	double z = lua_tonumber(L, 4);

	object->rotation.x = x;
	object->rotation.y = y;
	object->rotation.z = z;

	return 0;
}

static int LuaFN_GetLocationX(lua_State* L)
{
	MeshObject* object = (MeshObject*)lua_touserdata(L, 1);

	lua_pushnumber(L, (double)object->position.x);

	return 1;
}

static int LuaFN_GetLocationY(lua_State* L)
{
	MeshObject* object = (MeshObject*)lua_touserdata(L, 1);

	lua_pushnumber(L, (double)object->position.y);

	return 1;
}

static int LuaFN_GetLocationZ(lua_State* L)
{
	MeshObject* object = (MeshObject*)lua_touserdata(L, 1);

	lua_pushnumber(L, (double)object->position.z);

	return 1;
}

static int LuaFN_GetRotationX(lua_State* L)
{
	MeshObject* object = (MeshObject*)lua_touserdata(L, 1);

	lua_pushnumber(L, (double)object->rotation.x);

	return 1;
}

static int LuaFN_GetRotationY(lua_State* L)
{
	MeshObject* object = (MeshObject*)lua_touserdata(L, 1);

	lua_pushnumber(L, (double)object->rotation.y);

	return 1;
}

static int LuaFN_GetRotationZ(lua_State* L)
{
	MeshObject* object = (MeshObject*)lua_touserdata(L, 1);

	lua_pushnumber(L, (double)object->rotation.z);

	return 1;
}

static int LuaFN_AddWorldScale(lua_State* L)
{
	MeshObject* object = (MeshObject*)lua_touserdata(L, 1);

	double x = lua_tonumber(L, 2);
	double y = lua_tonumber(L, 3);
	double z = lua_tonumber(L, 4);

	object->scale.x += x;
	object->scale.y += y;
	object->scale.z += z;

	return 0;
}

static int LuaFN_SetWorldScale(lua_State* L)
{
	MeshObject* object = (MeshObject*)lua_touserdata(L, 1);

	double x = lua_tonumber(L, 2);
	double y = lua_tonumber(L, 3);
	double z = lua_tonumber(L, 4);


	object->scale.x = x;
	object->scale.y = y;
	object->scale.z = z;

	return 0;
}

static int LuaFN_GetScaleX(lua_State* L)
{
	MeshObject* object = (MeshObject*)lua_touserdata(L, 1);

	lua_pushnumber(L, (double)object->scale.x);

	return 1;
}

static int LuaFN_GetScaleY(lua_State* L)
{
	MeshObject* object = (MeshObject*)lua_touserdata(L, 1);

	lua_pushnumber(L, (double)object->scale.y);

	return 1;
}

static int LuaFN_GetScaleZ(lua_State* L)
{
	MeshObject* object = (MeshObject*)lua_touserdata(L, 1);

	lua_pushnumber(L, (double)object->scale.z);

	return 1;
}


// I'm not sure how to put everything in one lua_State and still have each script call their own version of the 'Tick' or 'Begin' functions
// So I have to explicity transfer a value from one state to another, I can't just lua_pushvalue() and call it a day
#define LuaFN_SendFunc(name, gatherline, pushline) static int name(lua_State* L) \
																								{ \
																									MeshObject* to = (MeshObject*)lua_touserdata(L, 1); \
																									gatherline; \
																									lua_Integer reason = lua_tointeger(L, 3); \
																									lua_getglobal(to->LuaState, "Receive"); \
																									pushline; \
																									lua_pushinteger(to->LuaState, reason); \
																									lua_call(to->LuaState, 2, 0); \
																									return 0; \
																								}

LuaFN_SendFunc(LuaFN_SendNum, lua_Number data = lua_tonumber(L, 2), lua_pushnumber(to->LuaState, data))
LuaFN_SendFunc(LuaFN_SendString, size_t len; const char* data = lua_tolstring(L, 2, &len), lua_pushlstring(to->LuaState, data, len))

static int LuaFN_SendBool(lua_State* L)
{
	MeshObject* to = (MeshObject*)lua_touserdata(L, 1);
	int data = lua_toboolean(L, 2);
	lua_Integer reason = lua_tointeger(L, 3);

	lua_getglobal(to->LuaState, "Receive");
	lua_pushboolean(to->LuaState, data);
	lua_pushinteger(to->LuaState, reason);
	lua_call(to->LuaState, 2, 0);

	return 0;
}

std::vector<void*> luaToFree = {};

#define Lua_Get(type, index) (type*)lua_touserdata(L, index)

static int LuaFN_Free(lua_State* L)
{
	void* ptr = lua_touserdata(L, 1);

	if (ptr)
		free(ptr);

	return 0;
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

static int Lua_MeshObjectNewIndex(lua_State* L)
{
	auto mo = (MeshObject*)Lua_GetRawData(L, 1);

	const char* key = lua_tolstring(L, 2, NULL);

	if (!strcmp(key, "position"))
		mo->position = *Lua_GetFloat3(L, 3);
	else if (!strcmp(key, "rotation"))
		mo->rotation = *Lua_GetFloat3(L, 3);
	else if (!strcmp(key, "scale"))
		mo->scale = *Lua_GetFloat3(L, 3);
	else
		lua_setfield(L, 1, key);

	mo->UpdateMatrix();

	return 0;
}

unsigned short idAsShort =  'i' | ('d' << 8);

static int Lua_MeshObjectIndex(lua_State* L)
{
	auto mo = (MeshObject*)Lua_GetRawData(L, 1);

	const char* key = lua_tolstring(L, 2, NULL);

	if (!strcmp(key, "position"))
		Lua_PushFloat3(L, &mo->position);
	else if (!strcmp(key, "rotation"))
		Lua_PushFloat3(L, &mo->rotation);
	else if (!strcmp(key, "id"))
		lua_pushinteger(L, mo->id);
	else if (!strcmp(key, "isStatic"))
		lua_pushboolean(L, mo->isStatic);
	else if (!strcmp(key, "scale"))
		Lua_PushFloat3(L, &mo->scale);
	else
		lua_getfield(L, 1, key);

	return 1;
}

static int Lua_MeshObjectClose(lua_State* L)
{
	return 0;
}

static int Lua_MeshObjectEq(lua_State* L)
{
	auto mo1 = (MeshObject*)Lua_GetRawData(L, 1);
	auto mo2 = (MeshObject*)Lua_GetRawData(L, 2);

	lua_pushboolean(L, mo1 == mo2);
	return 1;
}

static void Lua_PushMeshObject(lua_State* L, MeshObject* mo)
{
	lua_createtable(L, 0, 1);

	lua_pushlightuserdata(L, mo);
	lua_setfield(L, -2, "data");

	lua_createtable(L, 0, 3);
	lua_pushcclosure(L, Lua_MeshObjectNewIndex, 0);
	lua_setfield(L, -2, "__newindex");
	lua_pushcclosure(L, Lua_MeshObjectIndex, 0);
	lua_setfield(L, -2, "__index");
	lua_pushcclosure(L, Lua_MeshObjectEq, 0);
	lua_setfield(L, -2, "__eq");
	lua_setmetatable(L, -2);
}

int LuaFN_TraceRay(lua_State* L)
{
	float3* rayStart, * rayEnd;
	Lua_GetFloat3_2(L, rayStart, rayEnd);

	int id = lua_tointeger(L, 3);
	float3 rayDir = glm::normalize(*rayEnd - *rayStart);

	MeshObject* outObject;
	float outDist;

	bool hit = RayObjects(*rayStart, rayDir, id, &outObject, &outDist);

	lua_createtable(L, 0, 1);

	lua_pushboolean(L, hit);
	lua_setfield(L, -2, "hit");

	if (hit)
	{
		Lua_PushMeshObject(L, outObject);
		lua_setfield(L, -2, "object");

		lua_pushnumber(L, outDist);
		lua_setfield(L, -2, "distance");
	}

	return 1;
}



int LuaFN_MoveObjectTo(lua_State* L)
{
	auto mo = (MeshObject*)Lua_GetRawData(L, 1);
	auto moveTo = *Lua_GetFloat3(L, 2);
	auto moveSpeed = lua_tonumber(L, 3);
	auto callback = lua_tostring(L, 4);

	AddMovingObject(mo, moveTo, moveSpeed, callback);
	return 0;
}

int LuaFN_CreateImage(lua_State* L)
{
	auto tex = NEW(Texture);
	luaToFree.push_back(tex);

	int width = (int)lua_tonumber(L, 4);
	int height = (int)lua_tonumber(L, 5);
	int mipLevels = lua_tointeger(L, 6);

	VkImageAspectFlags aspect = lua_tointeger(L, 11);

	FullCreateImage((VkImageType)lua_tointeger(L, 1), (VkImageViewType)lua_tointeger(L, 2), (VkFormat)lua_tointeger(L, 3), width, height, mipLevels, lua_tointeger(L, 7), (VkSampleCountFlagBits)lua_tointeger(L, 8), (VkImageTiling)lua_tointeger(L, 9), (VkImageUsageFlags)lua_tointeger(L, 10), aspect, (VkFilter)lua_tointeger(L, 12), (VkFilter)lua_tointeger(L, 13), (VkSamplerAddressMode)lua_tointeger(L, 14), tex->Image, tex->Memory, tex->View, tex->Sampler, false);
	tex->filename = NULL;
	tex->freeFilename = false;
	tex->Width = width;
	tex->Height = height;
	tex->Aspect = aspect;
	tex->mipLevels = mipLevels;
	tex->currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	Lua_PushTexture(L, tex, width, height);
	return 1;
}

static int LuaFN_SendOther(lua_State* L)
{
	MeshObject* to = (MeshObject*)lua_touserdata(L, 1);
	void* data = lua_touserdata(L, 2);
	lua_Integer reason = lua_tointeger(L, 3);

	lua_getglobal(to->LuaState, "Receive");
	lua_pushlightuserdata(to->LuaState, data);
	lua_pushinteger(to->LuaState, reason);
	lua_call(to->LuaState, 2, 0);
	return 0;
}



void Lua_PushFloat3(lua_State* L, float3* data);
void Lua_PushFloat3_idx(lua_State* L, int index);


static int LuaFN_Float3NewIndex(lua_State* L)
{
	lua_getfield(L, 1, "data");
	float3* vec = (float3*)lua_touserdata(L, -1);
	lua_pop(L, 1);

	const char* key = lua_tolstring(L, 2, NULL);
	switch (*key)
	{
	case 'x':
	case 'r':
		vec->x = lua_tonumber(L, 3);
		break;
	case 'y':
	case 'g':
		vec->y = lua_tonumber(L, 3);
		break;
	case 'z':
	case 'b':
		vec->z = lua_tonumber(L, 3);
		break;
	}

	return 0;
}

static int LuaFN_Float3Index(lua_State* L)
{
	lua_getfield(L, 1, "data");
	float3* vec = (float3*)lua_touserdata(L, -1);
	lua_pop(L, 1);

	const char* key = lua_tolstring(L, 2, NULL);

	lua_Number n;

	switch (*key)
	{
	case 'x':
	case 'r':
	case '0':
		n = vec->x;
		break;
	case 'y':
	case 'g':
	case '1':
		n = vec->y;
		break;
	default:
		n = vec->z;
		break;
	}

	lua_pushnumber(L, n);
	return 1;
}

static int LuaFN_Float3Add(lua_State* L)
{
	float3* x, * y;
	Lua_GetFloat3_2(L, x, y);

	float3* vec = (float3*)lua_newuserdata(L, sizeof(float3));
	*vec = *x + *y;

	Lua_PushFloat3_idx(L, 3);
	return 1;
}

static int LuaFN_Float3Sub(lua_State* L)
{
	float3* x, * y;
	Lua_GetFloat3_2(L, x, y);

	float3* vec = (float3*)lua_newuserdata(L, sizeof(float3));
	*vec = *x - *y;

	Lua_PushFloat3_idx(L, 3);
	return 1;
}

static int LuaFN_Float3Mul(lua_State* L)
{
	float3* vec = (float3*)lua_newuserdata(L, sizeof(float3));

	if (lua_type(L, 2) == LUA_TNUMBER)
		*vec = *Lua_GetFloat3(L, 1) * (float)lua_tonumber(L, 2);
	else
	{
		float3* x, * y;
		Lua_GetFloat3_2(L, x, y);

		*vec = *x * *y;
	}

	Lua_PushFloat3_idx(L, 3);
	return 1;
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
	float3* vec = (float3*)lua_newuserdata(L, sizeof(float3));

	*vec = -(*Lua_GetFloat3(L, 1));

	Lua_PushFloat3_idx(L, 2);
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

int LuaFN_GetObjectsById(lua_State* L)
{
	int id = lua_tointeger(L, 1);
	int index = 1;

	size_t numObjects;
	MeshObject** ptr = GetObjectList(numObjects);

	lua_createtable(L, 0, 0);

	for (size_t i = 0; i < numObjects; i++)
	{
		if (ptr[i]->id == id)
		{
			Lua_PushMeshObject(L, ptr[i]);
			lua_seti(L, -2, index);
			index++;
		}
	}

	return 1;
}

void Lua_SetFloat3Metatable(lua_State* L)
{
	lua_createtable(L, 0, 5);

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

	lua_pushcclosure(L, LuaFN_Float3Eq, 0);
	lua_setfield(L, -2, "__eq");

	lua_pushcclosure(L, LuaFN_Float3Neg, 0);
	lua_setfield(L, -2, "__unm");

	lua_setmetatable(L, -2);
}

void Lua_PushFloat3_idx(lua_State* L, int index)
{
	lua_createtable(L, 0, 1);
	lua_rotate(L, index, -1);
	lua_setfield(L, -2, "data");

	Lua_SetFloat3Metatable(L);
}

void Lua_PushFloat3(lua_State* L, float3* data)
{
	lua_createtable(L, 0, 1);
	lua_pushlightuserdata(L, data);
	lua_setfield(L, -2, "data");

	Lua_SetFloat3Metatable(L);
}

static int LuaFN_CameraIndex(lua_State* L)
{
	lua_getfield(L, 1, "data");
	Camera* cam = (Camera*)lua_touserdata(L, -1);
	lua_pop(L, 1);
	const char* key = lua_tolstring(L, 2, NULL);

	float3* ptr;
	if (*key == 'p')
		ptr = &cam->position;
	else
		ptr = &cam->target;

	Lua_PushFloat3(L, ptr);
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
	case 't':
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

void Lua_PushCamera(lua_State* L, Camera* cam)
{
	lua_createtable(L, 0, 4);

	lua_pushlightuserdata(L, cam);
	lua_setfield(L, -2, "data");

	lua_pushlightuserdata(L, &cam->matrix);
	lua_setfield(L, -2, "matrix");

	lua_pushlightuserdata(L, &cam->viewMatrix);
	lua_setfield(L, -2, "viewMatrix");

	lua_pushlightuserdata(L, cam);
	lua_pushcclosure(L, LuaFN_CameraTargetFromRotation, 1);
	lua_setfield(L, -2, "TargetFromRotation");

	lua_createtable(L, 0, 2);
	lua_pushcclosure(L, LuaFN_CameraNewIndex, 0);
	lua_setfield(L, -2, "__newindex");
	lua_pushcclosure(L, LuaFN_CameraIndex, 0);
	lua_setfield(L, -2, "__index");
	lua_setmetatable(L, -2);
}

static int LuaFN_SetFloat3(lua_State* L)
{
	float3* vec = (float3*)lua_touserdata(L, 1);
	*vec = float3(IntFromTable(L, 2, 1, "x"), IntFromTable(L, 2, 2, "y"), IntFromTable(L, 2, 3, "z"));
	return 0;
}

static int LuaFN_GetFloat3(lua_State* L)
{
	float3* vec = (float3*)lua_touserdata(L, 1);
	lua_pushnumber(L, vec->x);
	lua_pushnumber(L, vec->y);
	lua_pushnumber(L, vec->z);
	return 3;
}

int LuaFN_NewFloat3(lua_State* L)
{
	auto vec = (float3*)lua_newuserdata(L, sizeof(float3));
	*vec = float3(lua_tonumber(L, 1), lua_tonumber(L, 2), lua_tonumber(L, 3));

	Lua_PushFloat3_idx(L, 4);
	return 1;
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

static int LuaFN_GetUData(lua_State* L)
{
	lua_pushlightuserdata(L, (void*)1);

	return 1;
}

RenderPass* Lua_GetRenderPass(lua_State* L, int index)
{
	lua_getfield(L, index, "data");
	auto pass = (RenderPass*)lua_touserdata(L, -1);
	lua_pop(L, 1);
	return pass;
}