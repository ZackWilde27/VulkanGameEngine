#include "engine.h"

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#include <stb_image.h>

#include <chrono>
#include <vector>
#include <iostream>
#include <algorithm> // Necessary for std::clamp
#include <array>
#include <thread>

#include "inireader.h"

#include "engineUtils.h"
#include "BackendUtils.h"
#include "luaUtils.h"
#include "luaSoundLib.h"

#include "luaVectorLib.h"
#include "zstring.h"
#include "engineSettings.h"
#include "luafunctions.h"

#include "VulkanBackend/Thing.h"
#include "VulkanBackend/Mesh.h"
#include "Camera.h"
#include "VulkanBackend/ComputeShader.h"
#include "VulkanBackend/SunLight.h"
#include "VulkanBackend/SpotLight.h"
#include "Thread.h"
#include "VulkanBackend/VulkanMemory.h"
#include "VulkanBackend/Shader.h"
#include "VulkanBackend/Texture.h"
#include "VulkanBackend/DescriptorSet.h"

#define VULKAN
//#define D3D12
//#define D3D9

#ifdef D3D9
#include "D3D9Backend/D3D9Backend.h"
#else
#ifdef D3D12
#include "D3D12Backend/D3D12Backend.h"
#else
#include "VulkanBackend/VulkanBackend.h"
#endif
#endif


// Default Window Size
uint32_t Width = 1280;
uint32_t Height = 720;
float resolutionScale = 2.f;

struct ThingMoveStruct
{
	Thing* target;
	float3 moveTo;
	float moveSpeed;
	const char* callback;
};

uint32_t numMovingThings;
ThingMoveStruct movingThings[50];

#define ReadSnippet_CheckBoundary(ptr, end) if (ptr >= end) { std::cout << "ReadSnippet() went past buffer!"; return (T*)L""; }

template<typename T>
static T* ReadSnippet(T* ptr, T* end, T* buffer)
{
	ReadSnippet_CheckBoundary(ptr, end);

	while (isspace(*ptr)) ptr++;

	ReadSnippet_CheckBoundary(ptr, end);

	while (*ptr == '#')
	{
		while (*ptr != '\n') ptr++;

		ReadSnippet_CheckBoundary(ptr, end);

		while (isspace(*ptr)) ptr++;

		ReadSnippet_CheckBoundary(ptr, end);
	}

	ReadSnippet_CheckBoundary(ptr, end);

	while (!isspace(*ptr))
		*buffer++ = *ptr++;

	return ++ptr;
}


static float WithinBounds(float point, float min, float max)
{
	return point < max && point > min;
}

static bool InsideBoundingBox(float3 point, float3 min, float3 max)
{
	return WithinBounds(point.x, min.x, max.x) && WithinBounds(point.y, min.y, max.y) && WithinBounds(point.z, min.z, max.z);
}

static auto startTime = std::chrono::high_resolution_clock::now();

// The returned string will need to be free'd at some point
char* ReplaceFilenameExtension(const char* filename, const char* extension, size_t extension_length)
{
	char* newStr = (char*)malloc(256);
	char i = 0;

	if (newStr)
	{
		ZEROMEM(newStr, 256);

		while (*filename != '.')
		{
			newStr[i++] = *filename;
			filename++;
		}

		StringCopy(&newStr[i], (char*)extension, extension_length);

		void* newptr = realloc(newStr, i + extension_length);
		if (newptr)
			newStr = (char*)newptr;
	}
	return newStr;
}

LastGenEngine* g_Engine;

LastGenEngine* GetEngine()
{
	return g_Engine;
}

struct LevelData_Shader
{
	const CHAR_T* vertexShaderFilename;
	const CHAR_T* pixelShaderFilename;
	ShaderFlags shaderType;
	CullMode cullMode;
	PolygonMode polygonMode;
	BlendMode alphaBlending;
	uint32_t stencilWriteValue;
	bool depthTest;
	bool depthWrite;
	bool masked;
	const CHAR_T* zlslFilename;
};

const std::vector<LevelData_Shader> shaders = {
	{ STRING("shaders/staticVert_vert.spv"), STRING("shaders/diffuse_pixl.spv"),			   SF_DEFAULT, CULL_MODE_BACK, POLYGON_MODE_FILL, BM_OPAQUE,		  0, true, false, false, STRING("shaders/diffuse.zlsl")              }, // Opaque Non-Metal
	{ STRING("shaders/staticVert_vert.spv"), STRING("shaders/metal_pixl.spv"),				   SF_DEFAULT, CULL_MODE_BACK, POLYGON_MODE_FILL, BM_OPAQUE,		  0, true, false, false, STRING("shaders/metal.zlsl")                }, // Opaque Metal
	{ STRING("shaders/staticVert_vert.spv"), STRING("shaders/glass_pixl.spv"),				   SF_ALPHA,	   CULL_MODE_BACK, POLYGON_MODE_FILL, BM_TRANSPARENT, 0, true, false, false, STRING("shaders/glass.zlsl")                }, // Glass
	{ STRING("shaders/skybox_vert.spv"),	 STRING("shaders/skybox_pixl.spv"),			   SF_SKYBOX,  CULL_MODE_BACK, POLYGON_MODE_FILL, BM_OPAQUE,		  2, true, false, false, STRING("shaders/skybox.zlsl")             }, // Skybox
	{ STRING("shaders/staticVert_vert.spv"), STRING("shaders/diffuse-masked_pixl.spv"), SF_DEFAULT, CULL_MODE_BACK, POLYGON_MODE_FILL, BM_OPAQUE,		  0, true, true, true,   STRING("shaders/diffuse-masked.zlsl") }, // Alpha Non-Metal
	{ STRING("shaders/staticVert_vert.spv"), STRING("shaders/metal-masked_pixl.spv"),  SF_DEFAULT, CULL_MODE_BACK, POLYGON_MODE_FILL, BM_OPAQUE,			  0, true, true, true,   STRING("shaders/metal-masked.zlsl")  }, // Alpha Metal
};

const float pi = 3.14159f;
const float halfpi = 1.57079f;

float moveSensitivity = 0.05f;

volatile bool threadAwaitingSync;
volatile bool threadSynced;
ThreadSyncer syncer = { threadAwaitingSync, threadSynced };

long long minFrametime = 8000;
int maxFPS = 165;

void LastGenEngine::StringReplace(char* string, char subject, char replacement)
{
	while (*string)
	{
		if (*string == subject)
			*string = replacement;

		string++;
	}
}

#define NEW(type) (type*)malloc(sizeof(type))

void LastGenEngine::DrawGUI(VkCommandBuffer commandBuffer)
{
	ImGui::Render();
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
}

void LastGenEngine::StartShaderCompileThread()
{
	threadAwaitingSync = false;
	threadSynced = false;

	shaderCompileThread = new Thread((zThreadFunc)RecompileShaderThreadProc, &syncer);
}

void LastGenEngine::EndShaderCompileThread()
{
	if (shaderCompileThread)
	{
		delete shaderCompileThread;
		shaderCompileThread = NULL;
	}
}

