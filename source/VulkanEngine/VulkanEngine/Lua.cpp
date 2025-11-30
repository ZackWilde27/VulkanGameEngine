#include "Lua.h"
#include "luafunctions.h"
#include "engine.h"
#include "Thread.h"
#include "Backend.h"
#include "VulkanBackend/SunLight.h"
#include "VulkanBackend/SpotLight.h"

#include "luaVectorLib.h"
#include "luaGLMlib.h"
#include "luaGLFWlib.h"
#include "luaSoundLib.h"
#include "luaImGuiLib.h"

std::vector<ConsoleCommandVar> consoleVars = {};

static void* LuaAllocator(void* ud, void* ptr, size_t osize, size_t nsize)
{
	if (!nsize)
	{
		free(ptr);
		return NULL;
	}

	return realloc(ptr, nsize);
}

#define AddLuaFunc(state, func, name) lua_pushcclosure(state, func, 0); \
														  lua_setglobal(state, name)

bool locked = true;

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	Lua* lua = GetEngine()->lua;

	if (action == GLFW_PRESS)
	{
		if (key == GLFW_KEY_GRAVE_ACCENT)
			lua->showConsole = !lua->showConsole;

		if (key == GLFW_KEY_TAB)
		{
			locked = !locked;
			glfwSetInputMode(window, GLFW_CURSOR, locked ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
			glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, locked ? GLFW_TRUE : GLFW_FALSE);
		}
	}

	lua_State* L = lua->L;

	lua_getglobal(L, "KeyCallback");
	lua_pushinteger(L, key);
	lua_pushinteger(L, scancode);
	lua_pushinteger(L, action);
	lua_pushinteger(L, mods);
	lua_call(L, 4, 0);
}

void mouse_callback(GLFWwindow* window, int button, int action, int mods)
{
	lua_State* L = GetEngine()->lua->L;

	lua_getglobal(L, "MouseCallback");
	lua_pushinteger(L, button);
	lua_pushinteger(L, action);
	lua_pushinteger(L, mods);
	lua_call(L, 3, 0);
}



