#pragma once
#include "Display.h"
namespace GlareEngine
{
	using namespace Microsoft::WRL;

	class CommandListManager;
	class CommandSignature;
	class ContextManager;
	class DescriptorAllocator;

	void InitializeGraphics(bool RequireDXRSupport = false);
	void ShutdownGraphics(void);

	//Device
	extern ID3D12Device* g_Device;
	extern CommandListManager g_CommandManager;
	extern ContextManager g_ContextManager;

	//FEATURE LEVE 
	extern D3D_FEATURE_LEVEL g_D3DFeatureLevel;

	extern bool g_bTypedUAVLoadSupport_R11G11B10_FLOAT;

	IDXGIAdapter1* GetAdapter();

	//Descriptor Allocator 
	extern DescriptorAllocator g_DescriptorAllocator[];
	D3D12_CPU_DESCRIPTOR_HANDLE AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE Type, UINT Count = 1);

	//SRV Descriptors Manager ,return Descriptor index
	extern vector<D3D12_CPU_DESCRIPTOR_HANDLE> g_TextureSRV;
	int AddToGlobalTextureSRVDescriptor(const D3D12_CPU_DESCRIPTOR_HANDLE& SRVdes);

	extern vector<D3D12_CPU_DESCRIPTOR_HANDLE> g_CubeSRV;
	int AddToGlobalCubeSRVDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE& SRVdes);

	extern vector<D3D12_CPU_DESCRIPTOR_HANDLE> g_RayTracingBuffer;
	int AddToGlobalRayTracingDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE& SRVdes);
}