void LastGenEngine::RenderGUI()
{
	// GUI
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	if (lua->showConsole)
	{
		ImGui::Begin("Console");
		ImGui::BeginChild("ConsoleOutput", ImVec2((float)lua->consoleWidth, 256), ImGuiChildFlags_Borders);
		for (const char* line : lua->consoleOutput)
			ImGui::Text(line);
		ImGui::EndChild();
		if (ImGui::InputText("Command", lua->consoleBuffer, 64, ImGuiInputTextFlags_EnterReturnsTrue))
			lua->InterpretConsoleCommand();

		ImGui::End();
	}

#ifdef LGE_ENABLE_PROFILER
	if (ImGui::Begin("Profiler"))
	{
		float waitTimeMS = waitTime / 1000.f;
		float totalTime = backend->stats.frametime + waitTimeMS;
		float gpuTimeMS = gpuTime / 1000.f;

		size_t effectiveFrameRate = (size_t)roundf(1000.f / totalTime);

		ImGui::Text("%zi FPS", effectiveFrameRate);
		ImGui::Text("Total Frame Time: %fms", totalTime);
		ImGui::Text("Total CPU Time: %fms", backend->stats.frametime - gpuTimeMS);
		ImGui::Text("\tAcquire Time: %fms", backend->stats.acquireTime / 1000.f);
		ImGui::Text("\tSetup Render: %fms", backend->stats.setupRenderTime / 1000.f);
		ImGui::Text("\tCommand Buffer Record Time: %fms", backend->stats.recordTime / 1000.f);
		ImGui::Text("\tLua: %fms", luaTime / 1000.f);
		ImGui::Text("Wait Time: %fms", waitTimeMS);
		ImGui::Text("Present Time: %fms", backend->stats.presentTime / 1000.f);
		ImGui::Text("Fence Wait: %fms", gpuTimeMS);

		/*
		ImGui::Text("%i Render Passes", stats.passes);
		ImGui::Text("%i vkCmdBindPipeline\'s", stats.bound_pipelines);
		ImGui::Text("%i Blits", stats.blits);
		ImGui::Text("%i Draw Calls", stats.drawcall_count);
		ImGui::Text("%i API Calls Total", stats.api_calls);

		ImGui::Text("%i Triangles", stats.triangle_count);
		*/
		int maxFps = maxFPS;
		ImGui::DragInt("Max FPS", &maxFps);
		if (maxFps != maxFPS && maxFps > 0)
		{
			maxFPS = maxFps;
			FPSToFrametime();
		}

		backend->GUIStuff();
	}
	ImGui::End();

#endif

	if (subtitleBufferLength)
	{
		if (onScreenSubtitleIndex < subtitleBufferLength)
			onScreenSubtitleBuffer[onScreenSubtitleIndex++] = subtitleBuffer[onScreenSubtitleIndex];

		uint32_t windowH, windowW;
		backend->GetWindowSize(windowW, windowH);

		const float subtitleWindowHeight = 0.25f;

		ImVec2 windowPos{ 0, (float)windowH - ((float)windowH * subtitleWindowHeight) };
		ImVec2 windowSize{ (float)windowW, windowH - windowPos.y };

		ImGui::SetNextWindowPos(windowPos);
		ImGui::SetNextWindowSize(windowSize);
		ImGui::Begin("Subtitles", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMouseInputs | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
		{
			ImGui::SetWindowFontScale(2.5f);
			ImVec2 textSize = ImGui::CalcTextSize(onScreenSubtitleBuffer);
			float offsetX = (windowSize.x - textSize.x) * 0.5f;
			float offsetY = (windowSize.y - textSize.y) * 0.5f;
			if (offsetX > 0)
				ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);

			if (offsetY > 0)
				ImGui::SetCursorPosY(ImGui::GetCursorPosY() + offsetY);

			ImGui::Text(onScreenSubtitleBuffer);
		}
		ImGui::End();
	}

	lua->OnGUIDraw();
}

int focused = VK_TRUE;

void LastGenEngine::PerFrame()
{
	if (threadAwaitingSync)
	{
		threadSynced = true;
		return;
	}

	glfwPollEvents();

	focused = glfwGetWindowAttrib(glWindow, GLFW_FOCUSED);
	if (!focused) return;

	auto now = std::chrono::high_resolution_clock::now();
	auto delta = std::chrono::duration_cast<std::chrono::microseconds>(now - start).count();

	if (delta > minFrametime)
	{
		waitTime = std::chrono::duration_cast<std::chrono::microseconds>(now - waitStart).count();
		start = now;

		auto luaStart = std::chrono::high_resolution_clock::now();

		lua->PerFrame(delta);

		auto luaEnd = std::chrono::high_resolution_clock::now();
		luaTime = std::chrono::duration_cast<std::chrono::microseconds>(luaEnd - luaStart).count();

		backend->UpdateCamera();
		sound->PerFrame();
		RenderGUI();
		
		// Returns whether or not the swap chain needs to be re-created
		if (backend->PerFrame())
		{
			ImGui::Render();

			EndShaderCompileThread();

			backend->RecreateSwapChainStuff(resolutionScale);

			backend->AddLuaStuff(lua->L);

			if (luaL_dofile(lua->L, "engine.lua"))
			{
				PrintF(lua_tostring(lua->L, -1));
				lua_pop(lua->L, 1);
				throw std::runtime_error("Failed to run engine.lua!");
			}

			backend->ReadRenderStages(lua->L);
			backend->RecordPostProcessCommandBuffers();

			StartShaderCompileThread();
			return;
		}

		waitStart = std::chrono::high_resolution_clock::now();
		auto frameelapsed = std::chrono::duration_cast<std::chrono::microseconds>(waitStart - now);
		backend->stats.frametime = frameelapsed.count() / 1000.f;
	}
}

void LastGenEngine::FPSToFrametime()
{
	minFrametime = (long long)1000000 / (long long)maxFPS;
}

LastGenEngine::LastGenEngine()
{
	glWindow = NULL;
	g_Engine = this;
	subtitleBufferLength = 0;
	onScreenSubtitleIndex = 0;

	char* gameLuaFilename = NULL;

	INI_UInt32("screenWidth", &Width);
	INI_UInt32("screenHeight", &Height);
	INI_Int("maxFPS", &maxFPS);
	INI_Float("resolutionScale", &resolutionScale);
	INI_String("game", &gameLuaFilename);
	ReadINIFile("engine.ini");

	FPSToFrametime();

	InitWindow();

#ifdef D3D9
	backend = new D3D9Backend(glWindow, resolutionScale);
#else
	#ifdef D3D12
		backend = new D3D12Backend(glWindow, resolutionScale);
	#else
		backend = new VulkanBackend(glWindow, resolutionScale);
	#endif
#endif
	backend->AfterConstruction(resolutionScale);

	sound = new SoundEngine(&GetActiveCamera());

	lua = new Lua(this, gameLuaFilename);
	lua->AddConsoleVar("minFrametime", &minFrametime, CCVT_ULONG);
	lua->AddConsoleVar("moveSensitivity", &moveSensitivity, CCVT_FLOAT);
	lua->OnGameBegin();

	backend->InitGUI();

	start = std::chrono::high_resolution_clock::now();
	waitStart = std::chrono::high_resolution_clock::now();

	StartShaderCompileThread();
}

Thing* LastGenEngine::AddThing(float3 position, float3 rotation, float3 scale, Mesh* mesh, std::vector<Material*>& materials, Texture*& shadowMap, bool isStatic, bool castsShadows, CollisionType collision, BYTE id, const char* filename, float shadowMapOffsetX, float shadowMapOffsetY, float shadowMapScale)
{
	Thing* thing = backend->AddThing(position, rotation, scale, mesh, materials, shadowMap, isStatic, castsShadows, collision, id, shadowMapOffsetX, shadowMapOffsetY, shadowMapScale);

	if (filename)
		lua->AddThing(thing, filename);

	return thing;
}

