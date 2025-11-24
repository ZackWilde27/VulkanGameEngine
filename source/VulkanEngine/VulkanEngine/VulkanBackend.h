#pragma once
#include "engineTypes.h"
#include "zstring.h"

#include "ComputeShader.h"
#include "Mesh.h"
#include "SunLight.h"
#include "VulkanMemory.h"

#include <array>
#include <vector>

constexpr size_t MAX_SPOT_LIGHTS = 50;
#define SPOT_LIGHT_INDEX uint8_t

constexpr size_t MAX_TEXTURES = 2048;
#define TEXTURE_INDEX uint16_t

constexpr size_t MAX_MATERIALS = 2048;
#define MATERIAL_INDEX uint16_t

constexpr size_t MAX_THINGS = 4096;
#define THING_INDEX uint16_t

constexpr size_t FONT_NAME_SIZE = 32;
#define FONT_NAME_INDEX uint8_t

struct QueueFamilyIndices
{
	uint32_t graphicsFamily;
	bool hasGraphicsFamily;
	uint32_t presentFamily;
	bool hasPresentFamily;
};

struct SwapChainSupportDetails
{
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};

struct CombinedBufferAndDescriptorSet
{
	VulkanMemory* buffer;
	VkDescriptorSet descriptorSet;
};

struct UIInstance
{
	float4 point;
	Texture* texture;
	bool isStatic;
};

struct Font3D
{
	CHAR_T* legend;
	size_t legendLength;
	Mexel** letters;
	CHAR_T fontName[FONT_NAME_SIZE];
};

// When packing things into a level, the shadow map isn't built so objects that get spawned during GameBegin need extra information to be included
struct PackedLevelThingShadowMapStruct
{
	THING_INDEX index;
	float2 shadowMapOffsets;
	float shadowMapScale;
};

class Font3DInstance
{
	zstring<CHAR_T>* text;
	VulkanMemory* worldMatrix;
	VulkanMemory* indexBuffer;
	VulkanMemory* vertexBuffer;
	VkDescriptorSet descriptorSet;

public:
	Font3DInstance(VulkanBackend* backend, const CHAR_T* fontName, const CHAR_T* text, float3& position, float3& rotation, float3& scale, bool isStatic);

	~Font3DInstance()
	{
		delete worldMatrix;
		delete indexBuffer;
		delete vertexBuffer;
		free(text);
	}

	void SetText(const CHAR_T* string)
	{
		if (text)
			delete text;

		text = new zstring(string);
	}

	void SetTransform(float3& position, float3& rotation, float3& scale);

	void Update()
	{
		if (indexBuffer)
			delete indexBuffer;

		if (vertexBuffer)
			delete vertexBuffer;
	}
};

const char* String_VkResult(VkResult vr);
inline VkCommandPoolCreateInfo MakeCommandPoolCreateInfo(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags);
bool MeshGroupOnScreen(RenderStageMeshGroup* meshGroup, float3& camPos, float3& camDir);
Camera*& GetActiveCamera();
void SetActiveCamera(Camera* camera);

class VulkanBackend
{
	VkSurfaceKHR surface;

	VkQueue presentQueue;
	float queuePriority;

	uint32_t imageIndex;

	GLFWwindow* glWindow;

	VkSubmitInfo submitInfo{};
	VkPresentInfoKHR presentInfo{};
	VkRenderPassBeginInfo renderPassInfo{};
	VkRenderPassBeginInfo depthPrepassBeginInfo{};
	VkRenderPassBeginInfo GUIBeginInfo{};

	VkViewport viewport{};
	VkRect2D scissor{};
	VkExtent2D renderExtent;
	VkViewport renderViewport{};

	CHAR_T strBuffer[256];

	// These are the post processing stages defined in engine.lua
	std::vector<RenderStage> renderStages = {};
	RenderStage mainRenderStage;
	RenderStage mainRenderStageTransparency;
	VkFramebuffer mainFrameBuffer;

