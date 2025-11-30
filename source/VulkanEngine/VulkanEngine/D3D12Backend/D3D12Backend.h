#pragma once

#include <windows.h>
#include "../include/directx/d3dx12.h"
#include "dxgi.h"
#include "dxgi1_4.h"
#include <wrl/client.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <array>
#include "../Backend.h"


#ifdef _DEBUG
#include "D3d12SDKLayers.h"
#define D3DCOMPILE_DEBUG 1
#endif

using Microsoft::WRL::ComPtr;

enum CullMode
{
	CULL_MODE_NONE = D3D12_CULL_MODE_NONE,
	CULL_MODE_BACK = D3D12_CULL_MODE_BACK,
	CULL_MODE_FRONT = D3D12_CULL_MODE_FRONT,
	// Both cull mode does not exist in Direct X, it will be interpreted as BACK
	CULL_MODE_BOTH = D3D12_CULL_MODE_BACK,
};

enum PolygonMode
{
	POLYGON_MODE_FILL = D3D12_FILL_MODE_SOLID,
	POLYGON_MODE_LINE = D3D12_FILL_MODE_WIREFRAME,
	// Point Polygon Mode does not exist in DirectX, it will be interpreted as FILL
	POLYGON_MODE_POINT = D3D12_FILL_MODE_SOLID
};

enum ImageLayout
{
	IMAGE_LAYOUT_UNDEFINED = D3D12_RESOURCE_STATE_COMMON,
	IMAGE_LAYOUT_READ_ONLY = D3D12_RESOURCE_STATE_GENERIC_READ,
	IMAGE_LAYOUT_GENERAL = D3D12_RESOURCE_STATE_COMMON,
	IMAGE_LAYOUT_TRANSFER_SRC = D3D12_RESOURCE_STATE_COPY_SOURCE,
	IMAGE_LAYOUT_TRANSFER_DST = D3D12_RESOURCE_STATE_COPY_DEST,
	IMAGE_LAYOUT_RENDER_TARGET = D3D12_RESOURCE_STATE_RENDER_TARGET,
	IMAGE_LAYOUT_DEPTH_TARGET = D3D12_RESOURCE_STATE_DEPTH_WRITE
};

enum Filter
{
	FILTER_LINEAR = D3D12_FILTER_TYPE_LINEAR,
	FILTER_NEAREST = D3D12_FILTER_TYPE_POINT
};

class D3D12Backend : public Backend
{
private:
	static const UINT MAX_FRAMES_IN_FLIGHT = 2;

	D3D12_VIEWPORT viewport;
	D3D12_RECT scissor;
	ComPtr<IDXGISwapChain3> swapChain;
	ComPtr<ID3D12Device> device;
	ComPtr<ID3D12Resource> renderTargets[MAX_FRAMES_IN_FLIGHT];
	ComPtr<ID3D12CommandAllocator> commandAllocator;
	ComPtr<ID3D12CommandQueue> commandQueue;
	ComPtr<ID3D12RootSignature> rootSignature;
	ComPtr<ID3D12DescriptorHeap> rtvHeap;
	ComPtr<ID3D12PipelineState> pipelineState;
	ComPtr<ID3D12GraphicsCommandList> commandBuffer;
	ComPtr<ID3D12GraphicsCommandList> commandBuffer_DepthPrepass;
	UINT rtvDescriptorSize;

	UINT imageIndex;

	HANDLE fenceEvent;
	ComPtr<ID3D12Fence> fence;
	UINT64 fenceValue;

	std::array<std::vector<ID3D12CommandList*>, MAX_FRAMES_IN_FLIGHT> commandBufferRefs;

	long long recordTime;

private:
	void RecordMainCommandBuffer(uint32_t imageIndex);
public:
	void WaitUntilIdle() override;

	D3D12Backend(GLFWwindow* glWindow, float resolutionScale);
	~D3D12Backend();

	bool PerFrame() override;
};