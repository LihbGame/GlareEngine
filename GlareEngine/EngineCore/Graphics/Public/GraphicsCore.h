#pragma once
#include "L3DUtil.h"

namespace GlareEngine
{
	namespace DirectX12Graphics
	{
#ifndef RELEASE
		extern const GUID WKPDID_D3DDebugObjectName;
#endif
		using namespace Microsoft::WRL;

		void Initialize(void);
		void Resize(uint32_t width, uint32_t height);
		void Terminate(void);
		void Shutdown(void);
		void Present(void);

		extern uint32_t g_DisplayWidth;
		extern uint32_t g_DisplayHeight;

		// Returns the number of elapsed frames since application start
		uint64_t GetFrameCount(void);

		// The amount of time elapsed during the last completed frame.  The CPU and/or
		// GPU may be idle during parts of the frame.  The frame time measures the time
		// between calls to present each frame.
		float GetFrameTime(void);

		// The total number of frames per second
		float GetFrameRate(void);

		extern ID3D12Device* g_Device;
		extern CommandListManager g_CommandManager;
		extern ContextManager g_ContextManager;

		extern D3D_FEATURE_LEVEL g_D3DFeatureLevel;
		extern bool g_bTypedUAVLoadSupport_R11G11B10_FLOAT;
		extern bool g_bEnableHDROutput;
	}
}
