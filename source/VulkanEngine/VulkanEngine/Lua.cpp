#include "Lua.h"
#include "luafunctions.h"
#include "engine.h"
#include "Thread.h"
#include "SunLight.h"
#include "SpotLight.h"

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

	AddLuaGlobalEnum(VK_SUBPASS_EXTERNAL);

	// I made a python script to auto generate all of these, otherwise this would have taken forever
	AddLuaGlobalEnum(VK_IMAGE_LAYOUT_UNDEFINED);
	AddLuaGlobalEnum(VK_IMAGE_LAYOUT_GENERAL);
	AddLuaGlobalEnum(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	AddLuaGlobalEnum(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
	AddLuaGlobalEnum(VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
	AddLuaGlobalEnum(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	AddLuaGlobalEnum(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	AddLuaGlobalEnum(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	AddLuaGlobalEnum(VK_IMAGE_LAYOUT_PREINITIALIZED);
	AddLuaGlobalEnum(VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL);
	AddLuaGlobalEnum(VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL);
	AddLuaGlobalEnum(VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
	AddLuaGlobalEnum(VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL);
	AddLuaGlobalEnum(VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL);
	AddLuaGlobalEnum(VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL);
	AddLuaGlobalEnum(VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL);
	AddLuaGlobalEnum(VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL);
	AddLuaGlobalEnum(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
	AddLuaGlobalEnum(VK_IMAGE_LAYOUT_VIDEO_DECODE_DST_KHR);
	AddLuaGlobalEnum(VK_IMAGE_LAYOUT_VIDEO_DECODE_SRC_KHR);
	AddLuaGlobalEnum(VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR);
	AddLuaGlobalEnum(VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR);
	AddLuaGlobalEnum(VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT);
	AddLuaGlobalEnum(VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR);
	AddLuaGlobalEnum(VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR);
	AddLuaGlobalEnum(VK_IMAGE_LAYOUT_VIDEO_ENCODE_DST_KHR);
	AddLuaGlobalEnum(VK_IMAGE_LAYOUT_VIDEO_ENCODE_SRC_KHR);
	AddLuaGlobalEnum(VK_IMAGE_LAYOUT_VIDEO_ENCODE_DPB_KHR);
	AddLuaGlobalEnum(VK_IMAGE_LAYOUT_ATTACHMENT_FEEDBACK_LOOP_OPTIMAL_EXT);
	AddLuaGlobalEnum(VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL_KHR);
	AddLuaGlobalEnum(VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL_KHR);
	AddLuaGlobalEnum(VK_IMAGE_LAYOUT_SHADING_RATE_OPTIMAL_NV);
	AddLuaGlobalEnum(VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL_KHR);
	AddLuaGlobalEnum(VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL_KHR);
	AddLuaGlobalEnum(VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL_KHR);
	AddLuaGlobalEnum(VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL_KHR);
	AddLuaGlobalEnum(VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL_KHR);
	AddLuaGlobalEnum(VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR);

	AddLuaGlobalEnum(VK_FORMAT_UNDEFINED);
	AddLuaGlobalEnum(VK_FORMAT_R4G4_UNORM_PACK8);
	AddLuaGlobalEnum(VK_FORMAT_R4G4B4A4_UNORM_PACK16);
	AddLuaGlobalEnum(VK_FORMAT_B4G4R4A4_UNORM_PACK16);
	AddLuaGlobalEnum(VK_FORMAT_R5G6B5_UNORM_PACK16);
	AddLuaGlobalEnum(VK_FORMAT_B5G6R5_UNORM_PACK16);
	AddLuaGlobalEnum(VK_FORMAT_R5G5B5A1_UNORM_PACK16);
	AddLuaGlobalEnum(VK_FORMAT_B5G5R5A1_UNORM_PACK16);
	AddLuaGlobalEnum(VK_FORMAT_A1R5G5B5_UNORM_PACK16);
	AddLuaGlobalEnum(VK_FORMAT_R8_UNORM);
	AddLuaGlobalEnum(VK_FORMAT_R8_SNORM);
	AddLuaGlobalEnum(VK_FORMAT_R8_USCALED);
	AddLuaGlobalEnum(VK_FORMAT_R8_SSCALED);
	AddLuaGlobalEnum(VK_FORMAT_R8_UINT);
	AddLuaGlobalEnum(VK_FORMAT_R8_SINT);
	AddLuaGlobalEnum(VK_FORMAT_R8_SRGB);
	AddLuaGlobalEnum(VK_FORMAT_R8G8_UNORM);
	AddLuaGlobalEnum(VK_FORMAT_R8G8_SNORM);
	AddLuaGlobalEnum(VK_FORMAT_R8G8_USCALED);
	AddLuaGlobalEnum(VK_FORMAT_R8G8_SSCALED);
	AddLuaGlobalEnum(VK_FORMAT_R8G8_UINT);
	AddLuaGlobalEnum(VK_FORMAT_R8G8_SINT);
	AddLuaGlobalEnum(VK_FORMAT_R8G8_SRGB);
	AddLuaGlobalEnum(VK_FORMAT_R8G8B8_UNORM);
	AddLuaGlobalEnum(VK_FORMAT_R8G8B8_SNORM);
	AddLuaGlobalEnum(VK_FORMAT_R8G8B8_USCALED);
	AddLuaGlobalEnum(VK_FORMAT_R8G8B8_SSCALED);
	AddLuaGlobalEnum(VK_FORMAT_R8G8B8_UINT);
	AddLuaGlobalEnum(VK_FORMAT_R8G8B8_SINT);
	AddLuaGlobalEnum(VK_FORMAT_R8G8B8_SRGB);
	AddLuaGlobalEnum(VK_FORMAT_B8G8R8_UNORM);
	AddLuaGlobalEnum(VK_FORMAT_B8G8R8_SNORM);
	AddLuaGlobalEnum(VK_FORMAT_B8G8R8_USCALED);
	AddLuaGlobalEnum(VK_FORMAT_B8G8R8_SSCALED);
	AddLuaGlobalEnum(VK_FORMAT_B8G8R8_UINT);
	AddLuaGlobalEnum(VK_FORMAT_B8G8R8_SINT);
	AddLuaGlobalEnum(VK_FORMAT_B8G8R8_SRGB);
	AddLuaGlobalEnum(VK_FORMAT_R8G8B8A8_UNORM);
	AddLuaGlobalEnum(VK_FORMAT_R8G8B8A8_SNORM);
	AddLuaGlobalEnum(VK_FORMAT_R8G8B8A8_USCALED);
	AddLuaGlobalEnum(VK_FORMAT_R8G8B8A8_SSCALED);
	AddLuaGlobalEnum(VK_FORMAT_R8G8B8A8_UINT);
	AddLuaGlobalEnum(VK_FORMAT_R8G8B8A8_SINT);
	AddLuaGlobalEnum(VK_FORMAT_R8G8B8A8_SRGB);
	AddLuaGlobalEnum(VK_FORMAT_B8G8R8A8_UNORM);
	AddLuaGlobalEnum(VK_FORMAT_B8G8R8A8_SNORM);
	AddLuaGlobalEnum(VK_FORMAT_B8G8R8A8_USCALED);
	AddLuaGlobalEnum(VK_FORMAT_B8G8R8A8_SSCALED);
	AddLuaGlobalEnum(VK_FORMAT_B8G8R8A8_UINT);
	AddLuaGlobalEnum(VK_FORMAT_B8G8R8A8_SINT);
	AddLuaGlobalEnum(VK_FORMAT_B8G8R8A8_SRGB);
	AddLuaGlobalEnum(VK_FORMAT_A8B8G8R8_UNORM_PACK32);
	AddLuaGlobalEnum(VK_FORMAT_A8B8G8R8_SNORM_PACK32);
	AddLuaGlobalEnum(VK_FORMAT_A8B8G8R8_USCALED_PACK32);
	AddLuaGlobalEnum(VK_FORMAT_A8B8G8R8_SSCALED_PACK32);
	AddLuaGlobalEnum(VK_FORMAT_A8B8G8R8_UINT_PACK32);
	AddLuaGlobalEnum(VK_FORMAT_A8B8G8R8_SINT_PACK32);
	AddLuaGlobalEnum(VK_FORMAT_A8B8G8R8_SRGB_PACK32);
	AddLuaGlobalEnum(VK_FORMAT_A2R10G10B10_UNORM_PACK32);
	AddLuaGlobalEnum(VK_FORMAT_A2R10G10B10_SNORM_PACK32);
	AddLuaGlobalEnum(VK_FORMAT_A2R10G10B10_USCALED_PACK32);
	AddLuaGlobalEnum(VK_FORMAT_A2R10G10B10_SSCALED_PACK32);
	AddLuaGlobalEnum(VK_FORMAT_A2R10G10B10_UINT_PACK32);
	AddLuaGlobalEnum(VK_FORMAT_A2R10G10B10_SINT_PACK32);
	AddLuaGlobalEnum(VK_FORMAT_A2B10G10R10_UNORM_PACK32);
	AddLuaGlobalEnum(VK_FORMAT_A2B10G10R10_SNORM_PACK32);
	AddLuaGlobalEnum(VK_FORMAT_A2B10G10R10_USCALED_PACK32);
	AddLuaGlobalEnum(VK_FORMAT_A2B10G10R10_SSCALED_PACK32);
	AddLuaGlobalEnum(VK_FORMAT_A2B10G10R10_UINT_PACK32);
	AddLuaGlobalEnum(VK_FORMAT_A2B10G10R10_SINT_PACK32);
	AddLuaGlobalEnum(VK_FORMAT_R16_UNORM);
	AddLuaGlobalEnum(VK_FORMAT_R16_SNORM);
	AddLuaGlobalEnum(VK_FORMAT_R16_USCALED);
	AddLuaGlobalEnum(VK_FORMAT_R16_SSCALED);
	AddLuaGlobalEnum(VK_FORMAT_R16_UINT);
	AddLuaGlobalEnum(VK_FORMAT_R16_SINT);
	AddLuaGlobalEnum(VK_FORMAT_R16_SFLOAT);
	AddLuaGlobalEnum(VK_FORMAT_R16G16_UNORM);
	AddLuaGlobalEnum(VK_FORMAT_R16G16_SNORM);
	AddLuaGlobalEnum(VK_FORMAT_R16G16_USCALED);
	AddLuaGlobalEnum(VK_FORMAT_R16G16_SSCALED);
	AddLuaGlobalEnum(VK_FORMAT_R16G16_UINT);
	AddLuaGlobalEnum(VK_FORMAT_R16G16_SINT);
	AddLuaGlobalEnum(VK_FORMAT_R16G16_SFLOAT);
	AddLuaGlobalEnum(VK_FORMAT_R16G16B16_UNORM);
	AddLuaGlobalEnum(VK_FORMAT_R16G16B16_SNORM);
	AddLuaGlobalEnum(VK_FORMAT_R16G16B16_USCALED);
	AddLuaGlobalEnum(VK_FORMAT_R16G16B16_SSCALED);
	AddLuaGlobalEnum(VK_FORMAT_R16G16B16_UINT);
	AddLuaGlobalEnum(VK_FORMAT_R16G16B16_SINT);
	AddLuaGlobalEnum(VK_FORMAT_R16G16B16_SFLOAT);
	AddLuaGlobalEnum(VK_FORMAT_R16G16B16A16_UNORM);
	AddLuaGlobalEnum(VK_FORMAT_R16G16B16A16_SNORM);
	AddLuaGlobalEnum(VK_FORMAT_R16G16B16A16_USCALED);
	AddLuaGlobalEnum(VK_FORMAT_R16G16B16A16_SSCALED);
	AddLuaGlobalEnum(VK_FORMAT_R16G16B16A16_UINT);
	AddLuaGlobalEnum(VK_FORMAT_R16G16B16A16_SINT);
	AddLuaGlobalEnum(VK_FORMAT_R16G16B16A16_SFLOAT);
	AddLuaGlobalEnum(VK_FORMAT_R32_UINT);
	AddLuaGlobalEnum(VK_FORMAT_R32_SINT);
	AddLuaGlobalEnum(VK_FORMAT_R32_SFLOAT);
	AddLuaGlobalEnum(VK_FORMAT_R32G32_UINT);
	AddLuaGlobalEnum(VK_FORMAT_R32G32_SINT);
	AddLuaGlobalEnum(VK_FORMAT_R32G32_SFLOAT);
	AddLuaGlobalEnum(VK_FORMAT_R32G32B32_UINT);
	AddLuaGlobalEnum(VK_FORMAT_R32G32B32_SINT);
	AddLuaGlobalEnum(VK_FORMAT_R32G32B32_SFLOAT);
	AddLuaGlobalEnum(VK_FORMAT_R32G32B32A32_UINT);
	AddLuaGlobalEnum(VK_FORMAT_R32G32B32A32_SINT);
	AddLuaGlobalEnum(VK_FORMAT_R32G32B32A32_SFLOAT);
	AddLuaGlobalEnum(VK_FORMAT_R64_UINT);
	AddLuaGlobalEnum(VK_FORMAT_R64_SINT);
	AddLuaGlobalEnum(VK_FORMAT_R64_SFLOAT);
	AddLuaGlobalEnum(VK_FORMAT_R64G64_UINT);
	AddLuaGlobalEnum(VK_FORMAT_R64G64_SINT);
	AddLuaGlobalEnum(VK_FORMAT_R64G64_SFLOAT);
	AddLuaGlobalEnum(VK_FORMAT_R64G64B64_UINT);
	AddLuaGlobalEnum(VK_FORMAT_R64G64B64_SINT);
	AddLuaGlobalEnum(VK_FORMAT_R64G64B64_SFLOAT);
	AddLuaGlobalEnum(VK_FORMAT_R64G64B64A64_UINT);
	AddLuaGlobalEnum(VK_FORMAT_R64G64B64A64_SINT);
	AddLuaGlobalEnum(VK_FORMAT_R64G64B64A64_SFLOAT);
	AddLuaGlobalEnum(VK_FORMAT_B10G11R11_UFLOAT_PACK32);
	AddLuaGlobalEnum(VK_FORMAT_E5B9G9R9_UFLOAT_PACK32);
	AddLuaGlobalEnum(VK_FORMAT_D16_UNORM);
	AddLuaGlobalEnum(VK_FORMAT_X8_D24_UNORM_PACK32);
	AddLuaGlobalEnum(VK_FORMAT_D32_SFLOAT);
	AddLuaGlobalEnum(VK_FORMAT_S8_UINT);
	AddLuaGlobalEnum(VK_FORMAT_D16_UNORM_S8_UINT);
	AddLuaGlobalEnum(VK_FORMAT_D24_UNORM_S8_UINT);
	AddLuaGlobalEnum(VK_FORMAT_D32_SFLOAT_S8_UINT);
	AddLuaGlobalEnum(VK_FORMAT_BC1_RGB_UNORM_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_BC1_RGB_SRGB_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_BC1_RGBA_UNORM_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_BC1_RGBA_SRGB_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_BC2_UNORM_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_BC2_SRGB_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_BC3_UNORM_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_BC3_SRGB_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_BC4_UNORM_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_BC4_SNORM_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_BC5_UNORM_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_BC5_SNORM_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_BC6H_UFLOAT_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_BC6H_SFLOAT_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_BC7_UNORM_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_BC7_SRGB_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_EAC_R11_UNORM_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_EAC_R11_SNORM_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_EAC_R11G11_UNORM_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_EAC_R11G11_SNORM_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_ASTC_4x4_UNORM_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_ASTC_4x4_SRGB_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_ASTC_5x4_UNORM_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_ASTC_5x4_SRGB_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_ASTC_5x5_UNORM_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_ASTC_5x5_SRGB_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_ASTC_6x5_UNORM_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_ASTC_6x5_SRGB_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_ASTC_6x6_UNORM_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_ASTC_6x6_SRGB_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_ASTC_8x5_UNORM_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_ASTC_8x5_SRGB_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_ASTC_8x6_UNORM_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_ASTC_8x6_SRGB_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_ASTC_8x8_UNORM_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_ASTC_8x8_SRGB_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_ASTC_10x5_UNORM_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_ASTC_10x5_SRGB_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_ASTC_10x6_UNORM_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_ASTC_10x6_SRGB_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_ASTC_10x8_UNORM_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_ASTC_10x8_SRGB_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_ASTC_10x10_UNORM_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_ASTC_10x10_SRGB_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_ASTC_12x10_UNORM_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_ASTC_12x10_SRGB_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_ASTC_12x12_UNORM_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_ASTC_12x12_SRGB_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_G8B8G8R8_422_UNORM);
	AddLuaGlobalEnum(VK_FORMAT_B8G8R8G8_422_UNORM);
	AddLuaGlobalEnum(VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM);
	AddLuaGlobalEnum(VK_FORMAT_G8_B8R8_2PLANE_420_UNORM);
	AddLuaGlobalEnum(VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM);
	AddLuaGlobalEnum(VK_FORMAT_G8_B8R8_2PLANE_422_UNORM);
	AddLuaGlobalEnum(VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM);
	AddLuaGlobalEnum(VK_FORMAT_R10X6_UNORM_PACK16);
	AddLuaGlobalEnum(VK_FORMAT_R10X6G10X6_UNORM_2PACK16);
	AddLuaGlobalEnum(VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16);
	AddLuaGlobalEnum(VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16);
	AddLuaGlobalEnum(VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16);
	AddLuaGlobalEnum(VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16);
	AddLuaGlobalEnum(VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16);
	AddLuaGlobalEnum(VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16);
	AddLuaGlobalEnum(VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16);
	AddLuaGlobalEnum(VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16);
	AddLuaGlobalEnum(VK_FORMAT_R12X4_UNORM_PACK16);
	AddLuaGlobalEnum(VK_FORMAT_R12X4G12X4_UNORM_2PACK16);
	AddLuaGlobalEnum(VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16);
	AddLuaGlobalEnum(VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16);
	AddLuaGlobalEnum(VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16);
	AddLuaGlobalEnum(VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16);
	AddLuaGlobalEnum(VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16);
	AddLuaGlobalEnum(VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16);
	AddLuaGlobalEnum(VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16);
	AddLuaGlobalEnum(VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16);
	AddLuaGlobalEnum(VK_FORMAT_G16B16G16R16_422_UNORM);
	AddLuaGlobalEnum(VK_FORMAT_B16G16R16G16_422_UNORM);
	AddLuaGlobalEnum(VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM);
	AddLuaGlobalEnum(VK_FORMAT_G16_B16R16_2PLANE_420_UNORM);
	AddLuaGlobalEnum(VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM);
	AddLuaGlobalEnum(VK_FORMAT_G16_B16R16_2PLANE_422_UNORM);
	AddLuaGlobalEnum(VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM);
	AddLuaGlobalEnum(VK_FORMAT_G8_B8R8_2PLANE_444_UNORM);
	AddLuaGlobalEnum(VK_FORMAT_G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16);
	AddLuaGlobalEnum(VK_FORMAT_G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16);
	AddLuaGlobalEnum(VK_FORMAT_G16_B16R16_2PLANE_444_UNORM);
	AddLuaGlobalEnum(VK_FORMAT_A4R4G4B4_UNORM_PACK16);
	AddLuaGlobalEnum(VK_FORMAT_A4B4G4R4_UNORM_PACK16);
	AddLuaGlobalEnum(VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_ASTC_5x4_SFLOAT_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_ASTC_5x5_SFLOAT_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_ASTC_6x5_SFLOAT_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_ASTC_8x5_SFLOAT_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_ASTC_8x6_SFLOAT_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_ASTC_10x5_SFLOAT_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_ASTC_10x6_SFLOAT_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_ASTC_10x8_SFLOAT_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_ASTC_10x10_SFLOAT_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_ASTC_12x10_SFLOAT_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK);
	AddLuaGlobalEnum(VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG);
	AddLuaGlobalEnum(VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG);
	AddLuaGlobalEnum(VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG);
	AddLuaGlobalEnum(VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG);
	AddLuaGlobalEnum(VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG);
	AddLuaGlobalEnum(VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG);
	AddLuaGlobalEnum(VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG);
	AddLuaGlobalEnum(VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG);
	AddLuaGlobalEnum(VK_FORMAT_R16G16_SFIXED5_NV);
	AddLuaGlobalEnum(VK_FORMAT_A1B5G5R5_UNORM_PACK16_KHR);
	AddLuaGlobalEnum(VK_FORMAT_A8_UNORM_KHR);
	AddLuaGlobalEnum(VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK_EXT);
	AddLuaGlobalEnum(VK_FORMAT_ASTC_5x4_SFLOAT_BLOCK_EXT);
	AddLuaGlobalEnum(VK_FORMAT_ASTC_5x5_SFLOAT_BLOCK_EXT);
	AddLuaGlobalEnum(VK_FORMAT_ASTC_6x5_SFLOAT_BLOCK_EXT);
	AddLuaGlobalEnum(VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK_EXT);
	AddLuaGlobalEnum(VK_FORMAT_ASTC_8x5_SFLOAT_BLOCK_EXT);
	AddLuaGlobalEnum(VK_FORMAT_ASTC_8x6_SFLOAT_BLOCK_EXT);
	AddLuaGlobalEnum(VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK_EXT);
	AddLuaGlobalEnum(VK_FORMAT_ASTC_10x5_SFLOAT_BLOCK_EXT);
	AddLuaGlobalEnum(VK_FORMAT_ASTC_10x6_SFLOAT_BLOCK_EXT);
	AddLuaGlobalEnum(VK_FORMAT_ASTC_10x8_SFLOAT_BLOCK_EXT);
	AddLuaGlobalEnum(VK_FORMAT_ASTC_10x10_SFLOAT_BLOCK_EXT);
	AddLuaGlobalEnum(VK_FORMAT_ASTC_12x10_SFLOAT_BLOCK_EXT);
	AddLuaGlobalEnum(VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK_EXT);
	AddLuaGlobalEnum(VK_FORMAT_G8B8G8R8_422_UNORM_KHR);
	AddLuaGlobalEnum(VK_FORMAT_B8G8R8G8_422_UNORM_KHR);
	AddLuaGlobalEnum(VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM_KHR);
	AddLuaGlobalEnum(VK_FORMAT_G8_B8R8_2PLANE_420_UNORM_KHR);
	AddLuaGlobalEnum(VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM_KHR);
	AddLuaGlobalEnum(VK_FORMAT_G8_B8R8_2PLANE_422_UNORM_KHR);
	AddLuaGlobalEnum(VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM_KHR);
	AddLuaGlobalEnum(VK_FORMAT_R10X6_UNORM_PACK16_KHR);
	AddLuaGlobalEnum(VK_FORMAT_R10X6G10X6_UNORM_2PACK16_KHR);
	AddLuaGlobalEnum(VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16_KHR);
	AddLuaGlobalEnum(VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16_KHR);
	AddLuaGlobalEnum(VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16_KHR);
	AddLuaGlobalEnum(VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16_KHR);
	AddLuaGlobalEnum(VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16_KHR);
	AddLuaGlobalEnum(VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16_KHR);
	AddLuaGlobalEnum(VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16_KHR);
	AddLuaGlobalEnum(VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16_KHR);
	AddLuaGlobalEnum(VK_FORMAT_R12X4_UNORM_PACK16_KHR);
	AddLuaGlobalEnum(VK_FORMAT_R12X4G12X4_UNORM_2PACK16_KHR);
	AddLuaGlobalEnum(VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16_KHR);
	AddLuaGlobalEnum(VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16_KHR);
	AddLuaGlobalEnum(VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16_KHR);
	AddLuaGlobalEnum(VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16_KHR);
	AddLuaGlobalEnum(VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16_KHR);
	AddLuaGlobalEnum(VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16_KHR);
	AddLuaGlobalEnum(VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16_KHR);
	AddLuaGlobalEnum(VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16_KHR);
	AddLuaGlobalEnum(VK_FORMAT_G16B16G16R16_422_UNORM_KHR);
	AddLuaGlobalEnum(VK_FORMAT_B16G16R16G16_422_UNORM_KHR);
	AddLuaGlobalEnum(VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM_KHR);
	AddLuaGlobalEnum(VK_FORMAT_G16_B16R16_2PLANE_420_UNORM_KHR);
	AddLuaGlobalEnum(VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM_KHR);
	AddLuaGlobalEnum(VK_FORMAT_G16_B16R16_2PLANE_422_UNORM_KHR);
	AddLuaGlobalEnum(VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM_KHR);
	AddLuaGlobalEnum(VK_FORMAT_G8_B8R8_2PLANE_444_UNORM_EXT);
	AddLuaGlobalEnum(VK_FORMAT_G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16_EXT);
	AddLuaGlobalEnum(VK_FORMAT_G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16_EXT);
	AddLuaGlobalEnum(VK_FORMAT_G16_B16R16_2PLANE_444_UNORM_EXT);
	AddLuaGlobalEnum(VK_FORMAT_A4R4G4B4_UNORM_PACK16_EXT);
	AddLuaGlobalEnum(VK_FORMAT_A4B4G4R4_UNORM_PACK16_EXT);

	AddLuaGlobalEnum(VK_IMAGE_TYPE_1D);
	AddLuaGlobalEnum(VK_IMAGE_TYPE_2D);
	AddLuaGlobalEnum(VK_IMAGE_TYPE_3D);

	AddLuaGlobalEnum(VK_IMAGE_VIEW_TYPE_1D);
	AddLuaGlobalEnum(VK_IMAGE_VIEW_TYPE_2D);
	AddLuaGlobalEnum(VK_IMAGE_VIEW_TYPE_3D);
	AddLuaGlobalEnum(VK_IMAGE_VIEW_TYPE_CUBE);
	AddLuaGlobalEnum(VK_IMAGE_VIEW_TYPE_1D_ARRAY);
	AddLuaGlobalEnum(VK_IMAGE_VIEW_TYPE_2D_ARRAY);
	AddLuaGlobalEnum(VK_IMAGE_VIEW_TYPE_CUBE_ARRAY);

	AddLuaGlobalEnum(VK_SAMPLE_COUNT_1_BIT);
	AddLuaGlobalEnum(VK_SAMPLE_COUNT_2_BIT);
	AddLuaGlobalEnum(VK_SAMPLE_COUNT_4_BIT);
	AddLuaGlobalEnum(VK_SAMPLE_COUNT_8_BIT);
	AddLuaGlobalEnum(VK_SAMPLE_COUNT_16_BIT);
	AddLuaGlobalEnum(VK_SAMPLE_COUNT_32_BIT);
	AddLuaGlobalEnum(VK_SAMPLE_COUNT_64_BIT);

	AddLuaGlobalEnum(VK_IMAGE_TILING_OPTIMAL);
	AddLuaGlobalEnum(VK_IMAGE_TILING_LINEAR);
	AddLuaGlobalEnum(VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT);

	AddLuaGlobalEnum(VK_FILTER_NEAREST);
	AddLuaGlobalEnum(VK_FILTER_LINEAR);
	AddLuaGlobalEnum(VK_FILTER_CUBIC_EXT);
	AddLuaGlobalEnum(VK_FILTER_CUBIC_IMG);

	AddLuaGlobalEnum(VK_SAMPLER_ADDRESS_MODE_REPEAT);
	AddLuaGlobalEnum(VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT);
	AddLuaGlobalEnum(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
	AddLuaGlobalEnum(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER);
	AddLuaGlobalEnum(VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE);

	AddLuaGlobalEnum(VK_DEPENDENCY_BY_REGION_BIT);
	AddLuaGlobalEnum(VK_DEPENDENCY_DEVICE_GROUP_BIT);
	AddLuaGlobalEnum(VK_DEPENDENCY_VIEW_LOCAL_BIT);
	AddLuaGlobalEnum(VK_DEPENDENCY_FEEDBACK_LOOP_BIT_EXT);
	AddLuaGlobalEnum(VK_DEPENDENCY_VIEW_LOCAL_BIT_KHR);
	AddLuaGlobalEnum(VK_DEPENDENCY_DEVICE_GROUP_BIT_KHR);

	AddLuaGlobalEnum(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
	AddLuaGlobalEnum(VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT);
	AddLuaGlobalEnum(VK_PIPELINE_STAGE_VERTEX_INPUT_BIT);
	AddLuaGlobalEnum(VK_PIPELINE_STAGE_VERTEX_SHADER_BIT);
	AddLuaGlobalEnum(VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT);
	AddLuaGlobalEnum(VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT);
	AddLuaGlobalEnum(VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT);
	AddLuaGlobalEnum(VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
	AddLuaGlobalEnum(VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);
	AddLuaGlobalEnum(VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);
	AddLuaGlobalEnum(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
	AddLuaGlobalEnum(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
	AddLuaGlobalEnum(VK_PIPELINE_STAGE_TRANSFER_BIT);
	AddLuaGlobalEnum(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
	AddLuaGlobalEnum(VK_PIPELINE_STAGE_HOST_BIT);
	AddLuaGlobalEnum(VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT);
	AddLuaGlobalEnum(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
	AddLuaGlobalEnum(VK_PIPELINE_STAGE_NONE);
	AddLuaGlobalEnum(VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT);
	AddLuaGlobalEnum(VK_PIPELINE_STAGE_CONDITIONAL_RENDERING_BIT_EXT);
	AddLuaGlobalEnum(VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR);
	AddLuaGlobalEnum(VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);
	AddLuaGlobalEnum(VK_PIPELINE_STAGE_FRAGMENT_DENSITY_PROCESS_BIT_EXT);
	AddLuaGlobalEnum(VK_PIPELINE_STAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR);
	AddLuaGlobalEnum(VK_PIPELINE_STAGE_COMMAND_PREPROCESS_BIT_NV);
	AddLuaGlobalEnum(VK_PIPELINE_STAGE_TASK_SHADER_BIT_EXT);
	AddLuaGlobalEnum(VK_PIPELINE_STAGE_MESH_SHADER_BIT_EXT);
	AddLuaGlobalEnum(VK_PIPELINE_STAGE_SHADING_RATE_IMAGE_BIT_NV);
	AddLuaGlobalEnum(VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_NV);
	AddLuaGlobalEnum(VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV);
	AddLuaGlobalEnum(VK_PIPELINE_STAGE_TASK_SHADER_BIT_NV);
	AddLuaGlobalEnum(VK_PIPELINE_STAGE_MESH_SHADER_BIT_NV);
	AddLuaGlobalEnum(VK_PIPELINE_STAGE_NONE_KHR);
	AddLuaGlobalEnum(VK_PIPELINE_STAGE_COMMAND_PREPROCESS_BIT_EXT);

	AddLuaGlobalEnum(VK_IMAGE_ASPECT_COLOR_BIT);
	AddLuaGlobalEnum(VK_IMAGE_ASPECT_DEPTH_BIT);
	AddLuaGlobalEnum(VK_IMAGE_ASPECT_STENCIL_BIT);
	AddLuaGlobalEnum(VK_IMAGE_ASPECT_METADATA_BIT);
	AddLuaGlobalEnum(VK_IMAGE_ASPECT_PLANE_0_BIT);
	AddLuaGlobalEnum(VK_IMAGE_ASPECT_PLANE_1_BIT);
	AddLuaGlobalEnum(VK_IMAGE_ASPECT_PLANE_2_BIT);
	AddLuaGlobalEnum(VK_IMAGE_ASPECT_NONE);
	AddLuaGlobalEnum(VK_IMAGE_ASPECT_MEMORY_PLANE_0_BIT_EXT);
	AddLuaGlobalEnum(VK_IMAGE_ASPECT_MEMORY_PLANE_1_BIT_EXT);
	AddLuaGlobalEnum(VK_IMAGE_ASPECT_MEMORY_PLANE_2_BIT_EXT);
	AddLuaGlobalEnum(VK_IMAGE_ASPECT_MEMORY_PLANE_3_BIT_EXT);
	AddLuaGlobalEnum(VK_IMAGE_ASPECT_PLANE_0_BIT_KHR);
	AddLuaGlobalEnum(VK_IMAGE_ASPECT_PLANE_1_BIT_KHR);
	AddLuaGlobalEnum(VK_IMAGE_ASPECT_PLANE_2_BIT_KHR);
	AddLuaGlobalEnum(VK_IMAGE_ASPECT_NONE_KHR);

	AddLuaGlobalEnum(VK_ACCESS_INDIRECT_COMMAND_READ_BIT);
	AddLuaGlobalEnum(VK_ACCESS_INDEX_READ_BIT);
	AddLuaGlobalEnum(VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT);
	AddLuaGlobalEnum(VK_ACCESS_UNIFORM_READ_BIT);
	AddLuaGlobalEnum(VK_ACCESS_INPUT_ATTACHMENT_READ_BIT);
	AddLuaGlobalEnum(VK_ACCESS_SHADER_READ_BIT);
	AddLuaGlobalEnum(VK_ACCESS_SHADER_WRITE_BIT);
	AddLuaGlobalEnum(VK_ACCESS_COLOR_ATTACHMENT_READ_BIT);
	AddLuaGlobalEnum(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
	AddLuaGlobalEnum(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT);
	AddLuaGlobalEnum(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
	AddLuaGlobalEnum(VK_ACCESS_TRANSFER_READ_BIT);
	AddLuaGlobalEnum(VK_ACCESS_TRANSFER_WRITE_BIT);
	AddLuaGlobalEnum(VK_ACCESS_HOST_READ_BIT);
	AddLuaGlobalEnum(VK_ACCESS_HOST_WRITE_BIT);
	AddLuaGlobalEnum(VK_ACCESS_MEMORY_READ_BIT);
	AddLuaGlobalEnum(VK_ACCESS_MEMORY_WRITE_BIT);
	AddLuaGlobalEnum(VK_ACCESS_NONE);
	AddLuaGlobalEnum(VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT);
	AddLuaGlobalEnum(VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_READ_BIT_EXT);
	AddLuaGlobalEnum(VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT);
	AddLuaGlobalEnum(VK_ACCESS_CONDITIONAL_RENDERING_READ_BIT_EXT);
	AddLuaGlobalEnum(VK_ACCESS_COLOR_ATTACHMENT_READ_NONCOHERENT_BIT_EXT);
	AddLuaGlobalEnum(VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR);
	AddLuaGlobalEnum(VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR);
	AddLuaGlobalEnum(VK_ACCESS_FRAGMENT_DENSITY_MAP_READ_BIT_EXT);
	AddLuaGlobalEnum(VK_ACCESS_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR);
	AddLuaGlobalEnum(VK_ACCESS_COMMAND_PREPROCESS_READ_BIT_NV);
	AddLuaGlobalEnum(VK_ACCESS_COMMAND_PREPROCESS_WRITE_BIT_NV);
	AddLuaGlobalEnum(VK_ACCESS_SHADING_RATE_IMAGE_READ_BIT_NV);
	AddLuaGlobalEnum(VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV);
	AddLuaGlobalEnum(VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV);
	AddLuaGlobalEnum(VK_ACCESS_NONE_KHR);
	AddLuaGlobalEnum(VK_ACCESS_COMMAND_PREPROCESS_READ_BIT_EXT);
	AddLuaGlobalEnum(VK_ACCESS_COMMAND_PREPROCESS_WRITE_BIT_EXT);

	AddLuaGlobalEnum(VK_ATTACHMENT_LOAD_OP_LOAD);
	AddLuaGlobalEnum(VK_ATTACHMENT_LOAD_OP_CLEAR);
	AddLuaGlobalEnum(VK_ATTACHMENT_LOAD_OP_DONT_CARE);
	AddLuaGlobalEnum(VK_ATTACHMENT_LOAD_OP_NONE_KHR);
	AddLuaGlobalEnum(VK_ATTACHMENT_LOAD_OP_NONE_EXT);

	AddLuaGlobalEnum(VK_ATTACHMENT_STORE_OP_STORE);
	AddLuaGlobalEnum(VK_ATTACHMENT_STORE_OP_DONT_CARE);
	AddLuaGlobalEnum(VK_ATTACHMENT_STORE_OP_NONE);
	AddLuaGlobalEnum(VK_ATTACHMENT_STORE_OP_NONE_KHR);
	AddLuaGlobalEnum(VK_ATTACHMENT_STORE_OP_NONE_QCOM);
	AddLuaGlobalEnum(VK_ATTACHMENT_STORE_OP_NONE_EXT);

	AddLuaGlobalEnum(VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
	AddLuaGlobalEnum(VK_IMAGE_USAGE_TRANSFER_DST_BIT);
	AddLuaGlobalEnum(VK_IMAGE_USAGE_SAMPLED_BIT);
	AddLuaGlobalEnum(VK_IMAGE_USAGE_STORAGE_BIT);
	AddLuaGlobalEnum(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
	AddLuaGlobalEnum(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
	AddLuaGlobalEnum(VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT);
	AddLuaGlobalEnum(VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT);
	AddLuaGlobalEnum(VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR);
	AddLuaGlobalEnum(VK_IMAGE_USAGE_VIDEO_DECODE_SRC_BIT_KHR);
	AddLuaGlobalEnum(VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR);
	AddLuaGlobalEnum(VK_IMAGE_USAGE_FRAGMENT_DENSITY_MAP_BIT_EXT);
	AddLuaGlobalEnum(VK_IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR);
	AddLuaGlobalEnum(VK_IMAGE_USAGE_HOST_TRANSFER_BIT_EXT);
	AddLuaGlobalEnum(VK_IMAGE_USAGE_VIDEO_ENCODE_DST_BIT_KHR);
	AddLuaGlobalEnum(VK_IMAGE_USAGE_VIDEO_ENCODE_SRC_BIT_KHR);
	AddLuaGlobalEnum(VK_IMAGE_USAGE_VIDEO_ENCODE_DPB_BIT_KHR);
	AddLuaGlobalEnum(VK_IMAGE_USAGE_ATTACHMENT_FEEDBACK_LOOP_BIT_EXT);
	AddLuaGlobalEnum(VK_IMAGE_USAGE_INVOCATION_MASK_BIT_HUAWEI);
	AddLuaGlobalEnum(VK_IMAGE_USAGE_SAMPLE_WEIGHT_BIT_QCOM);
	AddLuaGlobalEnum(VK_IMAGE_USAGE_SAMPLE_BLOCK_MATCH_BIT_QCOM);
	AddLuaGlobalEnum(VK_IMAGE_USAGE_SHADING_RATE_IMAGE_BIT_NV);

	AddLuaGlobalEnum(VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
	AddLuaGlobalEnum(VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	AddLuaGlobalEnum(VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT);
	AddLuaGlobalEnum(VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT);
	AddLuaGlobalEnum(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	AddLuaGlobalEnum(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	AddLuaGlobalEnum(VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
	AddLuaGlobalEnum(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	AddLuaGlobalEnum(VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
	AddLuaGlobalEnum(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
	AddLuaGlobalEnum(VK_BUFFER_USAGE_VIDEO_DECODE_SRC_BIT_KHR);
	AddLuaGlobalEnum(VK_BUFFER_USAGE_VIDEO_DECODE_DST_BIT_KHR);
	AddLuaGlobalEnum(VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT);
	AddLuaGlobalEnum(VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_COUNTER_BUFFER_BIT_EXT);
	AddLuaGlobalEnum(VK_BUFFER_USAGE_CONDITIONAL_RENDERING_BIT_EXT);
	AddLuaGlobalEnum(VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR);
	AddLuaGlobalEnum(VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR);
	AddLuaGlobalEnum(VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR);
	AddLuaGlobalEnum(VK_BUFFER_USAGE_VIDEO_ENCODE_DST_BIT_KHR);
	AddLuaGlobalEnum(VK_BUFFER_USAGE_VIDEO_ENCODE_SRC_BIT_KHR);
	AddLuaGlobalEnum(VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT);
	AddLuaGlobalEnum(VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT);
	AddLuaGlobalEnum(VK_BUFFER_USAGE_PUSH_DESCRIPTORS_DESCRIPTOR_BUFFER_BIT_EXT);
	AddLuaGlobalEnum(VK_BUFFER_USAGE_MICROMAP_BUILD_INPUT_READ_ONLY_BIT_EXT);
	AddLuaGlobalEnum(VK_BUFFER_USAGE_MICROMAP_STORAGE_BIT_EXT);
	AddLuaGlobalEnum(VK_BUFFER_USAGE_RAY_TRACING_BIT_NV);
	AddLuaGlobalEnum(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_EXT);
	AddLuaGlobalEnum(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR);
	AddLuaGlobalEnum(VK_BUFFER_USAGE_FLAG_BITS_MAX_ENUM);

	AddLuaGlobalEnum(VK_DESCRIPTOR_TYPE_SAMPLER);
	AddLuaGlobalEnum(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	AddLuaGlobalEnum(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLuaGlobalEnum(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	AddLuaGlobalEnum(VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER);
	AddLuaGlobalEnum(VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER);
	AddLuaGlobalEnum(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	AddLuaGlobalEnum(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	AddLuaGlobalEnum(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC);
	AddLuaGlobalEnum(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC);
	AddLuaGlobalEnum(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT);
	AddLuaGlobalEnum(VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK);
	AddLuaGlobalEnum(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR);
	AddLuaGlobalEnum(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV);
	AddLuaGlobalEnum(VK_DESCRIPTOR_TYPE_SAMPLE_WEIGHT_IMAGE_QCOM);
	AddLuaGlobalEnum(VK_DESCRIPTOR_TYPE_BLOCK_MATCH_IMAGE_QCOM);
	AddLuaGlobalEnum(VK_DESCRIPTOR_TYPE_MUTABLE_EXT);
	AddLuaGlobalEnum(VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT);
	AddLuaGlobalEnum(VK_DESCRIPTOR_TYPE_MUTABLE_VALVE);

	AddLuaGlobalEnum(VK_CULL_MODE_NONE);
	AddLuaGlobalEnum(VK_CULL_MODE_FRONT_BIT);
	AddLuaGlobalEnum(VK_CULL_MODE_BACK_BIT);
	AddLuaGlobalEnum(VK_CULL_MODE_FRONT_AND_BACK);

	AddLuaGlobalInt(engine->backend->renderFormat, "RenderFmt");
	AddLuaGlobalInt(engine->backend->postProcessingFormat, "PresentFmt");
	AddLuaGlobalInt(engine->backend->swapChainImageFormat, "SwapChainFmt");
	AddLuaGlobalInt(engine->backend->normalFormat, "NormalFmt");
	AddLuaGlobalInt(engine->backend->positionFormat, "PositionFmt");
	AddLuaGlobalInt(engine->backend->GIFormat, "GIFmt");
	AddLuaGlobalInt(engine->backend->findDepthFormat(), "DepthFmt");
	AddLuaGlobalInt(engine->backend->findDepthStencilFormat(), "DepthStencilFmt");

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

	engine->Lua_AddSwapChainStuff(L);

	if (luaL_dofile(L, "engine.lua"))
	{
		PrintF("Failed to load and run script! %s\n", lua_tostring(L, -1));
		lua_pop(L, 1);
		throw std::runtime_error("Failed to run engine.lua!");
	}

	lua_getglobal(L, "lightResultRenderPass");
	SunLight::sunShadowPassRenderPass = Lua_GetRenderPass(L, -1)->renderPass;
	lua_pop(L, 1);

	lua_getglobal(L, "spotShadowPassRenderPass");
	SpotLight::spotShadowPassRenderPass = Lua_GetRenderPass(L, -1)->renderPass;
	lua_pop(L, 1);

	lua_getglobal(L, "SampleCount");
	engine->backend->msaaSamples = (VkSampleCountFlagBits)lua_tointeger(L, -1);
	lua_pop(L, 1);

	engine->backend->CreateShadowPassShader();

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