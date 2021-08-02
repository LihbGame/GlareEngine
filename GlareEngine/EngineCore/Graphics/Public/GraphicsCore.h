#pragma once
#include "EngineUtility.h"
#include "PipelineState.h"
#include "DescriptorHeap.h"
#include "RootSignature.h"
#include "SamplerManager.h"
#include "GraphicsCommon.h"



namespace GlareEngine
{

	namespace DirectX12Graphics
	{
		using namespace Microsoft::WRL;

		class CommandContext;
		class CommandListManager;
		class CommandSignature;
		class ContextManager;
		class ColorBuffer;
		class DepthBuffer;
		class GraphicsPSO;
		class TextureManager;

		void Initialize(void);
		void Resize(uint32_t width, uint32_t height);
		void Terminate(void);
		void Shutdown(void);
		void Present(void);

		extern uint32_t g_DisplayWidth;
		extern uint32_t g_DisplayHeight;

		//返回自应用程序启动以来经过的帧数.
		uint64_t GetFrameCount(void);

		//在最后完成的帧中经过的时间量。 在部分帧期间，CPU/GPU可能处于空闲状态。
		//帧时间测量表示每个帧的调用之间的时间。
		float GetFrameTime(void);

		//每秒的总帧数
		float GetFrameRate(void);

		//Device
		extern ID3D12Device* g_Device;
		extern CommandListManager g_CommandManager;
		extern ContextManager g_ContextManager;
		//Swap Chain
		extern IDXGISwapChain1* s_SwapChain1;

		extern DXGI_FORMAT g_SwapChainFormat;

		//Core Features
		extern TextureManager g_TextureManager;

		//FEATURE LEVE 
		extern D3D_FEATURE_LEVEL g_D3DFeatureLevel;

		//HDR
		extern bool g_bTypedUAVLoadSupport_R11G11B10_FLOAT;
		extern bool g_bEnableHDROutput;

		//Descriptor Allocator 
		extern DescriptorAllocator g_DescriptorAllocator[];
		inline D3D12_CPU_DESCRIPTOR_HANDLE AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE Type, UINT Count = 1)
		{
			return g_DescriptorAllocator[Type].Allocate(Count);
		}

		extern RootSignature g_GenerateMipsRS;
		extern ComputePSO g_GenerateMipsLinearPSO[4];
		extern ComputePSO g_GenerateMipsGammaPSO[4];

		enum eResolution { E720p,E900p, E1080p, E1440p, E1800p, E2160p };

		extern BoolVar s_EnableVSync;
		extern EnumVar TargetResolution;
		extern uint32_t g_DisplayWidth;
		extern uint32_t g_DisplayHeight;
	}
}
