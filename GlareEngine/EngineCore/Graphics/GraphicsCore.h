#pragma once
#include "EngineUtility.h"
#include "PipelineState.h"
#include "DescriptorHeap.h"
#include "RootSignature.h"
#include "SamplerManager.h"
#include "GraphicsCommon.h"



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
		class ColorBuffer;
		class DepthBuffer;
		class GraphicsPSO;
		class TextureManager;

		void Initialize(void);
		void Resize(uint32_t width, uint32_t height);
		void Terminate(void);
		void Shutdown(void);
		void Present(void);
		void PreparePresent(void);

		extern uint32_t g_DisplayWidth;
		extern uint32_t g_DisplayHeight;

		//������Ӧ�ó�����������������֡��.
		uint64_t GetFrameCount(void);

		//�������ɵ�֡�о�����ʱ������ �ڲ���֡�ڼ䣬CPU/GPU���ܴ��ڿ���״̬��
		//֡ʱ�������ʾÿ��֡�ĵ���֮���ʱ�䡣
		float GetFrameTime(void);

		//ÿ�����֡��
		float GetFrameRate(void);

		ColorBuffer& GetCurrentBuffer();

		//Device
		extern ID3D12Device* g_Device;
		extern CommandListManager g_CommandManager;
		extern ContextManager g_ContextManager;
		//Swap Chain
		extern IDXGISwapChain1* s_SwapChain1;

		extern DXGI_FORMAT g_SwapChainFormat;

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