Lua::Lua(LastGenEngine* engine, char* gameLuaFilename)
{
	runningLua = false;
	threadRunningLua = false;
	this->gameLuaFilename = gameLuaFilename;

	L = lua_newstate(LuaAllocator, this);
	luaL_openlibs(L);

	AddLuaFunc(L, LuaFN_CreateRenderPass, "CreateRenderPass");
	AddLuaFunc(L, LuaFN_CreateImage, "CreateImage");
	AddLuaFunc(L, LuaFN_CreateFrameBuffer, "CreateFrameBuffer");
	AddLuaFunc(L, LuaFN_OneTimeBlit, "OneTimeBlit");
	AddLuaFunc(L, LuaFN_LoadImage, "LoadImage");
	AddLuaFunc(L, LuaFN_NewCStruct, "CStruct");
	AddLuaFunc(L, LuaFN_NewVulkanMemory, "VulkanMemory");
	AddLuaFunc(L, LuaFN_NewComputeShader, "ComputeShader");
	AddLuaFunc(L, LuaFN_Round, "round");

	AddLuaGlobalEnum(SF_DEFAULT);
	AddLuaGlobalEnum(SF_ALPHA);
	AddLuaGlobalEnum(SF_POSTPROCESS);
	AddLuaGlobalEnum(SF_SKYBOX);
	AddLuaGlobalEnum(SF_SHADOW);
	AddLuaGlobalEnum(SF_SUNSHADOWPASS);
	AddLuaGlobalEnum(SF_SPOTSHADOWPASS);

	AddLuaGlobalEnum(BM_OPAQUE);
	AddLuaGlobalEnum(BM_TRANSPARENT);
	AddLuaGlobalEnum(BM_ADDITIVE);
	AddLuaGlobalEnum(BM_MAX);

	engine->backend->AddLuaStuff(L);

	if (luaL_dofile(L, "engine.lua"))
	{
		PrintF("Failed to load and run script! %s\n", lua_tostring(L, -1));
		lua_pop(L, 1);
		throw std::runtime_error("Failed to run engine.lua!");
	}

	engine->backend->PostEngineLua(L);

	Lua_AddGLFWLib(L, engine->glWindow);
	Lua_AddGLMLib(L);
	Lua_AddImGuiLib(L);

	AddLuaGlobalInt(0, "SHADER_DIFFUSE");
	AddLuaGlobalInt(1, "SHADER_METAL");
	AddLuaGlobalInt(2, "SHADER_GLASS");
	AddLuaGlobalInt(3, "SHADER_SKYBOX");
	AddLuaGlobalInt(4, "SHADER_DIFFUSE_MASKED");
	AddLuaGlobalInt(5, "SHADER_METAL_MASKED");

	AddLuaGlobalEnum(CT_NONE);
	AddLuaGlobalEnum(CT_BOX);
	AddLuaGlobalEnum(CT_PERTRI);

	AddLuaGlobalEnum(PACK_NONE);
	AddLuaGlobalEnum(PACK_LEVEL);
	AddLuaGlobalEnum(PACK_CLEAN);

	lua_pushlightuserdata(L, this);
	lua_pushcclosure(L, LuaFN_LoadLevelFromFile, 1);
	lua_setglobal(L, "LoadLevelFromFile");

	lua_pushcclosure(L, LuaFN_NewCamera, 0);
	lua_setglobal(L, "NewCamera");

	lua_pushcclosure(L, LuaFN_GetActiveCamera, 0);
	lua_setglobal(L, "GetActiveCamera");

	lua_pushcclosure(L, LuaFN_SetActiveCamera, 0);
	lua_setglobal(L, "SetActiveCamera");

	lua_pushcclosure(L, LuaFN_DirectionFromAngle, 0);
	lua_setglobal(L, "DirFromAngle");

	lua_pushcclosure(L, LuaFN_GetThingsById, 0);
	lua_setglobal(L, "GetThingsById");

	lua_pushcclosure(L, LuaFN_GetThingsInRadius, 0);
	lua_setglobal(L, "GetThingsInRadius");

	lua_pushcclosure(L, LuaFN_GetAllThingsInRadius, 0);
	lua_setglobal(L, "GetAllThingsInRadius");

	lua_pushcclosure(L, LuaFN_SpawnThing, 0);
	lua_setglobal(L, "SpawnThing");

	lua_pushcclosure(L, LuaFN_TraceRay, 0);
	lua_setglobal(L, "TraceRay");

	lua_pushcclosure(L, LuaFN_TraceStep, 0);
	lua_setglobal(L, "TraceStep");

	lua_pushcclosure(L, LuaFN_NewFloat2, 0);
	lua_setglobal(L, "float2");

	lua_pushcclosure(L, LuaFN_NewFloat3, 0);
	lua_setglobal(L, "float3");

	lua_pushcclosure(L, LuaFN_NewFloat4, 0);
	lua_setglobal(L, "float4");

	lua_pushcclosure(L, LuaFN_NewMaterial, 0);
	lua_setglobal(L, "Material");

	lua_pushcclosure(L, LuaFN_MoveThingTo, 0);
	lua_setglobal(L, "MoveThingTo");

	lua_pushcclosure(L, LuaFN_Render, 0);
	lua_setglobal(L, "RenderTo");

	lua_pushcclosure(L, LuaFN_SetSubtitles, 0);
	lua_setglobal(L, "SetSubtitles");

	lua_pushcclosure(L, LuaFN_SetTimer, 0);
	lua_setglobal(L, "SetTimer");

#ifdef LGE_BACKWARDS_COMPATIBILITY
	lua_pushcclosure(L, LuaFN_GetThingsById, 0);
	lua_setglobal(L, "GetObjectsById");

	lua_pushcclosure(L, LuaFN_MoveThingTo, 0);
	lua_setglobal(L, "MoveObjectTo");
#endif

	Lua_AddSoundLib(L, engine->sound);
}

