#pragma once

#include "engineTypes.h"
#include <vector>
#include "engineSettings.h"
#include "zstring.h"

constexpr size_t LEVEL_HAS_SUN_FLAG = 0x80;
constexpr size_t LEVEL_IS_PACKED_FLAG = 0x40;

// These are flags, so you can OR level and clean together to say you want to pack the level, and clean up the textures folder
enum PackMode
{
	PACK_NONE,
	PACK_LEVEL = 1,
	PACK_CLEAN = 2
};

enum BlendMode
{
	BM_OPAQUE,
	BM_TRANSPARENT,
	BM_ADDITIVE,
	BM_MAX
};

struct Stats
{
	float frametime;
	long long aquireTime;
	long long presentTime;
	long long recordTime;
	long long setupRenderTime;
	long long acquireTime;
	int triangle_count;
	int drawcall_count;
	int bound_pipelines;
	int bound_buffers;
	int api_calls;
	int blits;
	int passes;
};

struct ThreadSyncer
{
	volatile bool& awaitingSync;
	volatile bool& synced;

	// Blocks until the threads are synced up
	void Sync()
	{
		awaitingSync = true;
		while (!synced);
	}

	void Reset()
	{
		awaitingSync = false;
		synced = false;
	}
};

class Thing;
class Texture;
class Shader;
class SpotLight;

struct Material;
struct lua_State;

enum CullMode : int;
enum PolygonMode : int;
enum ImageLayout : int;

// Some lua functions that need to be defined, and are API-dependent
int LuaFN_CreateRenderPass(lua_State* L);
int LuaFN_RenderPassGC(lua_State* L);
int LuaFN_CreateImage(lua_State* L);
int LuaFN_NewComputeShader(lua_State* L);
static int LuaFN_RunComputeShader(lua_State* L);
int LuaFN_SpotLightNewIndex(lua_State* L);
int LuaFN_CreateFrameBuffer(lua_State* L);

bool RecompileShaderThreadProc(ThreadSyncer* sync);
void SetActiveCamera(Camera* camera);
Camera*& GetActiveCamera();

struct TextureInfo
{
	zstring<CHAR_T>* filename;
	bool isNonColour;
};

// The Backend class is a generic wrapper for the graphics API, opening the doors to other APIs like Direct X and OpenGL
// I'm documenting as much as possible so hopefully adding an API yourself is as easy as possible
// The other classes like Mesh, Texture, Shader, are API-dependent, the engine does not know anything about them so the particular backend can decide what's in them
class Backend
{
public:
	Stats stats;

public:
	// Adds all the enums, framebuffer textures, cubemaps, and misc. stuff to lua
	virtual void AddLuaStuff(lua_State* L) {}

	// Draws the profiler window with all the stats, and some extra controls for things like adding lights
	virtual void GUIStuff() {};

	// Gets the size of the draw window
	virtual void GetWindowSize(uint32_t& out_width, uint32_t& out_height) {}

	// Gets called per frame, where rendering happens
	// Returns whether or not the frame buffer needs to be re-created, so the engine can re-do the engine.lua and stuff
	virtual bool PerFrame() { return false; }

	// Called after the camera is moved, so the GPU memory for it can be updated
	virtual void UpdateCamera() {}

	// This is called when the window is resized and the swapchain, frame buffer, etc needs to be remade
	virtual void RecreateSwapChainStuff(float resolutionScale) {}

	// This function reads in the render stages and converts them to a format that can be sent to the GPU
	// The actual implementation can vary depending on what the API needs, but it'd be nice if the same engine.lua could work in multiple APIs
	virtual void ReadRenderStages(lua_State* L) {}

	// This is called right after the constructor, this is for creating objects that need access to 'GetEngine()->backend' in their constructor
	// You could even treat this as an Init function if you want
	// Though I personally think it's much cleaner to do as much as possible in the constructor
	virtual void AfterConstruction(float resolutionScale) {}

	// Records the post-processing for the GPU. If the API has no equivalent to command buffers that can be recorded once and then re-sent,
	// Make this function do nothing and instead put that in PerFrame
	virtual void RecordPostProcessCommandBuffers() {}


	virtual void updateUniformBufferDescriptorSets() {}

	// This function blocks until the GPU is no longer doing work
	virtual void WaitUntilIdle() {}

