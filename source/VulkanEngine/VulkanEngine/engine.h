#pragma once

#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include <imgui_impl_glfw.h>

#include <vector>
#include <string>
#include "engineTypes.h"
#include "VulkanBackend.h"
#include "sound.h"
#include "Mesh.h"
#include "Lua.h"

void RecordStaticCommandBuffer();
bool LevelLoaded();

void PrintF(const char* message, ...);

void LoadLevelFromFile(const char* filename);

std::vector<std::vector<Thing*>>& GetThingList();
bool RayObjects(float3& rayOrigin, float3& rayDir, BYTE id, Thing** outThing, double* outDst, float3* outNormal);

void AddMovingThing(Thing* mo, float3 moveTo, float moveSpeed, const char* callback);
void RemoveMovingThing(uint32_t index);

class LastGenEngine
{
	void* iconData;
	void* iconSmallData;

	ImGuiContext* guiContext;

public:
	GLFWwindow* glWindow;
	VulkanBackend* backend;
	SoundEngine* sound;

	Thread* shaderCompileThread;

	// Used to check for buffers that haven't been destroyed at the end
	std::vector<VulkanMemory*> allBuffers = {};

	bool showSpotLightControls = false;
	bool showSunLightControls = false;
	uint32_t selectedSpotLight = 0;

	long long gpuTime;
	long long luaTime;
	long long waitTime;

	CHAR_T filename1[256];
	CHAR_T filename2[256];

	bool levelLoaded = false;

	std::chrono::high_resolution_clock::time_point start;
	std::chrono::high_resolution_clock::time_point waitStart;

	Lua* lua;

	char subtitleBuffer[SUBTITLE_BUFFER_SIZE];
	uint32_t subtitleBufferLength;
	char onScreenSubtitleBuffer[SUBTITLE_BUFFER_SIZE];
	uint32_t onScreenSubtitleIndex;

public:
	LastGenEngine();
	~LastGenEngine();

	void CompileShaderFromFilename(const CHAR_T* from, const CHAR_T* to);

	void StringReplace(char* string, char subject, char replacement);
	void TurnSPVIntoFilename(const CHAR_T* spv, bool bVertex, CHAR_T* outString);
	void RecompileShader(Shader* pipeline);
	void RecompileComputeShader(ComputeShader* shader);

	static void DrawGUI(VkCommandBuffer commandBuffer);

	void StartShaderCompileThread();
	void EndShaderCompileThread();

	void RenderGUI();

	void Run();
	void PerFrame();

	bool RayObjects(float3& rayOrigin, float3& rayDir, BYTE id, Thing** outObject, double* outDst, float3* outNormal);
	void LoadLevel_FromFile(const char* filename);
	void PackLevel();

	void updateMaterialDescriptorSets(Material* mat);

	//Mesh* LoadMeshFromGLTF(const char* filename);

	Thing* AddThing(float3 position, float3 rotation, float3 scale, Mesh* mesh, std::vector<Material*>& materials, Texture*& shadowMap, bool isStatic, bool castsShadows, CollisionType collision, BYTE id, const char* filename, float shadowMapOffsetX = 0.0f, float shadowMapOffsetY = 0.0f, float shadowMapScale = 0.0f);
	void RemoveThing(Thing* thing);

	Shader* ReadMaterialFile(const char* filename);

	void SetSubtitleText(const char* text, bool reset);

	void Lua_AddSwapChainStuff(lua_State* L);

private:
	char* AddFolder(const char* folder, const char* filename);
	char* Concat(const char** strings, size_t numStrings);

	template<typename T>
	bool VectorSame(std::vector<T> v1, std::vector<T> v2);

	void MakeSafeName(char* filename);

	void GetDir(const char* filename, char* outDir, size_t& outLength);

	void InitWindow();
	void DeInitGUI();
	void InitGUI();

	void DeInitWindow();

	template <typename T>
	bool InVector(std::vector<T>* list, T item);

	void FPSToFrametime();

	bool OnScreen(float3 worldPoint);

	bool hasStencilComponent(VkFormat format);

	void DeleteShadowMaps();
};

LastGenEngine* GetEngine();