void LastGenEngine::RemoveThing(Thing* thing)
{
	backend->RemoveThing(thing);
}

void LastGenEngine::Run()
{
	backend->PreRun(lua->packMode);

	lua->OnLevelBegin();

	while (!glfwWindowShouldClose(glWindow))
		PerFrame();
}

LastGenEngine::~LastGenEngine()
{
	EndShaderCompileThread();

	backend->WaitUntilIdle();

	delete lua;

	backend->DeInitGUI();

	backend->UnloadLevel();
	delete backend;

	delete sound;

#ifdef _DEBUG
	for (const auto i : allBuffers)
	{
		if (!i->destroyed)
		{
			PrintF("Buffer has not been destroyed from %s\n", i->origin);
		}
	}
#endif

	allBuffers.clear();

	DeInitWindow();
}

static void PrintThing(Thing* thing)
{
	PrintF("Thing: (%f, %f, %f) [%f, %f, %f], {%f, %f, %f}\n", thing->position.x, thing->position.y, thing->position.z, thing->rotation.x, thing->rotation.y, thing->rotation.z, thing->scale.x, thing->scale.y, thing->scale.z);
}

static CullMode CullModeFromString(char* str)
{
	switch (*(str + 13))
	{
		case L'N':
			return CULL_MODE_NONE;

		case L'F':
			return CULL_MODE_FRONT;

		default:
			return CULL_MODE_BACK;
	}
}

static PolygonMode PolygonModeFromString(char* str)
{
	switch (*(str + 17))
	{
		case L'P':
			return POLYGON_MODE_POINT;

		case L'L':
			return POLYGON_MODE_LINE;

		default:
			return POLYGON_MODE_FILL;
	}
}

static BlendMode BlendModeFromString(char* str)
{
	switch (*(str + 3))
	{
		case L'A':
			return BM_ADDITIVE;

		case L'T':
			return BM_TRANSPARENT;

		default:
			return BM_OPAQUE;
	}
}

constexpr size_t SNIPPET_SIZE = 32;

template<typename T>
static T* ReadSnippetMalloc(T** ptr, T* end)
{
	T* snippet = (T*)malloc(SNIPPET_SIZE * sizeof(T));
	check(snippet, "Failed to allocate in ReadMaterialFile");
	ZEROMEM(snippet, SNIPPET_SIZE);

	*ptr = ReadSnippet(*ptr, end, snippet);

	return snippet;
}

template<typename T>
static void ReadSnippetBuffer(T** ptr, T* end, T* buffer)
{
	ZEROMEM(buffer, SNIPPET_SIZE * sizeof(T));
	*ptr = ReadSnippet(*ptr, end, buffer);
}

Shader* LastGenEngine::ReadMaterialFile(const char* filename)
{
	auto mat = readFile(filename);

	char* ptr = (char*)mat.data();
	char* end = ptr + mat.size();

	char* zlsl = ReadSnippetMalloc(&ptr, end);
	char* vs = ReadSnippetMalloc(&ptr, end);
	char* ps = ReadSnippetMalloc(&ptr, end);

	char buffer[SNIPPET_SIZE];

	ReadSnippetBuffer(&ptr, end, buffer);
	CullMode cullMode = CullModeFromString(buffer);

	ReadSnippetBuffer(&ptr, end, buffer);
	PolygonMode polygonMode = PolygonModeFromString(buffer);

	ReadSnippetBuffer(&ptr, end, buffer);
	BlendMode alphaBlend = BlendModeFromString(buffer);

	ReadSnippetBuffer(&ptr, end, buffer);
	bool depthTest = WStringCompare(buffer, (char*)"true");

	ReadSnippetBuffer(&ptr, end, buffer);
	bool depthWrite = WStringCompare(buffer, (char*)"true");

	ReadSnippetBuffer(&ptr, end, buffer);
	bool masked = WStringCompare(buffer, (char*)"true");

	ReadSnippetBuffer(&ptr, end, buffer);
	uint32_t stencilWriteMask = atoi(buffer);

	return backend->AddMaterialShader(zlsl, vs, ps, SF_DEFAULT, cullMode, polygonMode, alphaBlend, depthTest, depthWrite, NULL, 0, stencilWriteMask, VK_COMPARE_OP_EQUAL, 0, 0.0f, masked);
}

static float sign(float x)
{
	return x >= 0 ? 1.f : -1.f;
}

// Used to calculate the normal of a ray-box hit based on the coordinate
static float3 StrongestAxis(float3 v)
{
	float x = abs(v.x);
	float y = abs(v.y);
	float z = abs(v.z);

	if (x > y)
	{
		if (x > z)
			return float3(sign(x), 0, 0);
	}
	else
	{
		if (y > z)
			return float3(0, sign(y), 0);
	}

	return float3(0, 0, sign(z));
}

bool LastGenEngine::RayObjects(float3& rayOrigin, float3& rayDir, BYTE id, Thing** outThing, double* outDst, float3* outNormal)
{
	float3 coords, localOrigin, localDir, normal;
	float4x4 inverseWorldMatrix;
	*outDst = INFINITY;
	double currentDst = 0;
	*outThing = NULL;
	bool hit = false;

	for (auto thing : backend->GetCollisionThingsById(id))
	{
		inverseWorldMatrix = glm::inverse(thing->matrix);
		localOrigin = inverseWorldMatrix * float4(rayOrigin, 1);
		localDir = inverseWorldMatrix * float4(rayDir, 0);

		if (thing->collisionType == CT_PERTRI)
		{
			if (HitMesh(localOrigin, localDir, thing->mesh, currentDst, normal))
			{
				if (currentDst < *outDst)
				{
					hit = true;

					*outNormal = thing->matrix * float4(normal, 0);
					*outThing = thing;
					*outDst = currentDst;
				}
			}
		}
		else
		{
			if (HitBoundingBox(thing->mesh->boundingBoxMin, thing->mesh->boundingBoxMax, localOrigin, localDir, coords))
			{
				currentDst = glm::distance(coords, localOrigin);
				if (currentDst < *outDst)
				{
					hit = true;

					float3 bboxSize = thing->mesh->boundingBoxMax - thing->mesh->boundingBoxMin;
					*outNormal = thing->matrix * float4(-StrongestAxis(((coords - thing->mesh->boundingBoxCentre) / bboxSize) - float3(0.5)), 0);
					*outThing = thing;
					*outDst = currentDst;
				}
			}
		}
	}

	return hit;
}


#define IncReadAs(x, type) *(type*)x; x += sizeof(type)

char* LastGenEngine::AddFolder(const char* folder, const char* filename)
{
	size_t l = strlen(filename) + strlen(folder);
	char* ptr = (char*)malloc(l + 1);
	check(ptr, "AddFolder: Failed to allocate filename buffer!");
	ZEROMEM(ptr, l);
	StringCopySafe(ptr, l + 1, folder);
	StringConcatSafe(ptr, l + 1, filename);
	return ptr;
}

char* LastGenEngine::Concat(const char** strings, size_t numStrings)
{
	char* concatBuffer = (char*)malloc(128);
	check(concatBuffer, "Failed to allocate memory in Concat()");
	ZEROMEM(concatBuffer, 128);
	StringCopySafe(concatBuffer, 128, strings[0]);
	for (size_t i = 1; i < numStrings; i++)
		StringConcatSafe(concatBuffer, 128, strings[i]);

	return concatBuffer;
}