void Lua::OnGameBegin()
{
	if (luaL_dofile(L, gameLuaFilename))
	{
		PrintF("Failed to do %s: %s\n", gameLuaFilename, lua_tostring(L, -1));
		lua_pop(L, 1);
	}


	lua_getglobal(L, "GameBegin");
#ifdef _DEBUG
	if (lua_pcall(L, 0, 1, 0))
	{
		PrintF("Failed to GameBegin: %s\n", lua_tostring(L, -1));
		lua_pop(L, 1);
	}
#else
	lua_call(L, 0, 1);
#endif

	packMode = (PackMode)lua_tointeger(L, -1);

	//backend->ReadRenderProcess(L);
}

Lua::~Lua()
{
	for (auto thread : luaDelayThreads)
		delete thread;

	luaDelayThreads.clear();

	lua_close(L);
}

void Lua::InterpretConsoleCommand()
{
	ZEROMEM(consoleReadBuffer, 64);

	char* ptr = consoleBuffer;
	char* outPtr = consoleReadBuffer;

	while (!isspace(*ptr))
		*outPtr++ = *ptr++;

	while (isspace(*ptr)) ptr++;

	if (!strcmp(consoleReadBuffer, "map"))
	{
		if (FileExists(ptr))
			GetEngine()->LoadLevel_FromFile(ptr);
		else
			consoleOutput.push_back("Map does not exist!");

		return;
	}

	for (size_t i = 0; i < consoleVars.size(); i++)
	{
		if (!strcmp(consoleReadBuffer, consoleVars[i].name))
		{
			long long aslong = atoll(ptr);

			switch (consoleVars[i].type)
			{
			case CCVT_BOOL:
				*(bool*)consoleVars[i].ptr = strcmp(ptr, "false");
				break;
			case CCVT_CHAR:
				*(char*)consoleVars[i].ptr = *ptr;
				break;
			case CCVT_UCHAR:
				*(unsigned char*)consoleVars[i].ptr = (unsigned char)aslong;
				break;
			case CCVT_SHORT:
				*(short*)consoleVars[i].ptr = (short)aslong;
				break;
			case CCVT_USHORT:
				*(unsigned short*)consoleVars[i].ptr = (unsigned short)aslong;
				break;
			case CCVT_INT:
				*(int*)consoleVars[i].ptr = (int)aslong;
				break;
			case CCVT_UINT:
				*(unsigned int*)consoleVars[i].ptr = (unsigned int)aslong;
				break;
			case CCVT_LONG:
				*(long long*)consoleVars[i].ptr = aslong;
				break;
			case CCVT_ULONG:
				*(unsigned long long*)consoleVars[i].ptr = (unsigned long long)aslong;
				break;
			case CCVT_FLOAT:
				*(float*)consoleVars[i].ptr = (float)atof(ptr);
				break;
			case CCVT_DOUBLE:
				*(double*)consoleVars[i].ptr = atof(ptr);
				break;
			}
		}
	}
}

void Lua::PerFrame(long long delta)
{
	while (threadRunningLua);
	runningLua = true;
	lua_getglobal(L, "GameTick");
	lua_pushnumber(L, (lua_Number)delta);
	lua_pushboolean(L, locked);
#ifdef _DEBUG
	if (lua_pcall(L, 2, 0, 0))
	{
		PrintF("Failed to GameTick: %s\n", lua_tostring(L, -1));
		lua_pop(L, 1);
	}
#else
	lua_call(L, 2, 0);
#endif

	lua_getglobal(L, TickFunctionsName);
	if (lua_type(L, -1) != LUA_TNIL)
	{
		lua_Integer numTickFunctions = Lua_Len(L, -1);
		for (lua_Integer i = 0; i < numTickFunctions; i++)
		{
			lua_geti(L, -1, i + 1);
			lua_geti(L, -1, 2);
			lua_geti(L, -2, 1);
#ifdef _DEBUG
			if (lua_pcall(L, 1, 0, 0))
			{
				PrintF("Failed to tick object: %s", lua_tostring(L, -1));
				lua_pop(L, 1);
			}
#else
			lua_call(L, 1, 0);
#endif
			lua_pop(L, 1);
		}
	}
	runningLua = false;
}

