#pragma once
#include "engineTypes.h"

#include <array>
#include <vector>

#define VK_FLAGS_NONE 0
constexpr size_t MAX_SPOT_LIGHTS = 50;

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

struct ShadowCaster
{
	uint32_t objectIndex;
	uint32_t mexelIndex;
	Material material;
};

inline VkCommandPoolCreateInfo MakeCommandPoolCreateInfo(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags);

struct Vertex {
	float3 pos;
	float3 nrm;
	float3 tangent;
	float4 uv;

	static VkVertexInputBindingDescription getBindingDescription()
	{
		VkVertexInputBindingDescription bindingDescription{};
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(Vertex);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		return bindingDescription;
	}

	// In order to change the attributes for each vertex, update the array length and fill in the added entry.
	static std::array<VkVertexInputAttributeDescription, 4> getAttributeDescriptions() {
		std::array<VkVertexInputAttributeDescription, 4> attributeDescriptions{};

		// The format parameter describes the type of data for the attribute.
		// A bit confusingly, the formats are specified using the same enumeration as color formats.
		// The following shader types and formats are commonly used together:

		// float: VK_FORMAT_R32_SFLOAT
		// vec2 : VK_FORMAT_R32G32_SFLOAT
		// vec3 : VK_FORMAT_R32G32B32_SFLOAT
		// vec4 : VK_FORMAT_R32G32B32A32_SFLOAT

		// As you can see, you should use the format where the amount of color channels matches the number of components in the shader data type.
		// It is allowed to use more channels than the number of components in the shader, but they will be silently discarded.
		// If the number of channels is lower than the number of components, then the BGA components will use default values of (0, 0, 1).
		// The color type(SFLOAT, UINT, SINT) and bit width should also match the type of the shader input.
		// See the following examples :

		// ivec2: VK_FORMAT_R32G32_SINT, a 2 - component vector of 32 - bit signed integers
		// uvec4 : VK_FORMAT_R32G32B32A32_UINT, a 4 - component vector of 32 - bit unsigned integers
		// double : VK_FORMAT_R64_SFLOAT, a double - precision(64 - bit) float

		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[0].offset = offsetof(Vertex, pos);

		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[1].offset = offsetof(Vertex, nrm);

		attributeDescriptions[2].binding = 0;
		attributeDescriptions[2].location = 2;
		attributeDescriptions[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		attributeDescriptions[2].offset = offsetof(Vertex, uv);

		attributeDescriptions[3].binding = 0;
		attributeDescriptions[3].location = 3;
		attributeDescriptions[3].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[3].offset = offsetof(Vertex, tangent);

		return attributeDescriptions;
	}
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

struct SpotLightThread
{
	Thread* thread;
	volatile bool go;
	volatile bool done;
	SpotLight* light;
	VulkanBackend* backend;
	VkRenderPassBeginInfo passInfo;
};

const char* String_VkResult(VkResult vr);

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
	
	char strBuffer[256];

	std::vector<ShadowCaster> shadowCastersOpaque;
	std::vector<ShadowCaster> shadowCastersMasked;

	std::vector<FullRenderPass> renderingProcess = {};
	FullRenderPass mainRenderProcess;
	FullRenderPass mainRenderProcessTransparency;
	VkFramebuffer mainFrameBuffer;

	VkFramebuffer depthPrepassFrameBuffer;
	Shader* depthPrepassStaticPipeline = NULL;
	VkRenderPass depthPrepassRenderPass = NULL;

	Thread* sunThreads[NUMCASCADES];
	SunPassThreadInfo sunThreadInfos[NUMCASCADES];

	VkDeviceSize offsets[1] = { 0 };

	std::vector<UIInstance> UI2D = {};
	std::vector<UIInstance> UI3D = {};
	std::vector<CombinedBufferAndDescriptorSet> UI3DDescriptorSets;
	std::vector<CombinedBufferAndDescriptorSet> UI2DDescriptorSets;
	Shader* UI3DPipeline;
	Shader* UI2DPipeline;

	VkRenderPass guiRenderPass = NULL;

	Shader* sunShadowPassPipeline = NULL;
	Shader* spotShadowPassPipeline = NULL;

	SpotLightThread spotLightThreads[MAX_SPOT_LIGHTS];
	uint32_t numSpotLightThreads;

	float cullThreshold;

	std::vector<VkBuffer> uniformBuffers;
	std::vector<VkDeviceMemory> uniformBuffersMemory;
	std::vector<VkBuffer> psBuffers;
	std::vector<VkDeviceMemory> psBuffersMemory;

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

	std::vector<Vertex> allVertices;
	VkDeviceMemory allVertexBufferMem;

	std::vector<uint32_t> allIndices;
	VkDeviceMemory allIndexBufferMem;

	FullSetLayout allSetLayouts[100];
	uint32_t numSetLayouts;

	VkCommandPool commandPool;

	std::vector<VkSemaphore> imageAvailableSemaphores;
	std::vector<VkSemaphore> renderFinishedSemaphores;
	std::vector<VkFence> inFlightFences;

	VkQueryPool queryPool;
	float timestampPeriod;
	float gpuTime;
	std::vector<std::array<uint64_t, 2>> queryResults;

	std::array<VkClearValue, 5> clearValues{};

	std::vector<FullSampler*> allSamplers;

	ComputeShader* RTShader;

	// This shader takes each vertex of the bounding box and determines if any of them are in front of the camera
	ComputeShader* cullShader;
	// This shader takes all 6 of those bools and writes the draw commands for the indirect draw
	ComputeShader* cullConsolidationShader;
	VulkanMemory* cullBoolsPerVertex;
	VulkanMemory* cullDrawCommands;
	VulkanMemory* cullBoundingBoxBuffer;

	Texture beegShadowMap;

	std::vector<std::array<VkDescriptorSet, 2>> uniformBufferDescriptorSets;

public:
	VkDevice logicalDevice;
	VkPhysicalDevice physicalDevice;
	VkInstance instance;
	VkQueue graphicsQueue;

	VkCommandBufferBeginInfo beginInfo{};

	Timing stats;

	uint32_t currentFrame;

	VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;

	VkFormat renderFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
	VkFormat normalFormat = VK_FORMAT_R8G8B8A8_SNORM;
	VkFormat positionFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
	VkFormat GIFormat = VK_FORMAT_R8G8B8A8_SRGB;

	VkFormat postProcessingFormat = VK_FORMAT_R8G8B8A8_SRGB;

	VkFormat swapChainImageFormat;

	Texture mainRenderTarget_C; // Colour buffer
	Texture mainRenderTarget_D; // Depth buffer
	Texture mainRenderTarget_N; // Normal buffer
	Texture mainRenderTarget_G; // Baked GI buffer
	Texture mainRenderTarget_P; // Position buffer

	VkRenderPass mainRenderPass = NULL;
	VkRenderPass postProcRenderPass = NULL;
	VkRenderPass sunShadowPassRenderPass = NULL;
	VkRenderPass spotShadowPassRenderPass = NULL;
	VkRenderPass lightRenderPass = NULL;

	VkDescriptorPool descriptorPool = NULL;

	Shader* lightPipelineOpaqueStatic;
	Shader* lightPipelineMaskedStatic;

	// Max number of frames that can be in-progress at a time, which is determined when creating a swap-chain
	// If there's more than 1, that means the CPU can start working on the next frame while the current one is being drawn by the GPU
	size_t MAX_FRAMES_IN_FLIGHT;

	float sunDownAngle = -1.0f;
	float sunSwingSpeed = 0.002f;
	float sunAngle = 0.0f;

	glm::mat4 perspectiveMatrix;

	Shader allPipelines[100];
	uint32_t numPipelines;

	// The sun's shadow maps are recorded by dispatching a thread for each cascade
	// I don't know why, but each thread needs its own command pool
	VkCommandPool sunThreadCommandPools[NUMCASCADES];

	Texture cubemap;
	Texture skyCubeMap;

	VkExtent2D swapChainExtent;

	long long presentTime;
	long long recordTime;
	long long setupRenderTime;
	long long acquireTime;

	SunLight* theSun;

	void (*drawGUI)(VkCommandBuffer);

	SpotLight* allSpotLights[MAX_SPOT_LIGHTS];
	uint32_t numSpotLights;

	uint16_t allObjectsLen;
	MeshObject* allObjects[65536];

	std::vector<Material_T> allMaterials;
	std::vector<Texture*> allTextures;
	std::vector<Mexel*> allMexels;
	std::vector<Mesh*> allMeshes;
	std::vector<ComputeShader**> allComputeShaders;
	Texture RTTexture;

private:
	VkApplicationInfo MakeAppInfo(const char* appName, uint32_t appVersion);
	void PickGPU();
	uint32_t RankDevice(VkPhysicalDevice device);

	SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
	VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
	void createLightRenderPass();
	void createDescriptorSetLayout(size_t vBuffers, size_t pBuffers, size_t numSamplers, size_t numStorageBuffers, VkDescriptorSetLayout* outLayout);

	void createCommandPool();
	void CreatePostProcessingRenderPass();
	void createFrameBuffers();
	void createUniformBuffers();
	void createDescriptorPool();
	void createCommandBuffers();
	void createSyncObjects();
	bool checkValidationLayerSupport();
	void CreateMainFrameBuffer();
	void UpdateComputeBuffer();

	void RecordSpotLightPass(VkCommandBuffer commandBuffer, VkFramebuffer frameBuffer, VkPipelineLayout layout, VkDescriptorSet descriptorSet, bool opaque);

	void recordGUICommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

	void AllocateDescriptorSet(VkDescriptorSetLayout* pSetLayouts, uint32_t descriptorSetCount, VkDescriptorPool descriptorPool, VkDescriptorSet* out_DescriptorSets);

	VkCommandBuffer beginSingleTimeCommands();
	void endSingleTimeCommands(VkCommandBuffer commandBuffer);
	void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

	void AddObjectToRenderProcess_Pipeline(FullRenderPass* pass, MeshObject* mo);
	void AddObjectToPipelineGroup(RenderPassPipelineGroup* pipelineGroup, MeshObject* mo, Mexel* mexel, Material material);
	bool AddObjectToExistingRenderProcess(FullRenderPass* renderStage, MeshObject* mo, Mexel* mexel, Material material);

	void GatherShadowCasters();

	void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
	void createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels, VkImageViewType viewType, int flags, VkImageView* outImageView);
	bool ObjectsSorted();