void LastGenEngine::LoadLevel_FromFile(const char* filename)
{
	if (levelLoaded)
	{
		EndShaderCompileThread();
		backend->UnloadLevel();
	}

	levelFilename = new zstring(filename);

	printf("Loading Level...\n");

	auto folder = new zstring(STRING("levels/%hs/"), filename);
	auto levelFilename = new zstring(STRING("%s%hs.lvl"), (CHAR_T*)*folder, filename);

	auto data = readFile((CHAR_T*)*levelFilename);
	delete levelFilename;

	auto cubemapFilename = new zstring(STRING("%stextures/cubemap"), (CHAR_T*)*folder);
	auto skyboxFilename = new zstring(STRING("%stextures/skycube"), (CHAR_T*)*folder);
	backend->LoadCubemaps(cubemapFilename->Ptr(), skyboxFilename->Ptr());
	delete cubemapFilename;
	delete skyboxFilename;
	delete folder;

	uint32_t preExistingShaders = backend->GetNumShaders();

	printf("\tLoading Built-in Shaders...\n");
	for (const LevelData_Shader& i : shaders)
		backend->AddMaterialShader(i.zlslFilename, i.vertexShaderFilename, i.pixelShaderFilename, i.shaderType, i.cullMode, i.polygonMode, i.alphaBlending, i.depthTest, i.depthWrite, NULL, 0, 0, VK_COMPARE_OP_EQUAL, 0, 0.0f, i.masked);

	printf("\tDone!\n");

	char* ptr = data.data();

	if (!StringCompare(ptr, "LVL"))
	{
		std::cout << "LoadLevel Error: This is not a level file. (The format has changed so you might have to re-export it)";
		return;
	}

	ptr += 3;
	BYTE levelData = *ptr++;

	printf("\tLoading Lights...\n");

	float3 pos, rot, dir, colour;
	float fov;
	float3 zeros = float3(0);
	float3 ones = float3(1);
	uint32_t nameIndex;
	SpotLight* light;

	backend->SetLevelPacked((levelData & LEVEL_IS_PACKED_FLAG) != 0);

	if (levelData & LEVEL_HAS_SUN_FLAG)
	{
		rot.x = IncReadAs(ptr, float);
		rot.y = IncReadAs(ptr, float);
		rot.z = IncReadAs(ptr, float);

		rot = RotationMatrix(rot) * float4(0, 0, -1, 0);

		backend->AddSunLight(rot);
	}

	uint16_t length = IncReadAs(ptr, uint16_t);

	for (uint16_t i = 0; i < length; i++)
	{
		pos.x = IncReadAs(ptr, float);
		pos.y = IncReadAs(ptr, float);
		pos.z = IncReadAs(ptr, float);

		rot.x = IncReadAs(ptr, float);
		rot.y = IncReadAs(ptr, float);
		rot.z = IncReadAs(ptr, float);

		colour.r = IncReadAs(ptr, float);
		colour.g = IncReadAs(ptr, float);
		colour.b = IncReadAs(ptr, float);

		fov = IncReadAs(ptr, float);
		dir = RotationMatrix(rot) * float4(0, 0, -1, 0);

		nameIndex = IncReadAs(ptr, uint32_t);

		light = backend->AddSpotLight(pos, dir, colour, fov, 500.f);
		if (nameIndex)
		{
			Lua_PushSpotLight(lua->L, light);
			lua_setglobal(lua->L, data.data() + nameIndex);
		}
	}

	// TODO: Add dynamic point lights
	ptr += sizeof(uint16_t);

	printf("\tDone!\n");

	length = *(BYTE*)ptr++;

	zstring<char>* string;

	printf("\tLoading Level Shaders...\n");
	for (uint16_t i = 0; i < length; i++)
	{
		string = new zstring<char>("materials/%s.mat", data.data() + *(uint32_t*)ptr);
		ReadMaterialFile(*string);

		delete string;
		ptr += sizeof(uint32_t);
	}
	printf("\tDone!\n");

	length = *(uint16_t*)ptr;
	ptr += sizeof(uint16_t);

	bool isNew = false;

	printf("\tLoading Materials...\n");
	uint16_t preExistingMaterials = backend->GetNumMaterials();

	zstring<CHAR_T>* textureFilename;
	std::vector<TextureInfo> textures = {};
	// materials
	for (uint16_t i = 0; i < length; i++)
	{
		BYTE pipelineDex = *(BYTE*)ptr++;
		BYTE numFilenames = *(BYTE*)ptr++;
		uint32_t filenameIndex;

		for (BYTE j = 0; j < numFilenames; j++)
		{
			filenameIndex = IncReadAs(ptr, uint32_t);
			textures.push_back({ new zstring(STRING(STRINGFMT), data.data() + filenameIndex), !(*ptr++) });
		}

		float roughness = IncReadAs(ptr, float);

		backend->AddMaterial(pipelineDex + preExistingShaders, textures, roughness);

		for (auto& j : textures)
			delete j.filename;

		textures.clear();
	}
	printf("\tDone!\n");

	printf("\tLoading Meshes...\n");
	size_t preExistingMeshes = Mesh::GetAllMeshes().size();

	while (true)
	{
		if (!*ptr) break;

		Mesh::LoadMesh(ptr);
		while (*ptr++);
	}

	ptr++;
	printf("\tDone!\n");

	Thing* thing;
	uint16_t meshIndex;
	float3 scale;
	BYTE numMaterials;
	std::vector<uint16_t> materialIndex = {};
	BYTE meshID;
	bool isStatic, castsShadows;
	CollisionType collision;
	unsigned int filenameIndex;
	char* scriptFilename;
	char* globalName;
	size_t fileOffset;
	std::vector<Material*> materials;
	float3 shadowMapParams;

	do 
	{
		length = *(uint16_t*)ptr;
		ptr += sizeof(uint16_t);

		printf("\tLoading Things...\n");
		for (uint16_t i = 0; i < length; i++)
		{
			meshIndex = IncReadAs(ptr, uint16_t);
			pos.x = IncReadAs(ptr, float);
			pos.y = IncReadAs(ptr, float);
			pos.z = IncReadAs(ptr, float);
			rot.x = IncReadAs(ptr, float);
			rot.y = IncReadAs(ptr, float);
			rot.z = IncReadAs(ptr, float);
			scale.x = IncReadAs(ptr, float);
			scale.y = IncReadAs(ptr, float);
			scale.z = IncReadAs(ptr, float);
			numMaterials = *ptr++;
			for (BYTE j = 0; j < numMaterials; j++)
			{
				materialIndex.push_back(*(uint16_t*)ptr);
				ptr += sizeof(uint16_t);
			}

			isStatic = *(bool*)ptr++;
			castsShadows = *(bool*)ptr++;

			collision = (CollisionType)(*(BYTE*)ptr++);

			meshID = *(BYTE*)ptr++;
			filenameIndex = IncReadAs(ptr, unsigned int);
			scriptFilename = *(uint32_t*)ptr ? data.data() + *(uint32_t*)ptr : NULL;
			ptr += sizeof(uint32_t);

			globalName = *(uint32_t*)ptr ? data.data() + *(uint32_t*)ptr : NULL;
			ptr += sizeof(uint32_t);

			fileOffset = ptr - data.data();
			shadowMapParams = IncReadAs(ptr, float3);

			materials.resize(numMaterials);

			for (BYTE j = 0; j < numMaterials; j++)
				materials[j] = backend->GetMaterial(materialIndex[j]);

			textureFilename = new zstring(STRING(STRINGFMT), data.data() + filenameIndex);
			thing = AddThing(pos, rot, scale, Mesh::GetAllMeshes()[meshIndex + preExistingMeshes], materials, Texture::LoadTexture(*textureFilename, false), isStatic, castsShadows, collision, meshID, scriptFilename, shadowMapParams.x, shadowMapParams.y, shadowMapParams.z);
			delete textureFilename;

			thing->fileOffset = fileOffset;

			if (globalName)
			{
				Lua_PushThing(lua->L, thing);
				lua_setglobal(lua->L, globalName);
			}

			materialIndex.clear();
		}
	} while (length == (uint16_t)-1);

	printf("\tDone!\n");


	if (levelLoaded)
	{
		backend->OnWarmStart();
		StartShaderCompileThread();
	}

	backend->ReadRenderStages(lua->L);

	printf("Done!\n");

	levelLoaded = true;
}


