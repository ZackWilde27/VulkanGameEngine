#pragma once
#include "engineTypes.h"

int LuaFN_Round(lua_State* L);

lua_Integer Lua_Len(lua_State* L, int idx);

zstring<CHAR_T>* Lua_ToString(lua_State* L, lua_Integer index);

int Lua_TextureGC(lua_State* L);
int LuaFN_OneTimeBlit(lua_State* L);
int LuaFN_LoadImage(lua_State* L);
int LuaFN_SetActiveCamera(lua_State* L);
int LuaFN_GetActiveCamera(lua_State* L);
int LuaFN_LoadLevelFromFile(lua_State* L);

int LuaFN_GetThingsById(lua_State* L);
int LuaFN_GetThingsInRadius(lua_State * L);
int LuaFN_GetAllThingsInRadius(lua_State * L);

int LuaFN_TraceRay(lua_State* L);
int LuaFN_TraceStep(lua_State* L);

int LuaFN_MoveThingTo(lua_State* L);
int LuaFN_DirectionFromAngle(lua_State* L);

int LuaFN_NewCamera(lua_State* L);
int LuaFN_NewFloat2(lua_State * L);
int LuaFN_NewFloat3(lua_State* L);
int LuaFN_NewFloat4(lua_State * L);
int LuaFN_NewMaterial(lua_State * L);
int LuaFN_NewComputeShader(lua_State* L);
int LuaFN_NewVulkanMemory(lua_State* L);
int LuaFN_NewCStruct(lua_State* L);
int LuaFN_CreateImage(lua_State* L);
int LuaFN_CreateRenderPass(lua_State* L);
int LuaFN_CreateFrameBuffer(lua_State* L);

int LuaFN_SetSubtitles(lua_State* L);

int LuaFN_Render(lua_State* L);

int LuaFN_SetTimer(lua_State* L);

int IntFromTable(lua_State* L, int tableDex, lua_Integer intDex, const char* description);
int IntFromTable_Default(lua_State* L, int tableDex, int intDex, int defaultVal);
bool GetBoolFromTable(lua_State* L, int tableDex, int boolDex);
const char* GetStringFromTable(lua_State* L, int tableDex, int stringDex);
zstring<CHAR_T>* GetWStringFromTable(lua_State* L, int tableDex, int stringDex);
void* GetUDataFromTable(lua_State* L, int tableDex, int dataDex);
float GetFloatFromTable(lua_State* L, int tableDex, int floatDex);
RenderPass* Lua_GetRenderPass(lua_State* L, int index);
VkClearValue Lua_GetClearValue(lua_State * L, int index);

int LuaFN_SpawnThing(lua_State* L);
int LuaFN_SpawnSpotLight(lua_State* L);

void Lua_PushThing(lua_State * L, Thing * mo);
void Lua_PushTexture_NoGC(lua_State* L, Texture** tex, int width, int height);
void Lua_PushCamera(lua_State* L, Camera* cam);
void Lua_PushTexture(lua_State* L, Texture** tex, int width, int height);

void Lua_PushSpotLight(lua_State* L, SpotLight* light);
void Lua_PushSpotLight_idx(lua_State* L, int index);

int LuaFN_SpotLightNewIndex(lua_State* L);

void Lua_PushDataWithGC(lua_State* L, void* data, lua_CFunction gc);
void Lua_PushDataWithGCIndexNewIndex(lua_State* L, void* data, lua_CFunction gc, lua_CFunction index, lua_CFunction newIndex);