	void SetupPipelineGroup(RenderPassPipelineGroup* pipelineGroup);

	//void InitRayTracing();

	void ResizeDebugPoints(std::vector<CombinedBufferAndDescriptorSet>& descriptorSetList, std::vector<UIInstance>& instanceList);

	void SortAndMakeBeegShadowMap();

	void DrawMexels(VkCommandBuffer commandBuffer, Mesh* mesh);
	bool BoundingBoxOnScreen(RenderPassMeshInstance* instance, float3& camPos, float3& camDir);

	void SortObjects();

	void Render(Camera* activeCamera);

	inline void updateUniformBuffer(Camera* activeCamera, uint32_t imageIndex);

	void RunComputeShader();
	void SetupCullShader();

	void RecordMainCommandBuffer(uint32_t imageIndex);

	void DrawRenderProcess(VkCommandBuffer commandBuffer, VkCommandBuffer prepassCommandBuffer, FullRenderPass* renderProcess, VkDescriptorSet* uniformBufferDescriptorSet);


public:
	VulkanBackend(GLFWwindow* glWindow, void (*drawGUIFunc)(VkCommandBuffer));
	~VulkanBackend();

	void FullCreateImage(VkImageType imageType, VkImageViewType imageViewType, VkFormat imageFormat, int width, int height, int mipLevels, int arrayLayers, VkSampleCountFlagBits sampleCount, VkImageTiling imageTiling, VkImageUsageFlags usage, VkImageAspectFlags imageAspectFlags, VkFilter magFilter, VkFilter minFilter, VkSamplerAddressMode samplerAddressMode, VkImage& outImage, VkDeviceMemory& outMemory, VkImageView& outView, VkSampler& outSampler, bool addSamplerToList);

