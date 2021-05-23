#include "L3DUtil.h"
#include "GPUTimeManager.h"
#include "GraphicsCore.h"
#include "CommandContext.h"
#include "CommandListManager.h"

using namespace  GlareEngine;

namespace
{
	ID3D12QueryHeap* sm_QueryHeap = nullptr;
	ID3D12Resource* sm_ReadBackBuffer = nullptr;
	uint64_t* sm_TimeStampBuffer = nullptr;
	uint64_t sm_Fence = 0;
	uint32_t sm_MaxNumTimers = 0;
	uint32_t sm_NumTimers = 1;
	uint64_t sm_ValidTimeStart = 0;
	uint64_t sm_ValidTimeEnd = 0;
	double sm_GpuTickDelta = 0.0;
}

void GPUTimeManager::Initialize(uint32_t MaxNumTimers)
{
	uint64_t GPUFrequency;
	Graphics::g_CommandManager.GetCommandQueue()->GetTimestampFrequency(&GPUFrequency);
	sm_GpuTickDelta = 1.0 / static_cast<double>(GPUFrequency);



}
