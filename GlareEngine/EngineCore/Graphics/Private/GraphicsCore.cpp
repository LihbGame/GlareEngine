#include "GraphicsCore.h"
#include "SamplerManager.h"
#include "DescriptorHeap.h"
#include "CommandContext.h"
#include "CommandListManager.h"
#include "RootSignature.h"
#include "CommandSignature.h"
#include "EngineAdjust.h"
#include "ColorBuffer.h"
#include "GPUTimeManager.h"

// 该宏决定是否检测是否有 HDR 显示并启用 HDR10 输出。
// 目前，在启用 HDR 显示的情况下，像素放大功能被破坏。
#define CONDITIONALLY_ENABLE_HDR_OUTPUT 1

#define SWAP_CHAIN_BUFFER_COUNT 3


#if defined(NTDDI_WIN10_RS2) && (NTDDI_VERSION >= NTDDI_WIN10_RS2)
#include <dxgi1_6.h>
#else
#include <dxgi1_4.h>    // For WARP
#endif
#include <winreg.h>        // To read the registry


DXGI_FORMAT SwapChainFormat = DXGI_FORMAT_R10G10B10A2_UNORM;



namespace GlareEngine
{
	using namespace GlareEngine::Math;

	namespace GameCore
	{
		extern HWND g_hWnd;
	}


	float s_FrameTime = 0.0f;
	uint64_t s_FrameIndex = 0;
	int64_t s_FrameStartTick = 0;

	BoolVar s_LimitTo30Hz("Timing/Limit To 30Hz", false);
	BoolVar s_DropRandomFrames("Timing/Drop Random Frames", false);


	namespace DirectX12Graphics
	{
		void PreparePresentLDR();
		void PreparePresentHDR();
		void CompositeOverlays(GraphicsContext& Context);

		const uint32_t MaxNativeWidth = 3840;
		const uint32_t MaxNativeHeight = 2160;
		const uint32_t NumPredefinedResolutions = 6;

		const char* ResolutionLabels[] = { "1280x720", "1600x900", "1920x1080", "2560x1440", "3200x1800", "3840x2160" };
		//Use 900P
		EnumVar TargetResolution("Graphics/Display/Native Resolution", E900p, NumPredefinedResolutions, ResolutionLabels);

		BoolVar s_EnableVSync("Timing/VSync", true);

		//HDR FORMAT
		bool g_bTypedUAVLoadSupport_R11G11B10_FLOAT = false;
		bool g_bTypedUAVLoadSupport_R16G16B16A16_FLOAT = false;

		bool g_bEnableHDROutput = false;

		//Color range
		NumVar g_HDRPaperWhite("Graphics/Display/Paper White (nits)", 200.0f, 100.0f, 500.0f, 50.0f);
		NumVar g_MaxDisplayLuminance("Graphics/Display/Peak Brightness (nits)", 1000.0f, 500.0f, 10000.0f, 100.0f);
		
		const char* HDRModeLabels[] = { "HDR", "SDR", "Side-by-Side" };
		EnumVar HDRDebugMode("Graphics/Display/HDR Debug Mode", 0, 3, HDRModeLabels);

		uint32_t g_NativeWidth = 0;
		uint32_t g_NativeHeight = 0;
		uint32_t g_DisplayWidth = 1600;
		uint32_t g_DisplayHeight = 900;
		ColorBuffer g_PreDisplayBuffer;


		void SetNativeResolution(void)
		{
			uint32_t NativeWidth, NativeHeight;

			switch (eResolution((int)TargetResolution))
			{
			default:
			case E720p:
				NativeWidth = 1280;
				NativeHeight = 720;
				break;
			case E900p:
				NativeWidth = 1600;
				NativeHeight = 900;
				break;
			case E1080p:
				NativeWidth = 1920;
				NativeHeight = 1080;
				break;
			case E1440p:
				NativeWidth = 2560;
				NativeHeight = 1440;
				break;
			case E1800p:
				NativeWidth = 3200;
				NativeHeight = 1800;
				break;
			case E2160p:
				NativeWidth = 3840;
				NativeHeight = 2160;
				break;
			}

			if (g_NativeWidth == NativeWidth && g_NativeHeight == NativeHeight)
				return;

			g_NativeWidth = NativeWidth;
			g_NativeHeight = NativeHeight;

			g_CommandManager.IdleGPU();

			//resize render buffer
			InitializeRenderingBuffers(NativeWidth, NativeHeight);
		}

