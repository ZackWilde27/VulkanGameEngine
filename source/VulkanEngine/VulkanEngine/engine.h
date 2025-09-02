#pragma once

#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include <imgui_impl_glfw.h>

#include <vector>
#include <string>
#include "engineTypes.h"

Mexel* LoadMexelFromFile(char* filename);
Texture* GetTextureImage(const char* filename, bool isNormal, bool freeFilename, bool* out_isNew);
void RecordStaticCommandBuffer();
bool LevelLoaded();

void PrintF(const char* message, ...);

void LoadLevelFromFile(const char* filename);
void FullCreateImage(VkImageType imageType, VkImageViewType imageViewType, VkFormat imageFormat, int width, int height, int mipLevels, int arrayLayers, VkSampleCountFlagBits sampleCount, VkImageTiling imageTiling, VkImageUsageFlags usage, VkImageAspectFlags imageAspectFlags, VkFilter magFilter, VkFilter minFilter, VkSamplerAddressMode samplerAddressMode, VkImage& outImage, VkDeviceMemory& outMemory, VkImageView& outView, VkSampler& outSampler, bool addSamplerToList);

MeshObject** GetObjectList(size_t& out_numObjects);
bool RayObjects(float3 rayOrigin, float3 rayDir, int id, MeshObject** outObject, float* outDst);

void AddMovingObject(MeshObject* mo, float3 moveTo, float moveSpeed, const char* callback);
void RemoveMovingObject(uint32_t index);

void GetInfoFromZLSL(const char* zlsl, size_t* outNumSamplers, size_t* outNumVBuffers, size_t* outNumPBuffers, size_t* outNumVPushBuffers, size_t* outNumPPushBuffers, size_t* outNumAttachments, char** outVShader);

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


class MyProgram
{
	VkSampler depthSampler;
	Texture normalBufferTex;

	void* iconData;
	void* iconSmallData;

public:
	GLFWwindow* glWindow;

	std::vector<VkDescriptorSet> midFrameDescriptorSet;
	VkDescriptorSet lightDescriptorSet;

	Thread* shaderCompileThread;
	Thread* pollThread;

	VulkanBackend* backend;

	// Used to check for buffers that haven't been destroyed at the end
	std::vector<VulkanMemory*> allBuffers = {};

	bool showSpotLightControls = false;
	bool showSunLightControls = false;
	int selectedSpotLight = 0;
	long long gpuTime;

	long long luaTime;
	long long waitTime;

	size_t MAX_OBJECTS = 250;

	char filename1[256];
	char filename2[256];

	bool levelLoaded = false;
	bool postprocessingflip;

	std::chrono::steady_clock::time_point start;
	std::chrono::steady_clock::time_point waitStart;

	lua_State* L;

	char consoleBuffer[64];
	//const char* consoleOutput = "This is a test of the console output window, I will try to put as much text in here as possible to see if it has automatic wrapping or if I need to do that myself";
	size_t consoleWidth = 256;
	std::vector<const char*> consoleOutput;
	char consoleReadBuffer[64];

public:
	MyProgram();
	~MyProgram();

	void CompileShaderFromFilename(const char* from, const char* to);

	void StringReplace(char* string, char subject, char replacement);
	void TurnSPVIntoFilename(const char* spv, bool bVertex, char* outString);
	void RecompileShader(Shader* pipeline);
	void RecompileComputeShader(ComputeShader* shader);
	void CheckIfShaderNeedsRecompilation(Shader* pipeline, bool reRecord);

	VkDescriptorSetLayout* GetDescriptorSetLayoutFromZLSL(const char* zlsl);

	void InitLua();

	static void DrawGUI(VkCommandBuffer commandBuffer);

	void StartShaderCompileThread();
	void EndShaderCompileThread();

	void RenderGUI();

	void Run();
	void PerFrame();

	bool RayObjects(float3 rayOrigin, float3 rayDir, int id, MeshObject** outObject, float* outDst);
	void LoadLevel_FromFile(const char* filename);

	void updateMaterialDescriptorSets(Material mat);

	// Gets an image from the filename
	// There's an optional pointer to store whether or not the texture is new, so if the filename is allocated you can free it
	Texture* GetTextureImage(const char* filename, bool isNormal, bool freeFilename, bool* out_IsNew);

	Mexel* LoadMexelFromFile(char* filename);
	Mesh* LoadMeshFromGLTF(const char* filename);

private:
	void PrintMeshObject(MeshObject* mo);

	VkCullModeFlagBits CullModeFromString(char* str);
	VkPolygonMode PolygonModeFromString(char* str);
	BlendMode BlendModeFromString(char* str);

	Shader* ReadMaterialFile(const char* filename);

	char* AddFolder(const char* folder, const char* filename);
	char* Concat(const char** strings, size_t numStrings);

	template<typename T>
	bool VectorSame(std::vector<T> v1, std::vector<T> v2);

	void MakeSafeName(char* filename);

	void GetDir(const char* filename, char* outDir, size_t& outLength);

	void InitWindow();
	void DeInitGUI();

	ImGuiContext* guiContext;
	void InitGUI();

	void DeInitWindow();

	template <typename T>
	bool InVector(std::vector<T>* list, T item);

	void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, VkImageAspectFlags aspectMask, uint32_t mipLevels, uint32_t layerCount);
	void generateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels, uint32_t layerCount);

	void FPSToFrametime();

	bool OnScreen(float3 worldPoint);

	// The resulting pointer will need to be freed at some point
	char* GetFileName(char* filename);

	bool hasStencilComponent(VkFormat format);

	void InterpretConsoleCommand();
};