#pragma once

#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include <imgui_impl_glfw.h>

#include <vector>
#include <string>
#include "engineTypes.h"

#define ENGINE_VERSION VK_MAKE_VERSION(2, 0, 0)

constexpr 	size_t MAX_OBJECTS = 250;

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


Mexel* LoadMexelFromFile(char* filename);
Texture*& LoadTexture(const char* filename, bool isNormal, bool freeFilename, bool* out_isNew);
void RecordStaticCommandBuffer();
bool LevelLoaded();

void PrintF(const char* message, ...);

void LoadLevelFromFile(const char* filename);
void FullCreateImage(VkImageType imageType, VkImageViewType imageViewType, VkFormat imageFormat, int width, int height, int mipLevels, int arrayLayers, VkSampleCountFlagBits sampleCount, VkImageTiling imageTiling, VkImageUsageFlags usage, VkImageAspectFlags imageAspectFlags, VkFilter magFilter, VkFilter minFilter, VkSamplerAddressMode samplerAddressMode, Texture* out_texture, bool addSamplerToList);

Thing** GetThingList(size_t& out_numThings);
bool RayObjects(float3 rayOrigin, float3 rayDir, int id, Thing** outThing, float* outDst);

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

	Thread* shaderCompileThread;

	// Used to check for buffers that haven't been destroyed at the end
	std::vector<VulkanMemory*> allBuffers = {};

	bool showSpotLightControls = false;
	bool showSunLightControls = false;
	int selectedSpotLight = 0;

	long long gpuTime;
	long long luaTime;
	long long waitTime;

	char filename1[256];
	char filename2[256];

	bool levelLoaded = false;

	std::chrono::high_resolution_clock::time_point start;
	std::chrono::high_resolution_clock::time_point waitStart;

	lua_State* L;

	char consoleBuffer[64];
	size_t consoleWidth = 256;
	std::vector<const char*> consoleOutput;
	char consoleReadBuffer[64];

public:
	LastGenEngine();
	~LastGenEngine();

	void CompileShaderFromFilename(const char* from, const char* to);

	void StringReplace(char* string, char subject, char replacement);
	void TurnSPVIntoFilename(const char* spv, bool bVertex, char* outString);
	void RecompileShader(Shader* pipeline);
	void RecompileComputeShader(ComputeShader* shader);
	void CheckIfShaderNeedsRecompilation(Shader* pipeline, bool reRecord);

	std::vector<VkDescriptorSetLayout> GetDescriptorSetLayoutFromZLSL(const char* zlsl, uint32_t* outAttachments);

	void InitLua();

	static void DrawGUI(VkCommandBuffer commandBuffer);

	void StartShaderCompileThread();
	void EndShaderCompileThread();

	void RenderGUI();

	void Run();
	void PerFrame();

	bool RayObjects(float3 rayOrigin, float3 rayDir, int id, Thing** outObject, float* outDst);
	void LoadLevel_FromFile(const char* filename);

	void updateMaterialDescriptorSets(Material* mat);

	// There's an optional pointer to store whether or not the texture is new, so if the filename is allocated you can free it
	Texture*& LoadTexture(const char* filename, bool isNormal, bool freeFilename, bool* out_IsNew);

	Mexel* LoadMexelFromFile(char* filename);
	Mesh* LoadMeshFromGLTF(const char* filename);

	// Given just the name of the mesh it will look in the meshes folder and load every mexel associated with it
	Mesh* LoadMesh(const char* name);

	Thing* AddThing(float3 position, float3 rotation, float3 scale, Mesh* mesh, std::vector<Material*>& materials, Texture*& shadowMap, bool isStatic, bool castsShadows, BYTE id, const char* filename, float shadowMapOffsetX = 0.0f, float shadowMapOffsetY = 0.0f, float shadowMapScale = 0.0f);

	Shader* ReadMaterialFile(const char* filename);

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

	void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, VkImageAspectFlags aspectMask, uint32_t mipLevels, uint32_t layerCount);
	void generateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels, uint32_t layerCount);

	void FPSToFrametime();

	bool OnScreen(float3 worldPoint);

	bool hasStencilComponent(VkFormat format);

	void InterpretConsoleCommand();
};
