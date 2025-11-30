#include "D3D9Backend.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

struct ThingData
{

};

Camera* activeCamera;

//std::vector<ThingData> things = {};

IDirect3DVertexBuffer9* vertexBuffer;
IDirect3DIndexBuffer9* indexBuffer;

D3D9Backend::D3D9Backend(GLFWwindow* glWindow, float resolutionScale)
{
	d3d = Direct3DCreate9(D3D_SDK_VERSION);

	HWND hWnd = glfwGetWin32Window(glWindow);
	int width, height;
	glfwGetWindowSize(glWindow, &width, &height);

	D3DPRESENT_PARAMETERS params{};
	params.hDeviceWindow = hWnd;
	params.SwapEffect = D3DSWAPEFFECT_DISCARD;
	params.EnableAutoDepthStencil = TRUE;
	params.BackBufferFormat = D3DFMT_A8R8G8B8;
	params.BackBufferWidth = width;
	params.BackBufferHeight = height;
	params.BackBufferCount = 2;
	params.Windowed = TRUE;
	d3d->CreateDevice(0, D3DDEVTYPE_HAL, hWnd, 0, &params, &device);

	device->CreateVertexBuffer(1, 0, D3DFVF_XYZ, D3DPOOL_DEFAULT, &vertexBuffer, NULL);

	float3* data;
	vertexBuffer->Lock(0, sizeof(float3) * 3, (void**)&data, 0);

	data[0] = float3(0, 0, 1);
	data[1] = float3(0, 1, 1);
	data[2] = float3(1, 0, 1);

	vertexBuffer->Unlock();
}

D3D9Backend::~D3D9Backend()
{
	if (device)
		device->Release();

	if (d3d)
		d3d->Release();
}





bool D3D9Backend::PerFrame()
{
	static float4x4 perspectiveMatrix = glm::perspective(0.5f, (float)16 / 9, 0.1f, 1500.f);
	static float4x4 viewMatrix = glm::lookAt(float3(0), float3(0, 1, 0), float3(0, 0, 1));
	static float4x4 worldMatrix = glm::identity<float4x4>();

	device->Clear(1, NULL, D3DCLEAR_STENCIL | D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, RGB(0, 0, 0), 1.0f, 0);

	device->BeginScene();
	device->SetFVF(D3DFVF_XYZ);
	device->SetTransform(D3DTS_WORLD, (D3DMATRIX*)&worldMatrix);
	device->SetTransform(D3DTS_VIEW, (D3DMATRIX*)&viewMatrix);
	device->SetTransform(D3DTS_PROJECTION, (D3DMATRIX*)&perspectiveMatrix);
	device->SetStreamSource(0, vertexBuffer, 0, sizeof(float3) * 3);

	device->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 1);

	device->EndScene();

	device->Present(NULL, NULL, NULL, NULL);

	return false;
}

bool MeshGroupOnScreen()
{
	return true;
}

int LuaFN_CreateRenderPass(lua_State* L)
{
	return 0;
}

int LuaFN_RenderPassGC(lua_State* L)
{
	return 0;
}

int LuaFN_CreateImage(lua_State* L)
{
	return 0;
}

int LuaFN_NewComputeShader(lua_State* L)
{
	return 0;
}

static int LuaFN_RunComputeShader(lua_State* L)
{
	return 0;
}

int LuaFN_SpotLightNewIndex(lua_State* L)
{
	return 0;
}

int LuaFN_CreateFrameBuffer(lua_State* L)
{
	return 0;
}

bool RecompileShaderThreadProc(ThreadSyncer* sync)
{
	return false;
}

void SetActiveCamera(Camera* camera)
{
	activeCamera = camera;
}

Camera*& GetActiveCamera()
{
	return activeCamera;
}