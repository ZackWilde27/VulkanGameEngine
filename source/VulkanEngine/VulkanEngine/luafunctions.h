#pragma once
#include "engineTypes.h"

#define LuaData(name, index, type) 	lua_getfield(L, index, "data"); \
														auto name = (type*)lua_touserdata(L, -1); \
														lua_pop(L, 1);

// Macro for easily making variations of vector operators
#define LuaBasicOperator(type, operation, pushfunc) 	LuaData(v1, 1, type); \
																				LuaData(v2, 2, type); \
\
																				auto v3 = (type*)lua_newuserdata(L, sizeof(type)); \
																				*v3 = *v1 operation *v2; \
																				pushfunc(L, 3); \
\
																				return 1;

// Macro for vector operators that can have a number on the right side
#define LuaOptionalNumberOperator(type, operation, pushfunc)	LuaData(v1, 1, type); \
\
																								auto v3 = (type*)lua_newuserdata(L, sizeof(type)); \
																								if (lua_isnumber(L, 2)) \
																									*v3 = *v1 operation (float)lua_tonumber(L, 2); \
																								else { \
																									LuaData(v2, 2, type); \
																									*v3 = *v1 operation * v2; \
																								} \
\
																								pushfunc(L, 3); \
																								return 1;

int Lua_Len(lua_State* L, int idx);

int Lua_TextureGC(lua_State* L);
int LuaFN_OneTimeBlit(lua_State* L);
int LuaFN_LoadImage(lua_State* L);
int LuaFN_SetActiveCamera(lua_State* L);
int LuaFN_GetActiveCamera(lua_State* L);
int LuaFN_LoadLevelFromFile(lua_State* L);
int LuaFN_GetObjectsById(lua_State* L);
int LuaFN_TraceRay(lua_State* L);

int LuaFN_MoveObjectTo(lua_State* L);
int LuaFN_DirectionFromAngle(lua_State* L);

int LuaFN_NewCamera(lua_State* L);
int LuaFN_NewFloat2(lua_State * L);
int LuaFN_NewFloat3(lua_State* L);
int LuaFN_NewFloat4(lua_State * L);
int LuaFN_CreateImage(lua_State* L);
int LuaFN_CreateRenderPass(lua_State* L);
int LuaFN_CreateFrameBuffer(lua_State* L);

int IntFromTable(lua_State* L, int tableDex, int intDex, const char* description);
int IntFromTable_Default(lua_State* L, int tableDex, int intDex, int defaultVal);
bool GetBoolFromTable(lua_State* L, int tableDex, int boolDex);
const char* GetStringFromTable(lua_State* L, int tableDex, int stringDex);
void* GetUDataFromTable(lua_State* L, int tableDex, int dataDex);
float GetFloatFromTable(lua_State* L, int tableDex, int floatDex);
RenderPass* Lua_GetRenderPass(lua_State* L, int index);

int LuaFN_SpawnObject(lua_State* L);

void Lua_PushTexture_NoGC(lua_State* L, Texture* tex, int width, int height);
void Lua_PushCamera(lua_State* L, Camera* cam);
void Lua_PushFloat2_idx(lua_State * L, int idx);
void Lua_PushFloat3(lua_State* L, float3* data);
void Lua_PushFloat3_idx(lua_State* L, int index);
void Lua_PushFloat4_idx(lua_State * L, int idx);