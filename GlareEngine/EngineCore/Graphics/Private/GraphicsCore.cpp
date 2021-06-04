#include "GraphicsCore.h"
#include "SamplerManager.h"
#include "DescriptorHeap.h"
#include "CommandContext.h"
#include "CommandListManager.h"
#include "RootSignature.h"
#include "CommandSignature.h"
#include "EngineAdjust.h"

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

using namespace GlareEngine::Math;

namespace GlareEngine
{
	namespace GameCore
	{
		extern HWND g_hWnd;

		float s_FrameTime = 0.0f;
		uint64_t s_FrameIndex = 0;
		int64_t s_FrameStartTick = 0;
	}


	namespace DirectX12Graphics
	{
		void PreparePresentLDR();
		void PreparePresentHDR();
		void CompositeOverlays(GraphicsContext& Context);

		const uint32_t MaxNativeWidth = 3840;
		const uint32_t MaxNativeHeight = 2160;
		const uint32_t NumPredefinedResolutions = 6;

		const char* ResolutionLabels[] = { "1280x720", "1600x900", "1920x1080", "2560x1440", "3200x1800", "3840x2160" };
		//Use 1080P
		EnumVar TargetResolution("Graphics/Display/Native Resolution", E1080p, NumPredefinedResolutions, ResolutionLabels);

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
		uint32_t g_DisplayWidth = 1920;
		uint32_t g_DisplayHeight = 1080;
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

			DEBUGPRINT("Changing native resolution to %ux%u", NativeWidth, NativeHeight);

			g_NativeWidth = NativeWidth;
			g_NativeHeight = NativeHeight;

			g_CommandManager.IdleGPU();

			InitializeRenderingBuffers(NativeWidth, NativeHeight);
		}

	}



	void DirectX12Graphics::Initialize(void)
	{
		assert(s_SwapChain1 == nullptr, "Graphics has already been initialized");

	}

}



