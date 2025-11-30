#include "D3D12Backend.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <stdexcept>
#include <chrono>
#include "../VulkanBackend/Light.h"
#include "../VulkanBackend/Mesh.h"
#include "d3dcompiler.h"

#define hrcheck(hr, msg) if (FAILED(hr)) throw std::runtime_error(msg)

#define offsetof(type, member) (size_t)&((type*)0)->member

struct Vertex
{
	float3 pos;
	float3 nrm;
	float2 uv;
};

const D3D12_INPUT_ELEMENT_DESC VertexDescription[] = {
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, pos), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, nrm), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(Vertex, uv), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
};

static ComPtr<ID3D12PipelineState> CreateShader(const wchar_t* hlsl)
{
#ifdef _DEBUG
	UINT flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	UINT flags = 0;
#endif
	ComPtr<ID3DBlob> vertexShader, pixelShader;
	D3DCompileFromFile(hlsl, nullptr, nullptr, "VertexShader", "vs_5_0", flags, 0, &vertexShader, nullptr);
	D3DCompileFromFile(hlsl, nullptr, nullptr, "PixelShader", "ps_5_0", flags, 0, &pixelShader, nullptr);


}

D3D12Backend::D3D12Backend(GLFWwindow* glWindow, float resolutionScale)
{
#ifdef _DEBUG
	ComPtr<ID3D12Debug> debugController;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
		debugController->EnableDebugLayer();
#endif

	ComPtr<IDXGIFactory4> factory;
	hrcheck(CreateDXGIFactory1(IID_PPV_ARGS(&factory)), "Failed to create factory!");

	ComPtr<IDXGIAdapter1> adapter;
	hrcheck(factory->EnumAdapters1(0, &adapter), "Failed to get adapter!");
	hrcheck(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device)), "Failed to create device!");

	D3D12_COMMAND_QUEUE_DESC queueDesc{};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue));

	int width, height;
	glfwGetWindowSize(glWindow, &height, &width);

	HWND hWnd = glfwGetWin32Window(glWindow);
	// Describe and create the swap chain.
	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
	swapChainDesc.BufferCount = MAX_FRAMES_IN_FLIGHT;
	swapChainDesc.BufferDesc.Width = width;
	swapChainDesc.BufferDesc.Height = height;
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.OutputWindow = hWnd;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.Windowed = TRUE;

	ComPtr<IDXGISwapChain> chain;
	hrcheck(factory->CreateSwapChain(commandQueue.Get(), &swapChainDesc, &chain), "Failed to create swap chain!");
	hrcheck(chain.As(&swapChain), "Failed to set swap chain!");

	hrcheck(factory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER), "Failed to make window association!");

	imageIndex = swapChain->GetCurrentBackBufferIndex();

	// Create descriptor heaps
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
	heapDesc.NumDescriptors = MAX_FRAMES_IN_FLIGHT;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	if (FAILED(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&rtvHeap))))
		throw "Failed to create descriptor heap!";

	rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	// Create frame resources
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvHeap->GetCPUDescriptorHandleForHeapStart());
	
	// Create an RTV for each frame.
	for (UINT n = 0; n < MAX_FRAMES_IN_FLIGHT; n++)
	{
		if (FAILED(swapChain->GetBuffer(n, IID_PPV_ARGS(&renderTargets[n]))))
			throw "Failed to GetBuffer!";

		device->CreateRenderTargetView(renderTargets[n].Get(), nullptr, rtvHandle);
		rtvHandle.Offset(1, rtvDescriptorSize);
	}

	hrcheck(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator)), "Failed to create command allocator");

	// Create Root Signature (whatever that is)
	CD3DX12_ROOT_SIGNATURE_DESC signatureDesc;
	signatureDesc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> signature;
	ComPtr<ID3DBlob> error;
	hrcheck(D3D12SerializeRootSignature(&signatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error), "Failed to serialize root signature!");

	device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature));

	// Create base shader (for some reason the command list needs an initial pipeline state)
	pipelineState = CreateShader(L"shaders/d3dtest.hlsl");

	// Create command list
	device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), pipelineState.Get(), IID_PPV_ARGS(&commandBuffer));

	commandBuffer->Close();

	Vertex triangleVertices[] = {
		{ { 0.0f, 0.25f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f }},
		{ { 0.0f, 0.25f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f }}
	};

	const UINT vertexBufferSize = sizeof(triangleVertices);

	CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
}

D3D12Backend::~D3D12Backend()
{
	WaitUntilIdle();

	CloseHandle(fenceEvent);
}

void D3D12Backend::WaitUntilIdle()
{
	hrcheck(commandQueue->Signal(fence.Get(), fenceValue), "Failed to signal fence!");
	fenceValue++;

	if (fence->GetCompletedValue() < fenceValue)
	{
		hrcheck(fence->SetEventOnCompletion(fenceValue, fenceEvent), "Failed to SetEventOnCompletion!");
		WaitForSingleObject(fenceEvent, INFINITE);
	}
}

