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
#include "EngineLog.h"
#include "BufferManager.h"
#include "TextureManager.h"

#include "ScreenGrab.h"
//Shaders
#include "CompiledShaders/PresentSDRPS.h"
#include "CompiledShaders/ScreenQuadVS.h"
#include "CompiledShaders/BufferCopyPS.h"

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


	

	namespace DirectX12Graphics
	{
		
		void CreateD3DDevice();
		void CheckUAVFormatSupport();
		void CheckHDRSupport();
		void InitializePersentPSO();

		void PreparePresentLDR();
		void PreparePresentHDR();

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

			//Resize Render Buffer
			InitializeRenderingBuffers(NativeWidth, NativeHeight);
		}

		//Device
		ID3D12Device* g_Device = nullptr;
		CommandListManager g_CommandManager;
		ContextManager g_ContextManager;

		//SwapChainFormat
		DXGI_FORMAT g_SwapChainFormat = DXGI_FORMAT_R10G10B10A2_UNORM;

		//FEATURE LEVEL 11
		D3D_FEATURE_LEVEL g_D3DFeatureLevel = D3D_FEATURE_LEVEL_11_0;

		//Display Buffer(三缓存)
		ColorBuffer g_DisplayBuffers[SWAP_CHAIN_BUFFER_COUNT];
		UINT g_CurrentBuffer = 0;

		ColorBuffer& GetCurrentBuffer() { return g_DisplayBuffers[g_CurrentBuffer]; }

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

		//SRV Descriptors Manager ,return Descriptor index
		vector<D3D12_CPU_DESCRIPTOR_HANDLE> g_TextureSRV;
		vector<D3D12_CPU_DESCRIPTOR_HANDLE> g_CubeSRV;

		RootSignature s_PresentRS;
		GraphicsPSO s_PresentPSO;
		GraphicsPSO PresentSDRPS;
		GraphicsPSO PresentHDRPS;
	}

	void DirectX12Graphics::CreateD3DDevice()
	{
		Microsoft::WRL::ComPtr<ID3D12Device> pDevice;
		// Obtain the DXGI factory
		Microsoft::WRL::ComPtr<IDXGIFactory4> dxgiFactory;
		ThrowIfFailed(CreateDXGIFactory2(0, IID_PPV_ARGS(&dxgiFactory)));

		// Create the D3D graphics device
		Microsoft::WRL::ComPtr<IDXGIAdapter1> pAdapter;
		//是否使用Windows软光栅
		static const bool bUseWarpDriver = false;

		if (!bUseWarpDriver)
		{
			SIZE_T MaxSize = 0;

			for (uint32_t Idx = 0; DXGI_ERROR_NOT_FOUND != dxgiFactory->EnumAdapters1(Idx, &pAdapter); ++Idx)
			{
				DXGI_ADAPTER_DESC1 desc;
				pAdapter->GetDesc1(&desc);
				if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
					continue;

				if (desc.DedicatedVideoMemory > MaxSize && SUCCEEDED(D3D12CreateDevice(pAdapter.Get(), D3D_FEATURE_LEVEL_11_1, IID_PPV_ARGS(&pDevice))))
				{
					pAdapter->GetDesc1(&desc);
					EngineLog::AddLog(L"D3D12-Capable HardWare found:  %s (%d MB)\0", desc.Description, desc.DedicatedVideoMemory >> 20);
					MaxSize = desc.DedicatedVideoMemory;
				}
			}

			if (MaxSize > 0)
				g_Device = pDevice.Detach();
		}

		if (g_Device == nullptr)
		{
			if (bUseWarpDriver)
				EngineLog::AddLog(L"WARP software adapter requested.  Initializing...\n");
			else
				EngineLog::AddLog(L"Failed to find a hardware adapter.  Falling back to WARP.\n");
			ThrowIfFailed(dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&pAdapter)));
			ThrowIfFailed(D3D12CreateDevice(pAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&pDevice)));
			g_Device = pDevice.Detach();
		}

		g_CommandManager.Create(g_Device);

		//Swap Chain
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
		swapChainDesc.Width = g_DisplayWidth;
		swapChainDesc.Height = g_DisplayHeight;
		swapChainDesc.Format = g_SwapChainFormat;
		swapChainDesc.Scaling = DXGI_SCALING_NONE;
		swapChainDesc.SampleDesc.Quality = 0;
		swapChainDesc.SampleDesc.Count = 1;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.BufferCount = SWAP_CHAIN_BUFFER_COUNT;
		swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;

		ThrowIfFailed(dxgiFactory->CreateSwapChainForHwnd(g_CommandManager.GetCommandQueue(), GameCore::g_hWnd, &swapChainDesc, nullptr, nullptr, &s_SwapChain1));

		//禁止 alt+enter 全屏
		dxgiFactory->MakeWindowAssociation(GameCore::g_hWnd, DXGI_MWA_NO_WINDOW_CHANGES | DXGI_MWA_NO_ALT_ENTER);
	}

	void DirectX12Graphics::CheckUAVFormatSupport()
	{
		// We like to do read-modify-write operations on UAVs during post processing.  To support that, we
		// need to either have the hardware do typed UAV loads of R11G11B10_FLOAT or we need to manually
		// decode an R32_UINT representation of the same buffer.  This code determines if we get the hardware
		// load support.
		D3D12_FEATURE_DATA_D3D12_OPTIONS FeatureData = {};
		if (SUCCEEDED(g_Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &FeatureData, sizeof(FeatureData))))
		{
			if (FeatureData.TypedUAVLoadAdditionalFormats)
			{
				D3D12_FEATURE_DATA_FORMAT_SUPPORT Support =
				{
					DXGI_FORMAT_R11G11B10_FLOAT, D3D12_FORMAT_SUPPORT1_NONE, D3D12_FORMAT_SUPPORT2_NONE
				};

				if (SUCCEEDED(g_Device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &Support, sizeof(Support))) &&
					(Support.Support2 & D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD) != 0)
				{
					g_bTypedUAVLoadSupport_R11G11B10_FLOAT = true;
				}

				Support.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;

				if (SUCCEEDED(g_Device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &Support, sizeof(Support))) &&
					(Support.Support2 & D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD) != 0)
				{
					g_bTypedUAVLoadSupport_R16G16B16A16_FLOAT = true;
				}
			}
		}
	}

	void DirectX12Graphics::CheckHDRSupport()
	{
		//检查是否支持HDR
#if CONDITIONALLY_ENABLE_HDR_OUTPUT && defined(NTDDI_WIN10_RS2) && (NTDDI_VERSION >= NTDDI_WIN10_RS2)
		{
			IDXGISwapChain4* swapChain = (IDXGISwapChain4*)s_SwapChain1;
			ComPtr<IDXGIOutput> output;
			ComPtr<IDXGIOutput6> output6;
			DXGI_OUTPUT_DESC1 outputDesc;
			UINT colorSpaceSupport;

			// Query support for ST.2084 on the display and set the color space accordingly
			if (SUCCEEDED(swapChain->GetContainingOutput(&output)) &&
				SUCCEEDED(output.As(&output6)) &&
				SUCCEEDED(output6->GetDesc1(&outputDesc)) &&
				outputDesc.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020 &&
				SUCCEEDED(swapChain->CheckColorSpaceSupport(DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020, &colorSpaceSupport)) &&
				(colorSpaceSupport & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT) &&
				SUCCEEDED(swapChain->SetColorSpace1(DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020)))
			{
				g_bEnableHDROutput = true;
			}
		}
#endif
	}

	void DirectX12Graphics::InitializePersentPSO()
	{
		s_PresentRS.Reset(4, 2);
		s_PresentRS[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 2);
		s_PresentRS[1].InitAsConstants(0, 6, D3D12_SHADER_VISIBILITY_ALL);
		s_PresentRS[2].InitAsBufferSRV(2, D3D12_SHADER_VISIBILITY_PIXEL);
		s_PresentRS[3].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1);
		s_PresentRS.InitStaticSampler(0, SamplerLinearWrapDesc);
		s_PresentRS.InitStaticSampler(1, SamplerPointClampDesc);
		s_PresentRS.Finalize(L"Present");

		// Initialize Present PSOs
		s_PresentPSO.SetRootSignature(s_PresentRS);
		s_PresentPSO.SetRasterizerState(RasterizerTwoSided);
		s_PresentPSO.SetBlendState(BlendPreMultiplied);
		s_PresentPSO.SetDepthStencilState(DepthStateDisabled);
		s_PresentPSO.SetSampleMask(0xFFFFFFFF);
		s_PresentPSO.SetInputLayout(0, nullptr);
		s_PresentPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
		s_PresentPSO.SetVertexShader(g_pScreenQuadVS, sizeof(g_pScreenQuadVS));
		s_PresentPSO.SetPixelShader(g_pBufferCopyPS, sizeof(g_pBufferCopyPS));
		s_PresentPSO.SetRenderTargetFormat(g_SwapChainFormat, DXGI_FORMAT_UNKNOWN);
		s_PresentPSO.Finalize();

		//Create SDR PSO
		PresentSDRPS = s_PresentPSO;
		PresentSDRPS.SetBlendState(BlendDisable);
		PresentSDRPS.SetPixelShader(g_pPresentSDRPS, sizeof(g_pPresentSDRPS));
		PresentSDRPS.Finalize();

		//Create HDR PSO
		PresentHDRPS = PresentSDRPS;
		//PresentHDRPS.SetPixelShader(g_pPresentHDRPS, sizeof(g_pPresentHDRPS));
		//DXGI_FORMAT SwapChainFormats[2] = { DXGI_FORMAT_R10G10B10A2_UNORM, DXGI_FORMAT_R10G10B10A2_UNORM };
		//PresentHDRPS.SetRenderTargetFormats(2, SwapChainFormats, DXGI_FORMAT_UNKNOWN);
		//PresentHDRPS.Finalize();

	}

	void DirectX12Graphics::PreparePresentLDR()
	{
		GraphicsContext& Context = GraphicsContext::Begin(L"Present");
		
		Context.PIXBeginEvent(L"Prepare Present LDR");

		// We're going to be reading these buffers to write to the swap chain buffer(s)
		Context.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		Context.SetRootSignature(s_PresentRS);
		Context.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// Copy (and convert) the LDR buffer to the back buffer
		Context.SetDynamicDescriptor(0, 0, g_SceneColorBuffer.GetSRV());;

		Context.SetPipelineState(PresentSDRPS);
		Context.TransitionResource(g_DisplayBuffers[g_CurrentBuffer], D3D12_RESOURCE_STATE_RENDER_TARGET);
		Context.SetRenderTarget(g_DisplayBuffers[g_CurrentBuffer].GetRTV());
		Context.SetViewportAndScissor(0, 0, g_DisplayWidth, g_DisplayHeight);
		Context.Draw(3);
		// Close the final context to be executed before frame present.
		Context.PIXEndEvent();
		Context.Finish();
	}

	void DirectX12Graphics::PreparePresentHDR()
	{
		GraphicsContext& Context = GraphicsContext::Begin(L"Present");

		// We're going to be reading these buffers to write to the swap chain buffer(s)
		Context.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		Context.TransitionResource(g_DisplayBuffers[g_CurrentBuffer], D3D12_RESOURCE_STATE_RENDER_TARGET);

		Context.SetRootSignature(s_PresentRS);
		Context.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		Context.SetDynamicDescriptor(0, 0, g_SceneColorBuffer.GetSRV());

		D3D12_CPU_DESCRIPTOR_HANDLE RTVs[] =
		{
			g_DisplayBuffers[g_CurrentBuffer].GetRTV()
		};

		Context.SetPipelineState(PresentHDRPS);
		Context.SetRenderTargets(_countof(RTVs), RTVs);
		Context.SetViewportAndScissor(0, 0, g_NativeWidth, g_NativeHeight);
		struct Constants
		{
			float RcpDstWidth;
			float RcpDstHeight;
			float PaperWhite;
			float MaxBrightness;
			int32_t DebugMode;
		};
		Constants consts = { 1.0f / g_NativeWidth, 1.0f / g_NativeHeight,
			(float)g_HDRPaperWhite, (float)g_MaxDisplayLuminance, (int32_t)HDRDebugMode };
		Context.SetConstantArray(1, sizeof(Constants) / 4, (float*)&consts);
		Context.Draw(3);

		Context.TransitionResource(g_DisplayBuffers[g_CurrentBuffer], D3D12_RESOURCE_STATE_PRESENT);

		// Close the final context to be executed before frame present.
		Context.Finish();
	}


	void DirectX12Graphics::Initialize(void)
	{
		assert(s_SwapChain1 == nullptr);

#if _DEBUG
		Microsoft::WRL::ComPtr<ID3D12Debug> debugInterface;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface))))
			debugInterface->EnableDebugLayer();
		else
			EngineLog::AddLog(L"WARNING:  Unable to enable D3D12 debug validation layer");