		//Device
		ID3D12Device* g_Device = nullptr;
		CommandListManager g_CommandManager;
		ContextManager g_ContextManager;
		
		//FEATURE LEVEL 11
		D3D_FEATURE_LEVEL g_D3DFeatureLevel = D3D_FEATURE_LEVEL_11_0;
		
		//Display Buffer(三缓存)
		ColorBuffer g_DisplayBuffers[SWAP_CHAIN_BUFFER_COUNT];
		UINT g_CurrentBuffer = 0;
		
		//Swap Chain
		IDXGISwapChain1* s_SwapChain1 = nullptr;

		//Descriptor Allocator
		DescriptorAllocator g_DescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES] =
		{
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
			D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
			D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
			D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
		};

		RootSignature s_PresentRS;
		GraphicsPSO PresentSDRPS;
		GraphicsPSO PresentHDRPS;
		GraphicsPSO MagnifyPixelsPS;
		GraphicsPSO SharpeningUpsamplePS;
		GraphicsPSO BicubicHorizontalUpsamplePS;
		GraphicsPSO BicubicVerticalUpsamplePS;
		GraphicsPSO BilinearUpsamplePS;

		//Generate Mips
		RootSignature g_GenerateMipsRS;
		ComputePSO g_GenerateMipsLinearPSO[4];
		ComputePSO g_GenerateMipsGammaPSO[4];

		enum { EBilinear, EBicubic, ESharpening, EFilterCount };
		const char* FilterLabels[] = { "Bilinear", "Bicubic", "Sharpening" };
		EnumVar UpsampleFilter("Graphics/Display/Upsample Filter", EFilterCount - 1, EFilterCount, FilterLabels);
		NumVar BicubicUpsampleWeight("Graphics/Display/Bicubic Filter Weight", -0.75f, -1.0f, -0.25f, 0.25f);
		NumVar SharpeningSpread("Graphics/Display/Sharpness Sample Spread", 1.0f, 0.7f, 2.0f, 0.1f);
		NumVar SharpeningRotation("Graphics/Display/Sharpness Sample Rotation", 45.0f, 0.0f, 90.0f, 15.0f);
		NumVar SharpeningStrength("Graphics/Display/Sharpness Strength", 0.10f, 0.0f, 1.0f, 0.01f);