	VkFramebuffer depthPrepassFrameBuffer;
	Shader* depthPrepassStaticShader = NULL;
	VkRenderPass depthPrepassRenderPass = NULL;

	VkDeviceSize offsets[1] = { 0 };

	std::vector<UIInstance> UI2D = {};
	std::vector<UIInstance> UI3D = {};
	std::vector<CombinedBufferAndDescriptorSet> UI3DDescriptorSets;
	std::vector<CombinedBufferAndDescriptorSet> UI2DDescriptorSets;
	Shader* UI3DPipeline;
	Shader* UI2DPipeline;

	VkRenderPass guiRenderPass = NULL;

	Shader* debugBBoxShader = NULL;

	float cullThreshold;

	std::vector<VulkanMemory*> uniformBuffers;
	std::vector<VulkanMemory*> psBuffers;

	std::vector<VkCommandBuffer> commandBuffers_DepthPrepass;
	std::vector<VkCommandBuffer> commandBuffers;
	std::vector<VkCommandBuffer> commandBuffers_Post;
	std::vector<VkCommandBuffer> commandBuffers_GUI;
	std::vector<std::vector<VkCommandBuffer>> commandBufferRefs;

	VkSwapchainKHR swapChain;
	std::vector<VkFramebuffer> swapChainFramebuffers;
	std::vector<VkImage> swapChainImages;
	std::vector<VkImageView> swapChainImageViews;

	VkImageView depthStencilImageView;

	VkCommandPool commandPool;

	std::vector<VkSemaphore> imageAvailableSemaphores;
	std::vector<VkSemaphore> renderFinishedSemaphores;
	std::vector<VkFence> inFlightFences;

	VkQueryPool queryPool;
	float timestampPeriod;
	long long gpuTime;
	std::vector<std::array<uint64_t, 2>> queryResults;

	ComputeShader* RTShader;

	// To allow for instancing, every object's shadow map is combined into a large atlas
	// It's done on startup so that lighting can be re-baked for a particular object without re-doing the whole shadow map
	// Though eventually I will implement a way to load in a pre-made beeg shadow map for a shipping build
	Texture* beegShadowMap;

	std::vector<std::array<VkDescriptorSet, 2>> uniformBufferDescriptorSets;

	void (*drawGUI)(VkCommandBuffer);

	std::vector<Font3D> fonts;
	std::vector<Font3DInstance*> text3DInstances;

	Mesh* debugBoundingBox;

	std::vector<PackedLevelThingShadowMapStruct> nonLevelPackedThings;

public:
	VkDevice logicalDevice;
	VkPhysicalDevice physicalDevice;
	VkInstance instance;
	VkQueue graphicsQueue;

	VkCommandBufferBeginInfo beginInfo{};

	std::array<VkClearValue, 5> clearValues{};

	Timing stats;

	uint32_t currentFrame;

	VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;

	VkFormat renderFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
	VkFormat normalFormat = VK_FORMAT_R8G8B8A8_SNORM;
	VkFormat positionFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
	VkFormat GIFormat = VK_FORMAT_R8G8B8A8_SRGB;

	VkFormat postProcessingFormat = VK_FORMAT_R8G8B8A8_SRGB;

	VkFormat swapChainImageFormat;

	Texture* mainRenderTarget_C; // Colour buffer
	Texture* mainRenderTarget_D; // Depth buffer
	Texture* mainRenderTarget_N; // Normal buffer
	Texture* mainRenderTarget_G; // Baked GI buffer
	Texture* mainRenderTarget_P; // Position buffer

	VkRenderPass mainRenderPass = NULL;
	VkRenderPass postProcRenderPass = NULL;

	// Max number of frames that can be in-progress at a time, which is determined when creating a swap-chain
	// If there's more than 1, that means the CPU can start working on the next frame while the current one is being drawn by the GPU
	uint32_t MAX_FRAMES_IN_FLIGHT;

	float sunDownAngle = -1.0f;
	float sunSwingSpeed = 0.002f;
	float sunAngle = 0.0f;

	float4x4 perspectiveMatrix;

