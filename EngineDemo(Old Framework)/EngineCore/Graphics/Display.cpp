#include "Display.h"
#include "EngineUtility.h"
#include "GraphicsCore.h"
#include "BufferManager.h"
#include "ColorBuffer.h"
#include "CommandContext.h"
#include "RootSignature.h"
#include "EngineLog.h"
#include "ScreenGrab.h"
#include "SamplerManager.h"
#include "CommandListManager.h"
#include "CommandSignature.h"
#include "EngineAdjust.h"
#include "GPUTimeManager.h"
#include "TextureManager.h"

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

	BoolVar s_LimitTo30Hz(L"Timing/Limit To 30Hz", false);

	namespace
	{
		float s_FrameTime = 0.0f;
		uint64_t s_FrameIndex = 0;
		int64_t s_FrameStartTick = 0;
	}

	namespace Display
	{

		void CheckHDRSupport();

		void PreparePresentLDR();
		void PreparePresentHDR();

		const uint32_t MaxNativeWidth = 3840;
		const uint32_t MaxNativeHeight = 2160;
		const uint32_t NumPredefinedResolutions = 6;

		enum eResolution { E720p, E900p, E1080p, E1440p, E1800p, E2160p };

		const wchar_t* ResolutionLabels[] = { L"1280x720", L"1600x900", L"1920x1080", L"2560x1440", L"3200x1800", L"3840x2160" };
		//Use 900P
		EnumVar TargetResolution(L"Graphics/Display/Native Resolution", E900p, NumPredefinedResolutions, ResolutionLabels);

		BoolVar s_EnableVSync(L"Timing/VSync", true);

		bool g_bEnableHDROutput = false;

		//Color range
		NumVar g_HDRPaperWhite(L"Graphics/Display/Paper White (nits)", 200.0f, 100.0f, 500.0f, 50.0f);
		NumVar g_MaxDisplayLuminance(L"Graphics/Display/Peak Brightness (nits)", 1000.0f, 500.0f, 10000.0f, 100.0f);

		const wchar_t* HDRModeLabels[] = { L"HDR", L"SDR", L"Side-by-Side" };
		EnumVar HDRDebugMode(L"Graphics/Display/HDR Debug Mode", 0, 3, HDRModeLabels);

		uint32_t g_NativeWidth = 0;
		uint32_t g_NativeHeight = 0;
		uint32_t g_DisplayWidth = 1600;
		uint32_t g_DisplayHeight = 900;
		DirectX12Graphics::ColorBuffer g_PreDisplayBuffer;


		void ResolutionToUINT(eResolution res, uint32_t& NativeWidth, uint32_t& NativeHeight)
		{
			switch (res)
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
		}

		void SetNativeResolution(void)
		{
			uint32_t NativeWidth, NativeHeight;

			ResolutionToUINT(eResolution((int)TargetResolution), NativeWidth, NativeHeight);

			if (g_NativeWidth == NativeWidth && g_NativeHeight == NativeHeight)
				return;
			EngineLog::AddLog(L"Changing native resolution to %ux%u", NativeWidth, NativeHeight);

			g_NativeWidth = NativeWidth;
			g_NativeHeight = NativeHeight;

			DirectX12Graphics::g_CommandManager.IdleGPU();

			//Resize Render Buffer
			DirectX12Graphics::InitializeRenderingBuffers(NativeWidth, NativeHeight);
		}

		//SwapChainFormat
		DXGI_FORMAT g_SwapChainFormat = DXGI_FORMAT_R10G10B10A2_UNORM;

		//Display Buffer(三缓存)
		DirectX12Graphics::ColorBuffer g_DisplayBuffers[SWAP_CHAIN_BUFFER_COUNT];
		UINT g_CurrentBuffer = 0;

		//Swap Chain
		IDXGISwapChain1* s_SwapChain1 = nullptr;

		//Descriptor Allocator
		DirectX12Graphics::DescriptorAllocator g_DescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES] =
		{
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
			D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
			D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
			D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
		};

		//SRV Descriptors Manager ,return Descriptor index
		std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> g_TextureSRV;
		std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> g_CubeSRV;

		DirectX12Graphics::RootSignature s_PresentRS;
		DirectX12Graphics::GraphicsPSO s_PresentPSO;
		DirectX12Graphics::GraphicsPSO PresentSDRPS;
		DirectX12Graphics::GraphicsPSO PresentHDRPS;
	}


	DirectX12Graphics::ColorBuffer& Display::GetCurrentBuffer()
	{
		return g_DisplayBuffers[g_CurrentBuffer];
	}
	

	void Display::CheckHDRSupport()
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

	void Display::PreparePresent()
	{
		if (g_bEnableHDROutput)
			PreparePresentHDR();
		else
			PreparePresentLDR();

	}

	void Display::PreparePresentLDR()
	{
		DirectX12Graphics::GraphicsContext& Context = DirectX12Graphics::GraphicsContext::Begin(L"Present");

		// We're going to be reading these buffers to write to the swap chain buffer(s)
		Context.TransitionResource(DirectX12Graphics::g_SceneColorBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		Context.SetRootSignature(s_PresentRS);
		Context.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// Copy (and convert) the LDR buffer to the back buffer
		Context.SetDynamicDescriptor(0, 0, DirectX12Graphics::g_SceneColorBuffer.GetSRV());;

		Context.SetPipelineState(PresentSDRPS);
		Context.TransitionResource(g_DisplayBuffers[g_CurrentBuffer], D3D12_RESOURCE_STATE_RENDER_TARGET);
		Context.SetRenderTarget(g_DisplayBuffers[g_CurrentBuffer].GetRTV());
		Context.SetViewportAndScissor(0, 0, g_DisplayWidth, g_DisplayHeight);
		Context.Draw(3);
		// Close the final context to be executed before frame present.
		Context.Finish();
	}

	void Display::PreparePresentHDR()
	{
		DirectX12Graphics::GraphicsContext& Context = DirectX12Graphics::GraphicsContext::Begin(L"Present");

		// We're going to be reading these buffers to write to the swap chain buffer(s)
		Context.TransitionResource(DirectX12Graphics::g_SceneColorBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		Context.TransitionResource(g_DisplayBuffers[g_CurrentBuffer], D3D12_RESOURCE_STATE_RENDER_TARGET);

		Context.SetRootSignature(s_PresentRS);
		Context.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		Context.SetDynamicDescriptor(0, 0, DirectX12Graphics::g_SceneColorBuffer.GetSRV());

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
		// Close the final context to be executed before frame present.
		Context.Finish();
	}

	uint64_t Display::GetFrameCount(void)
	{
		return s_FrameIndex;
	}

	float Display::GetFrameTime(void)
	{
		return s_FrameTime;
	}

	float Display::GetFrameRate(void)
	{
		return  s_FrameTime == 0.0f ? 0.0f : 1.0f / s_FrameTime;
	}

	void Display::Initialize(HWND hWnd)
	{
		assert(s_SwapChain1 == nullptr);
		
		Microsoft::WRL::ComPtr<IDXGIFactory4> dxgiFactory;
		SUCCEEDED(CreateDXGIFactory2(0, IID_PPV_ARGS(&dxgiFactory)));

		DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
		swapChainDesc.Width = g_DisplayWidth;
		swapChainDesc.Height = g_DisplayHeight;
		swapChainDesc.Format = g_SwapChainFormat;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.BufferCount = SWAP_CHAIN_BUFFER_COUNT;
		swapChainDesc.SampleDesc.Count = 1;
		swapChainDesc.SampleDesc.Quality = 0;
		swapChainDesc.Scaling = DXGI_SCALING_NONE;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
		swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

		DXGI_SWAP_CHAIN_FULLSCREEN_DESC fsSwapChainDesc = {};
		fsSwapChainDesc.Windowed = TRUE;

		SUCCEEDED(dxgiFactory->CreateSwapChainForHwnd(
			DirectX12Graphics::g_CommandManager.GetCommandQueue(),
			hWnd,
			&swapChainDesc,
			&fsSwapChainDesc,
			nullptr,
			&s_SwapChain1));


		CheckHDRSupport();

		for (uint32_t i = 0; i < SWAP_CHAIN_BUFFER_COUNT; ++i)
		{
			ComPtr<ID3D12Resource> DisplayPlane;
			SUCCEEDED(s_SwapChain1->GetBuffer(i, IID_PPV_ARGS(&DisplayPlane)));
			g_DisplayBuffers[i].CreateFromSwapChain(L"Primary SwapChain Buffer", DisplayPlane.Detach());
		}

		SetNativeResolution();
	}

	void Display::Shutdown(void)
	{
		s_SwapChain1->SetFullscreenState(FALSE, nullptr);
		s_SwapChain1->Release();

		for (UINT i = 0; i < SWAP_CHAIN_BUFFER_COUNT; ++i)
			g_DisplayBuffers[i].Destroy();

		g_PreDisplayBuffer.Destroy();
	}


	void Display::Present(void)
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
			s_FrameTime = CurrentTick - s_FrameStartTick;
		}

		s_FrameStartTick = (int64_t)CurrentTick;

		++s_FrameIndex;

		//TemporalEffects::Update((uint32_t)s_FrameIndex);

		SetNativeResolution();
	}

	void Display::Resize(uint32_t width, uint32_t height)
	{
		assert(s_SwapChain1 != nullptr);

		// Check for invalid window dimensions
		if (width == 0 || height == 0)
			return;

		// Check for an unneeded resize
		if (width == g_DisplayWidth && height == g_DisplayHeight)
			return;

		DirectX12Graphics::g_CommandManager.IdleGPU();

		g_DisplayWidth = width;
		g_DisplayHeight = height;

		EngineLog::AddLog(L"Changing display resolution to %ux%u", g_DisplayWidth, g_DisplayHeight);

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
		DirectX12Graphics::g_CommandManager.IdleGPU();

		DirectX12Graphics::ResizeDisplayDependentBuffers(width, height);
	}
}