	void AddObjectToRenderingProcess(FullRenderPass* renderStage, MeshObject* mo);

	void ReadRenderProcess(lua_State* L);
	void DestroyRenderProcesses();

	void SetupObjects();

	void updateUniformBufferDescriptorSets();

	void createImage(VkPhysicalDevice device, VkImageType imageType, uint32_t width, uint32_t height, uint32_t mipLevels, uint32_t arrayLayers, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
	void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, VkImageAspectFlags aspectMask, uint32_t mipLevels, uint32_t layerCount);

	void generateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels, uint32_t layerCount);
	void BlitImage(VkCommandBuffer commandBuffer, VkImage from, Rect fromArea, VkImage to, Rect toArea, VkImageLayout srcLayout, VkFilter filter, VkImageAspectFlags srcAspect, VkImageAspectFlags dstAspect, VkImageLayout srcFinalLayout, VkImageLayout dstFinalLayout, uint32_t srcMipLevel=0, uint32_t dstMipLevel=0, uint32_t srcLayer=0, uint32_t dstLayer=0, VkImageLayout dstInitialLayout=VK_IMAGE_LAYOUT_UNDEFINED);
	void OneTimeBlit(VkImage from, Rect fromArea, VkImage to, Rect toArea, VkImageLayout srcLayout, VkFilter filter, VkImageAspectFlags srcAspect, VkImageAspectFlags dstAspect, VkImageLayout srcFinalLayout, VkImageLayout dstFinalLayout);
	Texture* CreateTextureArray(Texture* textures, uint32_t numTextures, uint32_t width, uint32_t height, VkFormat format);

	// Helper function to copy arbitrary data to a texture, which first involves copying to a staging buffer, and then to the texture.
	void CopyBufferToImage(void* src, VkDeviceSize imageSize, Texture* dst);
	void RecordBufferForCopyingToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, uint32_t layerCount);

	// It's more like CreateMesh, it does the vertex and index buffer
	Mexel createVertexBuffer(void* vertices, size_t numVerts, void* indices, size_t numIndices, size_t indexSize);

	// Takes a mexel and applies the matrix to each vertex, creating a new mexel
	Mexel* MakeStaticMexel(float4x4& matrix, Mexel* sourceMexel);
	Mesh* MakeStaticMesh(float4x4& matrix, Mesh* mesh);

	void createTextureImage(const char* filename, bool isNormal, bool freeFilename, Texture* outTex);
	void CreateCubemap(const char* filename, Texture* outTexture);

	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
	uint32_t GetGraphicsFamily();

	VkShaderModule createShaderModule(const std::vector<char>& code);
	void createGraphicsPipeline(const char* vertfilename, const char* pixlfilename, VkRenderPass renderPass, VkDescriptorSetLayout* setLayouts, uint32_t numSetLayouts, int shader_type, VkExtent2D screen_size, VkCullModeFlags cullMode, VkPolygonMode polygonMode, VkSampleCountFlagBits sampleCount, BlendMode blendMode, bool depthTest, bool depthWrite, VkPushConstantRange* pushConstantRanges, uint32_t numPushConstantRanges, uint32_t numAttachments, uint32_t stencilWriteMask, VkCompareOp stencilCompareOp, uint32_t stencilTestValue, float depthBias, VkPipelineLayout* outPipelineLayout, VkPipeline* outPipeline);
	Shader* NewPipeline_Separate(const char* zlsl, const char* pixelShader, bool freePixelShader, const char* vertexShader, bool freeVertexShader, VkRenderPass renderPass, int shaderType, VkExtent2D screenSize, VkCullModeFlags cullMode, VkPolygonMode polygonMode, VkSampleCountFlagBits sampleCount, BlendMode blendMode, uint32_t stencilWriteMask, VkCompareOp stencilCompareOp, uint32_t stencilTestValue, float depthBias, bool depthTest, bool depthWrite, bool masked);
	void CreateShadowPassPipeline();

	VkResult CreateFrameBuffer(VkImageView* attachments, uint32_t attachmentCount, VkRenderPass* renderPass, VkExtent2D size, uint32_t layers, VkFramebuffer* out_frameBuffer);

	void DestroyTexture(Texture* ptr);

	void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);

	void createSwapChain();
	void cleanupSwapChain();

	Camera* GetActiveCamera();
	void SetActiveCamera(Camera* camera);

	void SaveTextureToPNG(Texture* texture, const char* filename);

	VkDescriptorSetLayout* GetDescriptorSetLayout(size_t numVBuffers, size_t numPBuffers, size_t numTextures, size_t numStorageBuffers=0);
	VkDescriptorSetLayout* GetComputeDescriptorSet(size_t numUniformBuffers, size_t numStorageBuffers, size_t numStorageTextures, size_t numSamplers);

	VkFormat findDepthFormat();
	VkFormat findDepthStencilFormat();

	void AllocateDescriptorSets(uint32_t numDescriptorSets, VkDescriptorSetLayout* pSetLayouts, VkDescriptorSet* out_sets);
	VkFormat GetStorageImageFormat(VkImageType type, VkImageTiling tiling);

	void UnloadLevel();

	uint32_t findMemoryType(VkPhysicalDevice device, uint32_t typeFilter, VkMemoryPropertyFlags properties);
	void recreateSwapChain();
	void createImageViews();

	void createTextureSampler(int mipLevels, VkFilter magFilter, VkFilter minFilter, VkSamplerAddressMode addressMode, VkSampler& outSampler);
	void GetTextureSampler(int mipLevels, VkFilter magFilter, VkFilter minFilter, VkSamplerAddressMode addressMode, VkSampler& outSampler, bool addToList);

	void CreateAllVertexBuffer();
	void CreateAllIndexBuffer();
	size_t AddVertexBuffer(Vertex* vertices, size_t numVerts);
	size_t AddIndexBuffer16(uint16_t* indices, size_t numIndices);
	size_t AddIndexBuffer32(uint32_t* indices, size_t numIndices);

	void RecordPostProcessCommandBuffers();

	// Returns the index into UI2D for updating the texture or location
	size_t Add2DUIElement(float2& pos, Texture* texture, bool isStatic);

	// By 3D element it means a point in 3D space which will be converted to a screen point
	// Returns the index into UI3D for updating the texture or location
	size_t Add3DUIElement(float3& pos, Texture* texture, bool isStatic);

	void AddMexelToRenderProcess(FullRenderPass* renderStage, MeshObject* mo, Mexel* mexel, Material material);
	void AddToMainRenderProcess(MeshObject* mo);

	void AddSpotLight(float3& position, float3& dir, float fov, float attenuation);

	// Buffers that can't be updated by the CPU once they've been created are much faster for the GPU to work with, so they should be used whenever possible
	void CreateStaticBuffer(void* data, size_t dataSize, VkBufferUsageFlags usage, VkBuffer& buffer, VkDeviceMemory& memory);

	void RefreshCommandBufferRefs();

	void PerFrame();
	void OnLevelLoad();
};