void Lua::OnLevelBegin() const
{
	lua_getglobal(L, "LevelBegin");
#ifdef _DEBUG
	if (lua_pcall(L, 0, 0, 0))
	{
		PrintF("Failed to call LevelBegin: %s", lua_tostring(L, -1));
		lua_pop(L, 1);
	}
#else
	lua_call(L, 0, 0);
#endif
}

void Lua::OnGUIDraw()
{
	while (threadRunningLua);
	runningLua = true;

	lua_getglobal(L, "GUI");
#ifdef _DEBUG
	if (lua_pcall(L, 0, 0, 0))
	{
		PrintF("Failed to call GUI: %s\n", lua_tostring(L, -1));
		lua_pop(L, 1);
	}
#else
	lua_call(L, 0, 0);
#endif
	runningLua = false;
}

void Lua::AddThing(Thing* thing, const char* filename) const
{
	char buffer[512];

	lua_pushnil(L);
	lua_setglobal(L, "Tick");
	lua_pushnil(L);
	lua_setglobal(L, "Spawn");

	if (luaL_dofile(L, filename))
	{
		ZEROMEM(buffer, 512);
		sprintf(buffer, "Failed to load thing script %s: %s", filename, lua_tostring(L, -1));
		lua_pop(L, 1);
		throw std::runtime_error(buffer);
	}

	Lua_PushThing(L, thing);
	lua_setglobal(L, TempThingName);

	lua_getglobal(L, "Tick");
	if (lua_type(L, -1) != LUA_TNIL)
	{
		lua_getglobal(L, TickFunctionsName);
		if (lua_isnil(L, -1))
		{
			lua_pop(L, 1);
			lua_createtable(L, 1, 0);
		}

		lua_createtable(L, 2, 0);
		lua_getglobal(L, TempThingName);
		lua_seti(L, -2, 1);
		lua_rotate(L, -3, -1);
		lua_seti(L, -2, 2);

		lua_seti(L, -2, Lua_Len(L, -2) + 1);
		lua_setglobal(L, TickFunctionsName);
	}
	else
		lua_pop(L, 1);

	lua_getglobal(L, "Spawn");
	if (lua_type(L, -1) != LUA_TNIL)
	{
		lua_getglobal(L, TempThingName);
		if (lua_pcall(L, 1, 0, 0))
		{
			ZEROMEM(buffer, 512);
			sprintf(buffer, "Failed to run spawn script in %s: %s", filename, lua_tostring(L, -1));
			lua_pop(L, 1);
			throw std::runtime_error(buffer);
		}
	}
}

static bool LuaDelayThreadFunc(LuaDelayThreadData* udata)
{
	std::this_thread::sleep_for(udata->delay);

	Lua* lua = GetEngine()->lua;

	// If the any of the other threads happens to be in the middle of running Lua code, this thread has to wait till that's done
	// Having more than 1 thread share the same state is horrible, this at least helps reduce conflicts
	// The only issue I can see now is if multiple delay threads reach this point around the same time, since they'll both wait and be pretty much guarunteed to try doing their thing at the same time
	while (lua->runningLua || lua->threadRunningLua);

	lua->threadRunningLua = true;
	lua_getglobal(udata->L, udata->functionToCall);
	lua_call(udata->L, 0, 0);
	lua->threadRunningLua = false;

	return udata->once;
}

static LuaDelayThreadData* Lua_SetUpDelayThreadData(lua_State* L)
{
	auto threadData = new LuaDelayThreadData();
	threadData->delay = std::chrono::milliseconds((unsigned long long)(lua_tonumber(L, 1) * 1000.f));
	threadData->functionToCall = lua_tostring(L, 2);
	threadData->once = lua_toboolean(L, 3);
	threadData->L = L;

	return threadData;
}

void Lua::AddTimer()
{
	luaDelayThreads.push_back(new Thread((zThreadFunc)LuaDelayThreadFunc, Lua_SetUpDelayThreadData(L)));
}

int LuaFN_SetTimer(lua_State* L)
{
	GetEngine()->lua->AddTimer();
	return 0;
}

void Lua::AddConsoleVar(const char* name, void* ptr, ConsoleCommandVarType type)
{
	consoleVars.push_back({ name, ptr, type });
}