#endif
		//D3D Device
		CreateD3DDevice();
		//Check UAV Format Support
		CheckUAVFormatSupport();
		//Check HDR Support
		CheckHDRSupport();

		
		for (uint32_t i = 0; i < SWAP_CHAIN_BUFFER_COUNT; ++i)
		{
			ComPtr<ID3D12Resource> DisplayPlane;
			ThrowIfFailed(s_SwapChain1->GetBuffer(i, IID_PPV_ARGS(&DisplayPlane)));
			g_DisplayBuffers[i].CreateFromSwapChain(L"Primary SwapChain Buffer", DisplayPlane.Detach());
		}

		// Common state 
		InitializeAllCommonState();
		//Initialize Present PSO
		InitializePersentPSO();

		GPUTimeManager::Initialize(4096);
		SetNativeResolution();

		//g_PreDisplayBuffer.Create(L"PreDisplay Buffer", g_DisplayWidth, g_DisplayHeight, 1, g_SwapChainFormat);

		//TemporalEffects::Initialize();
		//PostEffects::Initialize();
		//SSAO::Initialize();
		//ParticleEffects::Initialize(kMaxNativeWidth, kMaxNativeHeight);
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

		//g_PreDisplayBuffer.Create(L"PreDisplay Buffer", width, height, 1, g_SwapChainFormat);

		//Destroy old display buffers
		for (uint32_t i = 0; i < SWAP_CHAIN_BUFFER_COUNT; ++i)
			g_DisplayBuffers[i].Destroy();

		//Resize Swap Chain 
		ThrowIfFailed(s_SwapChain1->ResizeBuffers(SWAP_CHAIN_BUFFER_COUNT, width, height, g_SwapChainFormat, 0));

		for (uint32_t i = 0; i < SWAP_CHAIN_BUFFER_COUNT; ++i)
		{
			ComPtr<ID3D12Resource> DisplayBuffer;
			ThrowIfFailed(s_SwapChain1->GetBuffer(i, IID_PPV_ARGS(&DisplayBuffer)));
			g_DisplayBuffers[i].CreateFromSwapChain(L"Primary SwapChain Buffer", DisplayBuffer.Detach());
		}

		//reset current buffer index
		g_CurrentBuffer = 0;
		g_CommandManager.IdleGPU();

		ResizeDisplayDependentBuffers(width, height);
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

		for (UINT i = 0; i < SWAP_CHAIN_BUFFER_COUNT; ++i)
			g_DisplayBuffers[i].Destroy();

#if defined(_DEBUG)
		ID3D12DebugDevice* debugInterface;
		if (SUCCEEDED(g_Device->QueryInterface(&debugInterface)))
		{
			//debugInterface->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL);
			debugInterface->Release();
		}
#endif

		SAFE_RELEASE(g_Device);
	}

	void DirectX12Graphics::Present(void)
	{
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
		}
		else
		{
			//自由运行时，保留最近的总帧时间作为下一帧模拟的时间步长。 
			//这不是非常准确，但假设帧时间平滑变化，它应该足够接近。
			s_FrameTime = CurrentTick-s_FrameStartTick;
		}

		s_FrameStartTick = (int64_t)CurrentTick;

		++s_FrameIndex;
		//TemporalEffects::Update((uint32_t)s_FrameIndex);

		SetNativeResolution();
	}

	void DirectX12Graphics::PreparePresent(void)
	{
		if (g_bEnableHDROutput)
			PreparePresentHDR();
		else
			PreparePresentLDR();
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