template<typename T>
bool LastGenEngine::VectorSame(std::vector<T> v1, std::vector<T> v2)
{
	if (v1.size() == v2.size())
	{
		for (size_t i = 0; i < v1.size(); i++)
		{
			if (v1[i] != v2[i])
				return false;
		}
		return true;
	}
	return false;
}


void LastGenEngine::MakeSafeName(char* filename)
{
	while (*filename)
	{
		if (*filename == '.')
			*filename = '_';

		filename++;
	}
}

void LastGenEngine::GetDir(const char* filename, char* outDir, size_t& outLength)
{
	outLength = strlen(filename) - 1;

	while ((filename[outLength] != '/') && (outLength > 0))
		outLength--;

	// Include the ending slash, if there is one
	if (outLength)
		outLength++;

	for (size_t i = 0; i < outLength; i++)
		outDir[i] = filename[i];

}

void LastGenEngine::InitWindow()
{
	glfwInit();

	// Disable built-in OpenGL
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	// Disable resizing the window (since my engine does not handle that)
	//glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

#ifdef ENABLE_FULLSCREEN
	glWindow = glfwCreateWindow(Width, Height, "Last-Gen Engine", glfwGetPrimaryMonitor(), nullptr);
#else
	glWindow = glfwCreateWindow(Width, Height, "Last-Gen Engine", nullptr, nullptr);
#endif

	GLFWimage icons[2];
	int width0, height0, comp0, width1, height1, comp1;
	iconData = stbi_load("icon.png", &width0, &height0, &comp0, 4);
	iconSmallData = stbi_load("icon-small.png", &width1, &height1, &comp1, 4);

	icons[0].width = width0;
	icons[0].height = height0;
	icons[0].pixels = (unsigned char*)iconData;

	icons[1].width = width1;
	icons[1].height = height1;
	icons[1].pixels = (unsigned char*)iconSmallData;
	glfwSetWindowIcon(glWindow, 2, icons);

	// Apparently the icon in the taskbar doesn't update unless PollEvents is called right afterwards
	glfwPollEvents();

	glfwSetKeyCallback(glWindow, key_callback);
	glfwSetMouseButtonCallback(glWindow, mouse_callback);
	glfwSetInputMode(glWindow, GLFW_STICKY_KEYS, GLFW_FALSE);
}



void LastGenEngine::DeInitWindow()
{
	glfwDestroyWindow(glWindow);
	glfwTerminate();
	stbi_image_free(iconData);
	stbi_image_free(iconSmallData);
}

template <typename T>

bool LastGenEngine::InVector(std::vector<T>* list, T item) {
	for (size_t i = 0; i < list->size(); i++)
	{
		if ((*list)[i] == item)
			return true;
	}
	return false;
}

bool LastGenEngine::OnScreen(float3 worldPoint)
{
	return true;

	float4 screenPos = GetActiveCamera()->matrix * float4(worldPoint, 1);
	screenPos.x /= screenPos.w;
	screenPos.y /= screenPos.w;
	return screenPos.x > -1 && screenPos.x < 1 && screenPos.y > -1 && screenPos.y < 1;
}

static float3 ProjectPosition(float4x4& matrix, float4& pos)
{
	float4 screenPos = matrix * pos;
	return float3(screenPos.x / screenPos.z, screenPos.y / screenPos.z, -screenPos.z);
}

static void PrintFloat2(const char* prefix, float2& vec, const char* suffix)
{
	std::cout << prefix << "[" << vec.x << ", " << vec.y << "]" << suffix;
}

static void PrintFloat3(const char* prefix, float3& vec, const char* suffix)
{
	printf("%s[%f, %f, %f]%s", prefix, vec.x, vec.y, vec.z, suffix);
}

