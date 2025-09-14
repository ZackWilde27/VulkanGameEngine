#pragma once
#include "engineTypes.h"

#define LuaData(name, index, type) 	lua_getfield(L, index, "data"); \
														auto name = (type*)lua_touserdata(L, -1); \
														lua_pop(L, 1);

#define UDataFromTable(tableDex, dataDex) GetUDataFromTable(L, tableDex, dataDex)
#define FloatFromTable(tableDex, floatDex) GetFloatFromTable(L, tableDex, floatDex)
#define StringFromTable(tableDex, stringDex) GetStringFromTable(L, tableDex, stringDex)
#define BoolFromTable(tableDex, boolDex) GetBoolFromTable(L, tableDex, boolDex)

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

float3* Lua_GetFloat3(lua_State* L, int index);
void Lua_GetFloat3_2(lua_State* L, float3*& v1, float3*& v2);