	// This function is called after GameBegin, but before LevelBegin, so you can take the loaded level and do what needs to be done to make drawing more efficient
	// It's also where the level packing is done, if you want to support that
	virtual void PreRun(PackMode packMode) {}

	// Deletes all level stuff
	virtual void UnloadLevel() {}

	// Removes a Thing, so it won't be drawn anymore. Do not delete the Thing, as this function is called in the destructor
	virtual void RemoveThing(Thing* thing) {}

	// Adds a Thing so it can be drawn
	virtual Thing* AddThing(float3 position, float3 rotation, float3 scale, Mesh* mesh, std::vector<Material*>& materials, Texture*& shadowMap, bool isStatic, bool castsShadows, CollisionType collision, BYTE id, float shadowMapOffsetX, float shadowMapOffsetY, float shadowMapScale) { return NULL;  }

	// Adds a shader given the info
	virtual Shader* AddMaterialShader(const char* zlsl, const char* vertfilename, const char* pixlfilename, int shaderType, CullMode cullMode, PolygonMode polygonMode, BlendMode blendMode, bool depthTest, bool depthWrite, VkPushConstantRange* pushConstantRanges, uint32_t numPushConstantRanges, uint32_t stencilWriteMask, VkCompareOp stencilTestOp, uint32_t stencilTestValue, float depthBias, bool masked) { return NULL; }
	virtual Shader* AddMaterialShader(const wchar_t* zlsl, const wchar_t* vertfilename, const wchar_t* pixlfilename, int shaderType, CullMode cullMode, PolygonMode polygonMode, BlendMode blendMode, bool depthTest, bool depthWrite, VkPushConstantRange* pushConstantRanges, uint32_t numPushConstantRanges, uint32_t stencilWriteMask, VkCompareOp stencilTestOp, uint32_t stencilTestValue, float depthBias, bool masked) { return NULL; }

	// Loads the 2 cubemaps for the level, given their names. It includes the folder and name but not the extension, you'll have to add the '_up.png', '_down.png', etc.
	virtual void LoadCubemaps(const CHAR_T* objectCubemapFilename, const CHAR_T* skyboxFilename) {}

	// Returns the number of shaders/materials that have been made
	virtual uint32_t GetNumShaders() { return 0; }
	virtual uint32_t GetNumMaterials() { return 0; }

	// When loading the level, this tells the backend if the level is packed, so it can load in any additional stuff it created when packing the level
	virtual void SetLevelPacked(bool isPacked) {}

	// Adds a sun-light to the level, the engine expects there to only be one so this may be called once when the level is being loaded
	virtual void AddSunLight(float3& rotation) {}

	// Adds a spot-light to the level, the engine expects that there can be multiple, so it'll be called for each light in a level
	virtual SpotLight* AddSpotLight(float3& position, float3& dir, float3& colour, float fov, float attenuation) { return NULL; }

	// Adds a material given the shader's index, the filenames for the textures, and a roughness multiplier
	virtual Material* AddMaterial(uint32_t shaderIndex, std::vector<TextureInfo>& textureFilenames, float roughnessMultiplier) { return NULL; }
	virtual Material* AddMaterial(uint32_t shaderIndex, std::vector<Texture*>& textures, float roughnessMultiplier) { return NULL; }

	// Returns the index of a shader, so when calling AddMaterial with that index it'll pick that shader
	virtual uint32_t IndexOfShader(Shader* shader) { return 0; }

	// Returns a vector of all Things with collision enabled that have the particular id
	virtual std::vector<Thing*> GetCollisionThingsById(uint32_t id) { return {}; }
	virtual std::vector<Thing*> GetAllThings() { return {}; }
	virtual std::vector<Thing*> GetAllThingsById(uint32_t id) { return {}; }

	// Im Gui is set up differently based on the underlying API, so InitGUI and DeInitGUI have been moved to the backend
	virtual void InitGUI() {}
	virtual void DeInitGUI() {}

	virtual Material* GetMaterial(uint32_t index) { return NULL; }

	// This is called after loading a level, when a level has been loaded before, so it's not a cold-start
	virtual void OnWarmStart() {}

	// After the engine.lua is run, the backend can use this opportunity to read back settings and other stuff that was defined in there
	virtual void PostEngineLua(lua_State* L) {}
};