/*
Mesh* LastGenEngine::LoadMeshFromGLTF(const char* filename)
{
	Mesh* mesh = NULL;
	Mexel* mexel;

	cgltf_options options = {};
	cgltf_data* data = NULL;
	std::vector<Vertex> vertices = {};

	if (cgltf_parse_file(&options, filename, &data) == cgltf_result_success)
	{
		mesh = NEW(Mesh);
		check(mesh, "Failed to allocate mesh for LoadMeshFromGLTF!");
		ZEROMEM(mesh, sizeof(Mesh));

		mexel = NEW(Mexel);
		check(mexel, "Failed to allocate mexel for LoadMeshFromGLTF!");
		ZEROMEM(mexel, sizeof(Mexel));

		backend->allMexels.push_back(mexel);
		mesh->mexels = { mexel };

		if (data->meshes_count)
		{
			mexel->Filename = filename;
			cgltf_mesh* cMesh = &data->meshes[0];
			mexel->IndexBufferLength = 0;
			mexel->startingIndex = backend->AddIndexBuffer16(NULL, 0);
			for (uint32_t i = 0; i < cMesh->primitives_count; i++)
			{
				cgltf_primitive* prim = &cMesh->primitives[i];

				if (prim->type != cgltf_primitive_type_triangles)
					std::cout << "The mesh must consist of triangles";

				mexel->IndexBufferLength += (uint32_t)prim->indices->count;
				switch (prim->indices->component_type)
				{
					case cgltf_component_type_r_16u:
						backend->AddIndexBuffer16((uint16_t*)((size_t)data->bin + cMesh->primitives[i].indices->buffer_view->offset), (uint32_t)cMesh->primitives[i].indices->count);
						break;
					case cgltf_component_type_r_32u:
						backend->AddIndexBuffer32((uint32_t*)((size_t)data->bin + cMesh->primitives[i].indices->buffer_view->offset), (uint32_t)cMesh->primitives[i].indices->count);
						break;
					default:
						std::cout << "Invalid index type\n";
						break;
				}

				cgltf_attribute* positions = NULL;
				cgltf_attribute* uvs = NULL;
				cgltf_attribute* lightmapUVs = NULL;
				cgltf_attribute* normals = NULL;
				cgltf_attribute* tangents = NULL;
				for (uint32_t j = 0; j < prim->attributes_count; j++)
				{
					switch (prim->attributes[j].type)
					{
						case cgltf_attribute_type_texcoord:
							if (uvs)
								lightmapUVs = &prim->attributes[j];
							else
								uvs = &prim->attributes[j];
							break;
						case cgltf_attribute_type_position:
							positions = &prim->attributes[j];
							break;
						case cgltf_attribute_type_normal:
							normals = &prim->attributes[j];
							break;
						case cgltf_attribute_type_tangent:
							tangents = &prim->attributes[j];
							break;
					}
				}

				if (!positions)
				{
					std::cout << "Mesh has no positions!\n";
					goto Exit;
				}

				if (!normals)
				{
					std::cout << "Mesh has no normals!\n";
					goto Exit;
				}

				if (!tangents)
				{
					std::cout << "Mesh has no tangents!\n";
					goto Exit;
				}

				if (!uvs)
				{
					std::cout << "Mesh has no UVs!\n";
					goto Exit;
				}

				if (!lightmapUVs)
				{
					std::cout << "Mesh has no lightmap UVs!";
					goto Exit;
				}

				std::cout << positions->data->count << ", " << normals->data->count << ", " << tangents->data->count << ", " << uvs->data->count << ", " << lightmapUVs->data->count << "\n";

				float3* posPtr = (float3*)((size_t)data->bin + positions->data->buffer_view->offset);
				float3* nrmPtr = (float3*)((size_t)data->bin + normals->data->buffer_view->offset);
				float4* tangentPtr = (float4*)((size_t)data->bin + tangents->data->buffer_view->offset);
				float2* uv1Ptr = (float2*)((size_t)data->bin + uvs->data->buffer_view->offset);
				float2* uv2Ptr = (float2*)((size_t)data->bin + lightmapUVs->data->buffer_view->offset);

				Vertex v;
				uint16_t* indexPtr = (uint16_t*)((size_t)data->bin + prim->indices->buffer_view->offset);
				for (cgltf_size i = 0; i < prim->indices->count; i++)
				{
					printf("%u: ", (uint16_t)*indexPtr);
					PrintFloat3("Vertex: ", posPtr[*indexPtr], " ");
					PrintFloat3("", nrmPtr[*indexPtr], " ");
					PrintFloat3("", (float3&)tangentPtr[*indexPtr], " ");
					PrintFloat2("", uv1Ptr[*indexPtr], " ");
					PrintFloat2("", uv2Ptr[*indexPtr], "\n");
					indexPtr += 2;
				}

				assert(false);

				for (uint32_t j = 0; j < positions->data->count; j++)
				{
					PrintFloat3("Vertex: ", *posPtr, " ");
					PrintFloat3("", *nrmPtr, " ");
					PrintFloat3("", (float3&)tangentPtr, " ");
					PrintFloat2("", *uv1Ptr, " ");
					PrintFloat2("", *uv2Ptr, "\n");

					v.pos.x = IncReadAs(posPtr, float);
					v.pos.y = IncReadAs(posPtr, float);
					v.pos.z = IncReadAs(posPtr, float);
					v.nrm.x = IncReadAs(nrmPtr, float);
					v.nrm.y = IncReadAs(nrmPtr, float);
					v.nrm.z = IncReadAs(nrmPtr, float);
					v.tangent.x = IncReadAs(tangentPtr, float);
					v.tangent.y = IncReadAs(tangentPtr, float);
					v.tangent.z = IncReadAs(tangentPtr, float);
					v.uv.x = IncReadAs(uv1Ptr, float);
					v.uv.y = IncReadAs(uv1Ptr, float);
					v.uv.z = IncReadAs(uv2Ptr, float);
					v.uv.w = IncReadAs(uv2Ptr, float);

					vertices.push_back(v);

					//posPtr += positions->data->stride;
					//nrmPtr += normals->data->stride;
					//tangentPtr += tangents->data->stride;
					//uv1Ptr += uvs->data->stride;
					//uv2Ptr += lightmapUVs->data->stride;
				}
			}

			mexel->startingVertex = backend->AddVertexBuffer(vertices.data(), (uint32_t)vertices.size());
		}
		else
			std::cout << "This GLTF file has no mesh data!\n";

Exit:
		cgltf_free(data);
	}

	return mesh;
}
*/

