#include "GraphicsCore.h"
#include "SamplerManager.h"
#include "DescriptorHeap.h"
#include "CommandContext.h"
#include "CommandListManager.h"
#include "RootSignature.h"
#include "CommandSignature.h"



#define SWAP_CHAIN_BUFFER_COUNT 3

DXGI_FORMAT SwapChainFormat = DXGI_FORMAT_R10G10B10A2_UNORM;

using namespace GlareEngine::Math;

namespace GameCore
{
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
	extern HWND g_hWnd;
#else
	extern Platform::Agile<Windows::UI::Core::CoreWindow>  g_window;
#endif

	float s_FrameTime = 0.0f;
	uint64_t s_FrameIndex = 0;
	int64_t s_FrameStartTick = 0;
}


void GlareEngine::DirectX12Graphics::Initialize(void)
{
	assert(s_SwapChain1 == nullptr, "Graphics has already been initialized");

}
