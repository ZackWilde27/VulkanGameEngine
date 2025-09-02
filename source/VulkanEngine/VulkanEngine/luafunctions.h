#pragma once
#include "engineTypes.h"

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
int LuaFN_NewFloat3(lua_State* L);
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
void Lua_PushFloat3(lua_State* L, float3* data);