bool LastGenEngine::hasStencilComponent(VkFormat format)
{
	return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

void LoadLevelFromFile(const char* filename)
{
	g_Engine->LoadLevel_FromFile(filename);
}

void RecordStaticCommandBuffer()
{
	g_Engine->backend->RecordPostProcessCommandBuffers();
}

void AddMovingThing(Thing* thing, float3 moveTo, float moveSpeed, const char* callback)
{
	movingThings[numMovingThings++] = { thing, moveTo, moveSpeed, callback };
}

void RemoveMovingThing(uint32_t index)
{
	numMovingThings--;

	for (uint32_t i = index; i < numMovingThings; i++)
		movingThings[i] = movingThings[i + 1];
}

bool RayObjects(float3& rayOrigin, float3& rayDir, BYTE id, Thing** outThing, double* outDst, float3* outNormal)
{
	return g_Engine->RayObjects(rayOrigin, rayDir, id, outThing, outDst, outNormal);
}

int Lua_TextureGC(lua_State* L)
{
	lua_getfield(L, 1, "texture");
	Texture** tex = (Texture**)lua_touserdata(L, -1);
	lua_pop(L, 1);

	delete *tex;
	*tex = NULL;

	return 0;
}

int LuaFN_SetActiveCamera(lua_State* L)
{
	lua_getfield(L, 1, LUA_DATA_NAME);
		SetActiveCamera((Camera*)lua_touserdata(L, -1));
	lua_pop(L, 1);
	return 0;
}

int LuaFN_GetActiveCamera(lua_State* L)
{
	Lua_PushCamera(L, GetActiveCamera());
	return 1;
}

int LuaFN_NewMaterial(lua_State* L)
{
	std::vector<Texture*> textures = {};

	uint32_t shaderIndex;
	if (lua_type(L, 1) == LUA_TSTRING)
		shaderIndex = g_Engine->backend->IndexOfShader(g_Engine->ReadMaterialFile(lua_tostring(L, 1)));
	else
		shaderIndex = lua_tointeger(L, 1);

	lua_Integer numTextures = Lua_Len(L, 2);

	for (lua_Integer i = 0; i < numTextures; i++)
	{
#ifdef LGE_BACKWARDS_COMPATIBILITY
		lua_geti(L, 2, i + 1);
		lua_geti(L, -1, 1);
		lua_geti(L, -2, 2);
		textures.push_back(LoadTexture(lua_tostring(L, -2), lua_toboolean(L, -1), false, NULL));
		lua_pop(L, 3);
#else
		lua_geti(L, 2, i + 1);
		lua_getfield(L, -1, "texture");
		textures.push_back(*(Texture**)lua_touserdata(L, -1));
		lua_pop(L, 2);
#endif
	}

	lua_pushlightuserdata(L, g_Engine->backend->AddMaterial(shaderIndex, textures, lua_tonumber(L, 3)));

	return 1;
}

int LuaFN_SpawnThing(lua_State* L)
{
	std::vector<Material*> materials;

	lua_Integer numTextures = Lua_Len(L, 5);
	for (lua_Integer i = 0; i < numTextures; i++)
	{
		lua_geti(L, 5, i + 1);
		materials.push_back((Material*)lua_touserdata(L, -1));
		lua_pop(L, 1);
	}

	zstring<CHAR_T>* shadowMapFilename = Lua_ToString(L, 6);
	Thing* thing = g_Engine->AddThing(*LuaData<float3>(L, 1), *LuaData<float3>(L, 2), *LuaData<float3>(L, 3), Mesh::LoadMesh((char*)lua_tostring(L, 4)), materials, Texture::LoadTexture(*shadowMapFilename, true), lua_toboolean(L, 7), lua_toboolean(L, 8), (CollisionType)lua_tointeger(L, 9), (BYTE)lua_tointeger(L, 10), lua_type(L, 11) ? lua_tostring(L, 11) : NULL);

	Lua_PushThing(L, thing);
	return 1;
}

int LuaFN_OneTimeBlit(lua_State* L)
{
	lua_getfield(L, 1, "texture");
	Texture** src = (Texture**)lua_touserdata(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, 2, "texture");
	Texture** dst = (Texture**)lua_touserdata(L, -1);
	lua_pop(L, 1);

	auto filter = (VkFilter)lua_tointeger(L, 3);

	auto srcLayout = (VkImageLayout)lua_tointeger(L, 4);

	auto srcFinalLayout = (VkImageLayout)lua_tointeger(L, 5);
	auto dstFinalLayout = (VkImageLayout)lua_tointeger(L, 6);

	(*src)->BlitTo(*dst, filter, NULL, 0, 0, NULL, 0, 0);
	//g_Engine->backend->OneTimeBlit(*src, srcArea, *dst, dstArea, srcLayout, filter, (*src)->aspect, (*dst)->aspect, srcFinalLayout, dstFinalLayout);
	g_Engine->backend->WaitUntilIdle();

	return 0;
}

int LuaFN_LoadImage(lua_State* L)
{
	zstring<CHAR_T>* filename = Lua_ToString(L, 1);
	Texture*& tex = Texture::LoadTexture(*filename, lua_toboolean(L, 2));
	delete filename;
	Lua_PushTexture_NoGC(L, &tex, tex->size.x, tex->size.y);
	return 1;
}


int LuaFN_Render(lua_State* L)
{
	/*
	auto cam = LuaData<Camera>(L, 1);
	
	lua_getfield(L, 2, "renderPass");
	auto renderPass = Lua_GetRenderPass(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, 2, "width");
	auto width = lua_tonumber(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, 2, "height");
	auto height = lua_tonumber(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, 2, "buffer");
	auto framebuffer = (VkFramebuffer)lua_touserdata(L, -1);
	lua_pop(L, 1);

	VkRect2D renderArea;
	renderArea.extent = { (uint32_t)width, (uint32_t)height };
	renderArea.offset = { 0, 0 };

	lua_Integer numClearValues = Lua_Len(L, 3);
	auto clearValues = (VkClearValue*)malloc(numClearValues * sizeof(VkClearValue));

	g_Engine->backend->RenderTo(cam, framebuffer, renderArea, (uint32_t)numClearValues, clearValues);

	free(clearValues);
	*/
	return 0;
}


int LuaFN_SetSubtitles(lua_State* L)
{
	g_Engine->SetSubtitleText(lua_tostring(L, 1), lua_gettop(L) > 1 ? lua_toboolean(L, 2) : true);
	return 0;
}

struct LuaCStructItem
{
	zstring<char>* name;
	int type;
	size_t index;
};

static size_t Align(size_t index, size_t alignment)
{
	while (index % alignment)
		index++;

	return index;
}

#define GetAtIndex(ptr, index, type) *(type*)((uintptr_t)ptr + index)

#define NewCStruct_UData(type)	GetAtIndex(buffer, index, type) = *LuaData<type>(L, -1); \
													index += sizeof(type);

static int LuaFN_CStructGC(lua_State* L)
{
	auto data = LuaData<void>(L, 1);
	lua_getfield(L, 1, "legend");
	auto legend = (LuaCStructItem*)lua_touserdata(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, 1, "count");
	lua_Integer count = lua_tointeger(L, -1);
	lua_pop(L, 1);

	free(data);

	for (lua_Integer i = 0; i < count; i++)
		delete legend[i].name;

	free(legend);

	return 0;
}

static int LuaFN_CStructIndex(lua_State* L)
{
	auto data = LuaData<void>(L, 1);
	lua_getfield(L, 1, "legend");
	auto legend = (LuaCStructItem*)lua_touserdata(L, -1);
	lua_pop(L, 1);

	LuaCStructItem* item;

	if (lua_isnumber(L, 2))
		item = &legend[lua_tointeger(L, 2) - 1];
	else
	{
		const char* key = lua_tostring(L, 2);
		lua_getfield(L, 1, "count");
		lua_Integer count = lua_tointeger(L, -1);
		lua_pop(L, 1);

		for (lua_Integer i = 0; i < count; i++)
		{
			if (!strcmp(*legend[i].name, key))
			{
				item = &legend[i];
				goto ValidKey;
			}
		}

		lua_pushnil(L);
		return 1;
	}

ValidKey:
	switch (item->type)
	{
		case LUA_TBOOLEAN:
			lua_pushboolean(L, GetAtIndex(data, item->index, bool));
			break;

		case LUA_TNUMBER:
			lua_pushnumber(L, GetAtIndex(data, item->index, float));
			break;

		case MAKESHORT("f2"):
			Lua_PushFloat2(L, &GetAtIndex(data, item->index, float2));
			break;

		case MAKESHORT("f3"):
			Lua_PushFloat3(L, &GetAtIndex(data, item->index, float3));
			break;

		case MAKESHORT("f4"):
			Lua_PushFloat4(L, &GetAtIndex(data, item->index, float4));
			break;

		case MAKESHORT("m4"):
			Lua_PushFloat4x4(L, &GetAtIndex(data, item->index, float4x4));
			break;
	}

	return 1;
}

static int LuaFN_CStructNewIndex(lua_State* L)
{
	auto data = LuaData<void>(L, 1);
	lua_getfield(L, 1, "legend");
	auto legend = (LuaCStructItem*)lua_touserdata(L, -1);
	lua_pop(L, 1);

	LuaCStructItem* item;

	if (lua_isnumber(L, 2))
		item = &legend[lua_tointeger(L, 2) - 1];
	else
	{
		const char* key = lua_tostring(L, 2);
		lua_getfield(L, 1, "count");
		lua_Integer count = lua_tointeger(L, -1);
		lua_pop(L, 1);

		for (lua_Integer i = 0; i < count; i++)
		{
			if (!strcmp(*legend[i].name, key))
			{
				item = &legend[i];
				goto ValidKey;
			}
		}

		lua_pushnil(L);
		return 1;
	}

ValidKey:
	switch (item->type)
	{
	case LUA_TBOOLEAN:
		GetAtIndex(data, item->index, bool) = lua_toboolean(L, 3);
		break;

	case LUA_TNUMBER:
		GetAtIndex(data, item->index, float) = (float)lua_tonumber(L, 3);
		break;

	case MAKESHORT("f2"):
		GetAtIndex(data, item->index, float2) = *LuaData<float2>(L, 3);
		break;

	case MAKESHORT("f3"):
		GetAtIndex(data, item->index, float3) = *LuaData<float3>(L, 3);
		break;

	case MAKESHORT("f4"):
		GetAtIndex(data, item->index, float4) = *LuaData<float4>(L, 3);
		break;

	case MAKESHORT("m4"):
		GetAtIndex(data, item->index, float4x4) = *LuaData<float4x4>(L, 3);
		break;
	}

	return 0;
}

static int LuaFN_CStructLen(lua_State* L)
{
	lua_getfield(L, 1, "count");

	return 1;
}

static void Lua_CreateCStructMetatable(lua_State* L)
{
	lua_createtable(L, 0, 3);

	lua_pushcclosure(L, LuaFN_CStructIndex, 0);
	lua_setfield(L, -2, "__index");

	lua_pushcclosure(L, LuaFN_CStructNewIndex, 0);
	lua_setfield(L, -2, "__newindex");

	lua_pushcclosure(L, LuaFN_CStructLen, 0);
	lua_setfield(L, -2, "__len");
}

static void Lua_PushCStruct(lua_State* L, void* buffer, LuaCStructItem* legend, lua_Integer sizeInBytes, lua_Integer numItems)
{
	lua_createtable(L, 0, 2);

	lua_pushlightuserdata(L, buffer);
	lua_setfield(L, -2, LUA_DATA_NAME);

	lua_pushlightuserdata(L, legend);
	lua_setfield(L, -2, "legend");

	lua_pushinteger(L, sizeInBytes);
	lua_setfield(L, -2, "size");

	lua_pushinteger(L, numItems);
	lua_setfield(L, -2, "count");


	Lua_CreateCStructMetatable(L);

	lua_pushcclosure(L, LuaFN_CStructGC, 0);
	lua_setfield(L, -2, "__gc");

	lua_setmetatable(L, -2);
}

int LuaFN_NewCStruct(lua_State* L)
{
	size_t index = 0;
	std::vector<LuaCStructItem> items = {};

	int numArgs = lua_gettop(L);

	// To make it so only 1 loop needs to be done, it allocates the maximum possible size (assuming every item is the largest type), and will realloc at the end
	void* buffer = malloc(numArgs * sizeof(float4x4));

	lua_pushnil(L);
	while (lua_next(L, 1))
	{
		int luaType = lua_type(L, -1);

		if (luaType != LUA_TBOOLEAN)
		{
			index = Align(index, 4);

			if (luaType == LUA_TTABLE)
			{
				index = Align(index, sizeof(float4));
				luaType = *(unsigned short*)Lua_GetLGEType(L, -1);
			}
		}

		items.push_back({ new zstring(lua_tostring(L, -2)), luaType, index });

		switch (luaType)
		{
			case LUA_TBOOLEAN:
				GetAtIndex(buffer, index, bool) = lua_toboolean(L, -1);
				index++;
				break;

			case LUA_TNUMBER:
				GetAtIndex(buffer, index, float) = (float)lua_tonumber(L, -1);
				index += sizeof(float);
				break;

			case MAKESHORT("f2"):
				NewCStruct_UData(float2);
				break;

			case MAKESHORT("f3"):
				NewCStruct_UData(float3);
				break;

			case MAKESHORT("f4"):
				NewCStruct_UData(float4);
				break;

			case MAKESHORT("m4"):
				NewCStruct_UData(float4x4);
				break;
		}

		lua_pop(L, 1);
	}

	lua_pop(L, 1);

	void* newPtr = realloc(buffer, index);
	if (newPtr)
		buffer = newPtr;

	size_t legendSize = items.size() * sizeof(LuaCStructItem);
	void* legend = malloc(legendSize);
	check(legend, "Failed to allocate memory in LuaFN_NewCStruct()");
	memcpy(legend, items.data(), legendSize);

	Lua_PushCStruct(L, buffer, (LuaCStructItem*)legend, index, items.size());

	return 1;
}

static int LuaFN_VulkanMemoryGC(lua_State* L)
{
	auto buffer = LuaData<GPUMemory>(L, 1);

	delete buffer;

	return 0;
}

static int LuaFN_VulkanMemoryUpdate(lua_State* L)
{
	auto buffer = (GPUMemory*)lua_touserdata(L, lua_upvalueindex(1));
	auto data = LuaData<void>(L, 1);

	void* d = buffer->Map();
	memcpy(d, data, buffer->size);
	buffer->UnMap();

	return 0;
}

static int LuaFN_VulkanMemoryMap(lua_State* L)
{
	auto buffer = (GPUMemory*)lua_touserdata(L, lua_upvalueindex(1));

	lua_createtable(L, 0, 4);

	lua_pushlightuserdata(L, buffer->Map());
	lua_setfield(L, -2, LUA_DATA_NAME);

	lua_pushvalue(L, lua_upvalueindex(2));
	lua_setfield(L, -2, "legend");

	lua_pushinteger(L, buffer->size);
	lua_setfield(L, -2, "size");

	lua_pushvalue(L, lua_upvalueindex(3));
	lua_setfield(L, -2, "count");

	Lua_CreateCStructMetatable(L);
	lua_setmetatable(L, -2);

	return 1;
}

static int LuaFN_VulkanMemoryUnMap(lua_State* L)
{
	auto buffer = (GPUMemory*)lua_touserdata(L, lua_upvalueindex(1));

	buffer->UnMap();

	return 0;
}

int LuaFN_NewVulkanMemory(lua_State* L)
{
	auto data = LuaData<void>(L, 1);

	lua_getfield(L, 1, "size");
	lua_Integer size = lua_tointeger(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, 1, "legend");
	auto legend = (LuaCStructItem*)lua_touserdata(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, 1, "count");
	lua_Integer count = lua_tointeger(L, -1);
	lua_pop(L, 1);

	auto buffer = new GPUMemory(size, (VkBufferUsageFlags)lua_tointeger(L, 2), "LuaGPUMemory", lua_toboolean(L, 3), data);

	Lua_PushDataWithGC(L, buffer, LuaFN_VulkanMemoryGC);

	lua_pushlightuserdata(L, buffer);
	lua_pushcclosure(L, LuaFN_VulkanMemoryUpdate, 1);
	lua_setfield(L, -2, "Update");

	lua_pushlightuserdata(L, buffer);

	size_t legendSize = count * sizeof(LuaCStructItem);
	void* newLegend = lua_newuserdata(L, legendSize);
	memcpy(newLegend, legend, legendSize);

	lua_pushinteger(L, count);
	lua_pushcclosure(L, LuaFN_VulkanMemoryMap, 3);
	lua_setfield(L, -2, "Map");

	lua_pushlightuserdata(L, buffer);
	lua_pushcclosure(L, LuaFN_VulkanMemoryUnMap, 1);
	lua_setfield(L, -2, "UnMap");

	return 1;
}

int LuaFN_GLFWSetCursor(lua_State* L)
{
	Texture* image;
	GLFWimage glfwImage;
	GLFWcursor* cursor;
	std::vector<float4> pixels;

	auto glWindow = (GLFWwindow*)lua_touserdata(L, lua_upvalueindex(1));

	if (lua_isnil(L, 1))
	{
		cursor = NULL;
		goto SkipLoadingCursor;
	}

	image = LuaData<Texture>(L, 1);

	glfwImage.width = image->size.x;
	glfwImage.height = image->size.y;
	pixels = image->CopyToBuffer();
	glfwImage.pixels = (unsigned char*)pixels.data();
	
	cursor = glfwCreateCursor(&glfwImage, (int)lua_tointeger(L, 2), (int)lua_tointeger(L, 3));

SkipLoadingCursor:
	glfwSetCursor(glWindow, cursor);

	return 0;
}



void LastGenEngine::SetSubtitleText(const char* text, bool reset)
{
	size_t length = strlen(text);
	if (length >= SUBTITLE_BUFFER_SIZE)
	{
		std::cout << "SetSubtitleText: Cannot fit text onto subtitle buffer!\n";
		return;
	}

	subtitleBufferLength = (uint32_t)length;

	StringCopySafe(subtitleBuffer, 256, text);

	if (reset)
	{
		ZEROMEM(onScreenSubtitleBuffer, SUBTITLE_BUFFER_SIZE);
		onScreenSubtitleIndex = 0;
	}
}