void D3D12Backend::RecordMainCommandBuffer(uint32_t imageIndex)
{
	auto start = std::chrono::high_resolution_clock::now();

	/*
	Light::lightMapImageIndex = imageIndex;

	if (theSun)
		theSun->SetupSunThreads(imageIndex);

	for (size_t i = 0; i < numSpotLights; i++)
	{
		allSpotLights[i]->thread.done = false;
		allSpotLights[i]->thread.go = true;
	}
	*/

	commandAllocator->Reset();

	//auto depthCommands = commandBuffer_DepthPrepass;

	commandBuffer->Reset(commandAllocator.Get(), pipelineState.Get());
	commandBuffer->SetGraphicsRootSignature(rootSignature.Get());
	commandBuffer->RSSetViewports(1, &viewport);
	commandBuffer->RSSetScissorRects(1, &scissor);

	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(renderTargets[imageIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	commandBuffer->ResourceBarrier(1, &barrier);

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvHeap->GetCPUDescriptorHandleForHeapStart(), imageIndex, rtvDescriptorSize);
	commandBuffer->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

	static const float clearValue[] = { 0.0f, 0.2f, 0.4f, 1.0f };
	commandBuffer->ClearRenderTargetView(rtvHandle, clearValue, 0, nullptr);

	commandBuffer->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	commandBuffer->IASetVertexBuffers(0, 1, &bufferView);

	commandBuffer->DrawInstanced(3, 1, 0, 0);

	//DrawRenderStage(commandBuffer, depthCommands, &mainRenderStage, &uniformBufferDescriptorSets[imageIndex][0]);
	//DrawRenderStage(commandBuffer, depthCommands, &mainRenderStageTransparency, &uniformBufferDescriptorSets[imageIndex][0]);

/*
#ifdef ENABLE_DEBUG_COLLISION
	DrawBoundingBoxes(commandBuffer, &mainRenderStage, &uniformBufferDescriptorSets[imageIndex][0]);
	DrawBoundingBoxes(commandBuffer, &mainRenderStageTransparency, &uniformBufferDescriptorSets[imageIndex][0]);
#endif
*/

	//vkCmdEndRenderPass(depthCommands);
	//vkEndCommandBuffer(depthCommands);

	/*
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, UI3DPipeline->pipeline);
	size_t len = UI3D.size();
	for (size_t i = 0; i < len; i++)
	{
		if (!UI3D[i].isStatic)
		{
			DebugPoint3DStruct* data = (DebugPoint3DStruct*)UI3DDescriptorSets[i].buffer->Map();
			data->matrix = activeCamera->matrix;
			data->point = UI3D[i].point;
			UI3DDescriptorSets[i].buffer->UnMap();
		}
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, UI3DPipeline->pipelineLayout, 0, 1, &UI3DDescriptorSets[i].descriptorSet, 0, VK_NULL_HANDLE);
		vkCmdDraw(commandBuffer, 6, 1, 0, 0);
	}

	len = UI2D.size();
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, UI2DPipeline->pipeline);
	for (size_t i = 0; i < len; i++)
	{
		if (!UI2D[i].isStatic)
		{
			DebugPoint2DStruct* data = (DebugPoint2DStruct*)UI2DDescriptorSets[i].buffer->Map();
			data->matrix = activeCamera->matrix;
			data->point = UI2D[i].point;
			UI2DDescriptorSets[i].buffer->UnMap();
		}
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, UI2DPipeline->pipelineLayout, 0, 1, &UI2DDescriptorSets[i].descriptorSet, 0, VK_NULL_HANDLE);
		vkCmdDraw(commandBuffer, 6, 1, 0, 0);
	}
	*/

	// Indicate that the back buffer will now be used to present.
	barrier = CD3DX12_RESOURCE_BARRIER::Transition(renderTargets[imageIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	commandBuffer->ResourceBarrier(1, &barrier);

	commandBuffer->Close();

	/*
	// Optimization idea: What if I combine all cascades into a single texture and use instancing to draw NUMCASCADES copies of the mesh, each one accessing their own viewProj with the instance ID
	// that way it'll draw on every shadow map at the same time with 1 draw call
	// The only caveat is that meshes can't be skipped that way, it'll waste time drawing every mesh from the big map onto the small map where it's probably not going to be visible
	// and clipping will have to be done manually in the shader

	if (theSun)
		SunLight::WaitForSunThreads();

	for (size_t i = 0; i < numSpotLights; i++)
		while (!allSpotLights[i]->thread.done);

	*/

	auto end = std::chrono::high_resolution_clock::now();

	recordTime = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
}

bool D3D12Backend::PerFrame()
{
	//RecordMainCommandBuffer();

	commandQueue->ExecuteCommandLists(commandBufferRefs[imageIndex].size(), commandBufferRefs[imageIndex].data());

	swapChain->Present(1, 0);

	// Does this mean I should be doing VkDeviceWaitIdle() in the vulkan backend?
	WaitUntilIdle();

	// Move to the next image in the swap chain
	imageIndex = swapChain->GetCurrentBackBufferIndex();

	return false;
}