	Shader* allShaders[100];
	uint32_t numShaders;
	// The number of engine shaders / the starting index of the material shaders
	uint32_t preExistingShaders;

	bool setup;

	Texture* cubemap;
	Texture* skyCubeMap;

	VkExtent2D swapChainExtent;

	long long presentTime;
	long long recordTime;
	long long setupRenderTime;
	long long acquireTime;

	SunLight* theSun;

	SpotLight* allSpotLights[MAX_SPOT_LIGHTS];
	uint32_t numSpotLights;

	Thing* allThings[MAX_THINGS];
	THING_INDEX allThingsLen;

	Material allMaterials[MAX_MATERIALS];
	MATERIAL_INDEX numMaterials;

	// To make searching by ID faster, things are grouped by IDs
	std::vector<std::vector<Thing*>> idThings;

	// Things that have collision also have a list, grouped by ID
	std::vector<std::vector<Thing*>> collisionThings;

	std::vector<ComputeShader*> allComputeShaders;
	Texture* RTTexture;

	zstring<char>* levelFilename;
	bool levelIsPacked;

private:
	VkApplicationInfo MakeAppInfo(const char* appName, uint32_t appVersion);
	void PickGPU();
	uint32_t RankDevice(VkPhysicalDevice device);

	SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
	VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
	void createLightRenderPass();
	void createDescriptorSetLayout(size_t vBuffers, size_t pBuffers, size_t numSamplers, size_t numStorageBuffers, size_t numStorageImages, VkDescriptorSetLayout* outLayout);

	void createCommandPool();
	void CreatePostProcessingRenderPass();
	void createFrameBuffers();
	void createUniformBuffers();
	void createCommandBuffers();
	void createSyncObjects();
	bool checkValidationLayerSupport();
	void CreateMainFrameBuffer(float resolutionScale);
	void DestroyMainFrameBuffer();
	void UpdateComputeBuffer();

	void recordGUICommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

	void AddThingToShaderGroup(RenderStageShaderGroup* pipelineGroup, Thing* thing, Mexel* mexel, Material* material);
	bool AddThingToExistingRenderStage(RenderStage* renderStage, Thing* thing, Mexel* mexel, Material* material);
	void AddMexelToMainRenderStage(Thing* thing, Mexel* mexel, Material* material);

	void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
	void DrawRenderStage(VkCommandBuffer commandBuffer, VkCommandBuffer prepassCommandBuffer, RenderStage* renderProcess, VkDescriptorSet* uniformBufferDescriptorSet);
	void DrawBoundingBoxes(VkCommandBuffer commandBuffer, RenderStage* renderStage, VkDescriptorSet* uniformBufferDescriptorSet);
	void RecordMainCommandBuffer(uint32_t imageIndex);

	void SortThings();
	void SetupPipelineGroup(RenderStageShaderGroup* pipelineGroup);
	void AllocateMeshGroupBuffers(RenderStageMeshGroup* meshGroup);
	RenderStageMeshGroup* NewMeshGroup(Thing* thing, Mexel* mexel);
	RenderStageMaterialGroup* NewMaterialGroup(Material* material, Thing* object, Mexel* mexel);

	//void InitRayTracing();

	void ResizeDebugPoints(std::vector<CombinedBufferAndDescriptorSet>& descriptorSetList, std::vector<UIInstance>& instanceList);

	void SortAndMakeBeegShadowMap();
	void AddThingToExistingBeegShadowMap(Thing* thing);

	void Render(Camera* activeCamera);

	inline void updateUniformBuffer(Camera* activeCamera, uint32_t imageIndex);

	void RunComputeShader();

	void DestroyPostProcessRenderStages();

	void CreateBeegShadowMap();

public:
	VulkanBackend(GLFWwindow* glWindow, void (*drawGUIFunc)(VkCommandBuffer), float resolutionScale);
	~VulkanBackend();

	// Some things like creating meshes require the LastGenEngine class to have the Backend defined, so they have to wait until after the constructor
	void AfterConstruction(float resolutionScale);

