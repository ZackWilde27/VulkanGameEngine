#pragma once

#include "../Backend.h"

#include <d3d9.h>

class D3D9Backend : public Backend
{
	LPDIRECT3D9 d3d;
	LPDIRECT3DDEVICE9 device;
	const UINT MAX_FRAMES_IN_FLIGHT = 2;

public:
	D3D9Backend(GLFWwindow* glWindow, float resolutionScale);
	~D3D9Backend();

	//void AddLuaStuff(lua_State* L) override;
	//void GUIStuff() override;
	//void GetWindowSize(uint32_t& out_width, uint32_t& out_height) override;
	bool PerFrame() override;
	//void UpdateCamera() override;
	//void RecreateSwapChainStuff(float resolutionScale) override;
	//void ReadRenderStages(lua_State* L) override;
	//void AfterConstruction(float resolutionScale) override;
	//void RecordPostProcessCommandBuffers() override;
	//void updateUniformBufferDescriptorSets() override;
	//void WaitUntilIdle() override;
	//void PreRun(PackMode packMode) override;
	//void UnloadLevel() override;
	//void RemoveThing(Thing* thing) override;
	//Thing* AddThing(float3 position, float3 rotation, float3 scale, Mesh* mesh, std::vector<Material*>& materials, Texture*& shadowMap, bool isStatic, bool castsShadows, CollisionType collision, BYTE id, float shadowMapOffsetX, float shadowMapOffsetY, float shadowMapScale) override;
	//Shader* AddMaterialShader(const char* zlsl, const char* vertfilename, const char* pixlfilename, int shaderType, CullMode cullMode, PolygonMode polygonMode, BlendMode blendMode, bool depthTest, bool depthWrite, VkPushConstantRange* pushConstantRanges, uint32_t numPushConstantRanges, uint32_t stencilWriteMask, VkCompareOp stencilTestOp, uint32_t stencilTestValue, float depthBias, bool masked) override;
	//Shader* AddMaterialShader(const wchar_t* zlsl, const wchar_t* vertfilename, const wchar_t* pixlfilename, int shaderType, CullMode cullMode, PolygonMode polygonMode, BlendMode blendMode, bool depthTest, bool depthWrite, VkPushConstantRange* pushConstantRanges, uint32_t numPushConstantRanges, uint32_t stencilWriteMask, VkCompareOp stencilTestOp, uint32_t stencilTestValue, float depthBias, bool masked) override;
	//void LoadCubemaps(const CHAR_T* objectCubemapFilename, const CHAR_T* skyboxFilename) override;
	//uint32_t GetNumShaders() override;
	//uint32_t GetNumMaterials() override;
	//void SetLevelPacked(bool isPacked) override;
	//void AddSunLight(float3& rotation) override;
	//SpotLight* AddSpotLight(float3& position, float3& dir, float3& colour, float fov, float attenuation) override;
	//Material* AddMaterial(uint32_t shaderIndex, std::vector<TextureInfo>& textureFilenames, float roughnessMultiplier) override;
	//Material* AddMaterial(uint32_t shaderIndex, std::vector<Texture*>& textures, float roughnessMultiplier) override;
	//uint32_t IndexOfShader(Shader* shader) override;
	//std::vector<Thing*> GetCollisionThingsById(uint32_t id) override;
	//std::vector<Thing*> GetAllThings() override;
	//std::vector<Thing*> GetAllThingsById(uint32_t id) override;
	//void InitGUI() override;
	//void DeInitGUI() override;
	//Material* GetMaterial(uint32_t index) override;
	//void OnWarmStart() override;
	//void PostEngineLua(lua_State* L) override;
};