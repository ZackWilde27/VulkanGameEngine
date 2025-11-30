#pragma once
#include "engineTypes.h"
#include "VulkanBackend/Thing.h"

#define TempThingName "TEMPOBJECT"
#define TickFunctionsName "TICKFUNCTIONS"

struct LuaDelayThreadData
{
	const char* functionToCall;
	lua_State* L;
	std::chrono::milliseconds delay;
	bool once;
};

enum ConsoleCommandVarType
{
	CCVT_BOOL,
	CCVT_CHAR,
	CCVT_UCHAR,
	CCVT_SHORT,
	CCVT_USHORT,
	CCVT_INT,
	CCVT_UINT,
	CCVT_LONG,
	CCVT_ULONG,
	CCVT_FLOAT,
	CCVT_DOUBLE
};

struct ConsoleCommandVar
{
	const char* name;
	void* ptr;
	ConsoleCommandVarType type;
};

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
void mouse_callback(GLFWwindow* window, int button, int action, int mods);

class Lua
{
private:
	std::vector<Thread*> luaDelayThreads = {};
	char consoleReadBuffer[64];

public:
	lua_State* L;
	PackMode packMode;

	bool showConsole = false;
	size_t consoleWidth = 256;
	char consoleBuffer[64];
	std::vector<const char*> consoleOutput;

	char* gameLuaFilename;

	// If the main thread is currently running lua code, then timer threads needs to wait
	volatile bool runningLua;
	// If a timer thread is currently running lua code, then the main thread needs to wait
	volatile bool threadRunningLua;

	Lua(class LastGenEngine* engine, char* gameLuaFilename);
	~Lua();

	void OnGUIDraw();
	void OnLevelBegin() const;
	void PerFrame(long long delta);

	void OnGameBegin();

	void AddThing(Thing* thing, const char* filename) const;

	void AddTimer();

	void InterpretConsoleCommand();

	void AddConsoleVar(const char* name, void* ptr, ConsoleCommandVarType type);
};