		enum DebugZoomLevel { EDebugZoomOff, EDebugZoom2x, EDebugZoom4x, EDebugZoom8x, EDebugZoom16x, EDebugZoomCount };
		const char* DebugZoomLabels[] = { "Off", "2x Zoom", "4x Zoom", "8x Zoom", "16x Zoom" };
		EnumVar DebugZoom("Graphics/Display/Magnify Pixels", EDebugZoomOff, EDebugZoomCount, DebugZoomLabels);
	}



	void DirectX12Graphics::Initialize(void)
	{
		assert(s_SwapChain1 == nullptr, "Graphics has already been initialized");

	}

	void DirectX12Graphics::Resize(uint32_t width, uint32_t height)
	{
		assert(s_SwapChain1 != nullptr);

		// Check for invalid window dimensions
		if (width == 0 || height == 0)
			return;

		// Check for an unneeded resize
		if (width == g_DisplayWidth && height == g_DisplayHeight)
			return;

		g_CommandManager.IdleGPU();

		g_DisplayWidth = width;
		g_DisplayHeight = height;

		g_PreDisplayBuffer.Create(L"PreDisplay Buffer", width, height, 1, SwapChainFormat);

		//Destroy old display buffers
		for (uint32_t i = 0; i < SWAP_CHAIN_BUFFER_COUNT; ++i)
			g_DisplayBuffers[i].Destroy();

		//Resize Swap Chain 
		ThrowIfFailed(s_SwapChain1->ResizeBuffers(SWAP_CHAIN_BUFFER_COUNT, width, height, SwapChainFormat, 0));

		for (uint32_t i = 0; i < SWAP_CHAIN_BUFFER_COUNT; ++i)
		{
			ComPtr<ID3D12Resource> DisplayBuffer;
			ThrowIfFailed(s_SwapChain1->GetBuffer(i, IID_PPV_ARGS(&DisplayBuffer)));
			g_DisplayBuffers[i].CreateFromSwapChain(L"Primary SwapChain Buffer", DisplayBuffer.Detach());
		}

		//reset current buffer index
		g_CurrentBuffer = 0;
		g_CommandManager.IdleGPU();

		ResizeDisplayDependentBuffers(g_NativeWidth, g_NativeHeight);
	}

	void DirectX12Graphics::Terminate(void)
	{
		g_CommandManager.IdleGPU();
		s_SwapChain1->SetFullscreenState(FALSE, nullptr);
	}

	void DirectX12Graphics::Shutdown(void)
	{
		CommandContext::DestroyAllContexts();
		g_CommandManager.Shutdown();
		GPUTimeManager::Shutdown();
		s_SwapChain1->Release();
		PSO::DestroyAll();
		RootSignature::DestroyAll();
		DescriptorAllocator::DestroyAll();

		DestroyCommonState();
		DestroyRenderingBuffers();

		//TemporalEffects::Shutdown();
		//PostEffects::Shutdown();
		//SSAO::Shutdown();
		//ParticleEffects::Shutdown();
		//TextureManager::Shutdown();

		for (UINT i = 0; i < SWAP_CHAIN_BUFFER_COUNT; ++i)
			g_DisplayBuffers[i].Destroy();

		g_PreDisplayBuffer.Destroy();

#if defined(_DEBUG)
		ID3D12DebugDevice* debugInterface;
		if (SUCCEEDED(g_Device->QueryInterface(&debugInterface)))
		{
			debugInterface->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL);
			debugInterface->Release();
		}
#endif

		SAFE_RELEASE(g_Device);
	}

	void DirectX12Graphics::Present(void)
	{
		if (g_bEnableHDROutput)
			PreparePresentHDR();
		else
			PreparePresentLDR();

		g_CurrentBuffer = (g_CurrentBuffer + 1) % SWAP_CHAIN_BUFFER_COUNT;

		UINT PresentInterval = s_EnableVSync ? min(4, (int)round(s_FrameTime * 60.0f)) : 0;
		s_SwapChain1->Present(PresentInterval, 0);


		float CurrentTick = GameTimer::TotalTime();

		if (s_EnableVSync)
		{
			// 启用 VSync 后，帧之间的时间步长变为 16.666 ms 的倍数。 
			//我们需要添加逻辑以在 1 和 2（或 3 个字段）之间变化。 
			//这个增量时间还决定了前一帧应该显示多长时间（即当前间隔）。
			s_FrameTime = (s_LimitTo30Hz ? 2.0f : 1.0f) / 60.0f;
			if (s_DropRandomFrames)
			{
				if (std::rand() % 50 == 0)
					s_FrameTime += (1.0f / 60.0f);
			}
		}
		else
		{
			//自由运行时，保留最近的总帧时间作为下一帧模拟的时间步长。 
			//这不是非常准确，但假设帧时间平滑变化，它应该足够接近。
			s_FrameTime = CurrentTick-s_FrameStartTick;
		}

		s_FrameStartTick = CurrentTick;

		++s_FrameIndex;
		//TemporalEffects::Update((uint32_t)s_FrameIndex);

		SetNativeResolution();
	}

	uint64_t DirectX12Graphics::GetFrameCount(void)
	{
		return s_FrameIndex;
	}

	float DirectX12Graphics::GetFrameTime(void)
	{
		return s_FrameTime;
	}

	float DirectX12Graphics::GetFrameRate(void)
	{
		return s_FrameTime == 0.0f ? 0.0f : 1.0f / s_FrameTime;
	}







}