	Thing* AddThing(float3 position, float3 rotation, float3 scale, Mesh* mesh, std::vector<Material*>& materials, Texture*& shadowMap, bool isStatic, bool castsShadows, CollisionType collision, BYTE id, float shadowMapOffsetX, float shadowMapOffsetY, float shadowMapScale);
	void RemoveThing(Thing* thing);

	void AddThingToRenderStage(RenderStage* renderStage, Thing* thing);

	void ReadRenderStages(lua_State* L);
	void DestroyRenderStages();

	void SetupThings();
	void SetupMeshGroup(RenderStageMeshGroup* meshGroup);

	void updateUniformBufferDescriptorSets();
	void UpdateMeshGroupBufferDescriptorSet(RenderStageMeshGroup* meshGroup);

	//Texture* CreateTextureArray(Texture* textures, uint32_t numTextures, uint32_t width, uint32_t height, VkFormat format);

	void createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels, VkImageViewType viewType, int flags, VkImageView* outImageView);

	void CreateCubemap(const CHAR_T* filename, Texture*& outTexture);

	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
	uint32_t GetGraphicsFamily();

	VkShaderModule createShaderModule(const std::vector<char>& code);
	void CreateShadowPassShader();

	VkResult CreateFrameBuffer(VkImageView* attachments, uint32_t attachmentCount, VkRenderPass* renderPass, VkExtent2D size, uint32_t layers, VkFramebuffer* out_frameBuffer);

	void createSwapChain();
	void cleanupSwapChain();

	//void SaveTextureToPNG(Texture* texture, VkImageLayout currentLayout, const char* filename);
	void SaveBeegShadowMapToPNG(const char* filename);

	VkDescriptorSetLayout* GetComputeDescriptorSet(size_t numUniformBuffers, size_t numStorageBuffers, size_t numStorageTextures, size_t numSamplers);

	VkFormat findDepthFormat();
	VkFormat findDepthStencilFormat();

	VkFormat GetStorageImageFormat(VkImageType type, VkImageTiling tiling);

	std::vector<std::array<uint32_t, 4>> GetInfoFromZLSL(const char* zlsl, uint32_t* outAttachments, bool vertexShaderOnly=false);

	void UnloadLevel();

	void recreateSwapChain();
	void RecreateSwapChainStuff(float resolutionScale);
	void createImageViews();

	void RecordPostProcessCommandBuffers();

	// Returns the index into UI2D for updating the texture or location
	size_t Add2DUIElement(float2& pos, Texture* texture, bool isStatic);

	// By 3D element it means a point in 3D space which will be converted to a screen point
	// Returns the index into UI3D for updating the texture or location
	size_t Add3DUIElement(float3& pos, Texture* texture, bool isStatic);

	void AddMexelToRenderStage(RenderStage* renderStage, Thing* thing, Mexel* mexel, Material* material);
	void AddToMainRenderStage(Thing* thing);

	SpotLight* AddSpotLight(float3& position, float3& dir, float3& colour, float fov, float attenuation);

	void RefreshCommandBufferRefs();

	bool PerFrame();
	void OnLevelLoad();

	Material* AllocateMaterial();

	void RenderTo(Camera* camera, VkFramebuffer frameBuffer, VkRect2D renderArea, uint32_t clearValueCount, const VkClearValue* pClearValues);

	void UpdateCamera();

	void LoadFont3D(const CHAR_T* fontName);
	Font3DInstance* Add3DText(const CHAR_T* fontName, const CHAR_T* text, float3 position, float3 rotation, float3 scale, bool isStatic);

	void GetWindowSize(uint32_t& out_width, uint32_t& out_height) const;

	VkCommandBuffer beginSingleTimeCommands();
	void endSingleTimeCommands(VkCommandBuffer commandBuffer);

	void RecreateMainFrameBuffer(float resolutionScale);

	void AddThingToNonLevelPackedThings(Thing* thing, THING_INDEX index);
	void SaveNonLevelPackedThings();
	void LoadNonLevelPackedThings();
};
