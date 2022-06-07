#pragma once
#include "Display.h"
#include "DescriptorHeap.h"
namespace GlareEngine
{
	enum class RenderPipelineType:int
	{
		Forward,
		Forward_Plus,
		Deferred
	};

	namespace DirectX12Graphics
	{
		using namespace Microsoft::WRL;

		class CommandContext;
		class CommandListManager;
		class CommandSignature;
		class ContextManager;

		void Initialize(bool RequireDXRSupport = false);
		void Shutdown(void);

		//Device
		extern ID3D12Device* g_Device;
		extern CommandListManager g_CommandManager;
		extern ContextManager g_ContextManager;

		//FEATURE LEVE 
		extern D3D_FEATURE_LEVEL g_D3DFeatureLevel;

		extern bool g_bTypedUAVLoadSupport_R11G11B10_FLOAT;

		//Descriptor Allocator 
		extern DescriptorAllocator g_DescriptorAllocator[];
		inline D3D12_CPU_DESCRIPTOR_HANDLE AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE Type, UINT Count = 1)
		{
			return g_DescriptorAllocator[Type].Allocate(Count);
		}

		//SRV Descriptors Manager ,return Descriptor index
		extern vector<D3D12_CPU_DESCRIPTOR_HANDLE> g_TextureSRV;
		inline int AddToGlobalTextureSRVDescriptor(const D3D12_CPU_DESCRIPTOR_HANDLE& SRVdes)
		{
			g_TextureSRV.push_back(SRVdes);
			return int(g_TextureSRV.size() - 1);
		}

		extern vector<D3D12_CPU_DESCRIPTOR_HANDLE> g_CubeSRV;
		inline int AddToGlobalCubeSRVDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE& SRVdes)
		{
			g_CubeSRV.push_back(SRVdes);
			return int(g_CubeSRV.size() - 1);
		}
	}
}
