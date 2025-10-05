#include "Engine/EngineUtility.h"
#include "GPUTimeManager.h"
#include "GraphicsCore.h"
#include "CommandContext.h"
#include "CommandListManager.h"
#include "Engine/Scene.h"
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
	g_CommandManager.GetCommandQueue()->GetTimestampFrequency(&GPUFrequency);
	sm_GpuTickDelta = 1.0 / static_cast<double>(GPUFrequency);

	D3D12_HEAP_PROPERTIES HeapProps;
	HeapProps.Type = D3D12_HEAP_TYPE_READBACK;
	HeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	HeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	HeapProps.CreationNodeMask = 1;
	HeapProps.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC BufferDesc;
	BufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	BufferDesc.Alignment = 0;
	BufferDesc.Width = sizeof(uint64_t) * MaxNumTimers * 2;
	BufferDesc.Height = 1;
	BufferDesc.DepthOrArraySize = 1;
	BufferDesc.MipLevels = 1;
	BufferDesc.Format = DXGI_FORMAT_UNKNOWN;
	BufferDesc.SampleDesc.Count = 1;
	BufferDesc.SampleDesc.Quality = 0;
	BufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	BufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	ThrowIfFailed(g_Device->CreateCommittedResource(&HeapProps, D3D12_HEAP_FLAG_NONE, &BufferDesc,
		D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&sm_ReadBackBuffer)));
	sm_ReadBackBuffer->SetName(L"GpuTimeStamp Buffer");

	D3D12_QUERY_HEAP_DESC QueryHeapDesc;
 	QueryHeapDesc.Count = MaxNumTimers * 2;
	QueryHeapDesc.NodeMask = 1;
	QueryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
	ThrowIfFailed(g_Device->CreateQueryHeap(&QueryHeapDesc, IID_PPV_ARGS(&sm_QueryHeap)));
	sm_QueryHeap->SetName(L"GpuTimeStamp QueryHeap");

	sm_MaxNumTimers = (uint32_t)MaxNumTimers;
}

void GPUTimeManager::Shutdown()
{
	if (sm_ReadBackBuffer != nullptr)
		sm_ReadBackBuffer->Release();

	if (sm_QueryHeap != nullptr)
		sm_QueryHeap->Release();
}

uint32_t GPUTimeManager::NewTimer(void)
{
	return sm_NumTimers++;
}

void GPUTimeManager::StartTimer(CommandContext& Context, uint32_t TimerIdx)
{
	Context.InsertTimeStamp(sm_QueryHeap, TimerIdx * 2);
}

void GPUTimeManager::StopTimer(CommandContext& Context, uint32_t TimerIdx)
{
	Context.InsertTimeStamp(sm_QueryHeap, TimerIdx * 2 + 1);
}

void GPUTimeManager::BeginReadBack(void)
{
	g_CommandManager.WaitForFence(sm_Fence);

	D3D12_RANGE Range;
	Range.Begin = 0;
	Range.End = (sm_NumTimers * 2) * sizeof(uint64_t);
	ThrowIfFailed(sm_ReadBackBuffer->Map(0, &Range, reinterpret_cast<void**>(&sm_TimeStampBuffer)))

	//ThrowIfFailed();
	if (sm_TimeStampBuffer)
	{
		sm_ValidTimeStart = sm_TimeStampBuffer[0];
		sm_ValidTimeEnd = sm_TimeStampBuffer[1];
	}
	//在第一帧中，时间戳查询堆中具有随机值，我们可以避免错误启动。
	if (sm_ValidTimeEnd < sm_ValidTimeStart)
	{
		sm_ValidTimeStart = 0ull;
		sm_ValidTimeEnd = 0ull;
	}
}

void GPUTimeManager::EndReadBack(void)
{
	//用空范围取消映射以指示没有任何内容被CPU写入 
	D3D12_RANGE EmptyRange = {};
	sm_ReadBackBuffer->Unmap(0, &EmptyRange);
	sm_TimeStampBuffer = nullptr;

	CommandContext& Context = CommandContext::Begin();

	static bool needReset = true;
	if (needReset)
	{
		for (UINT i = 0; i < sm_NumTimers * 2; i++)
		{
			Context.InsertTimeStamp(sm_QueryHeap, i);
		}
		needReset = false;
	}

	Context.InsertTimeStamp(sm_QueryHeap, 1);
	Context.InsertTimeStamp(sm_QueryHeap, 3); // for sm_RootScope
	Context.ResolveTimeStamps(sm_ReadBackBuffer, sm_QueryHeap, (sm_NumTimers-1) * 2);
	Context.Flush(true);
	for (UINT i = 0; i < sm_NumTimers * 2; i++)
	{
		Context.InsertTimeStamp(sm_QueryHeap, i);
	}
	Context.InsertTimeStamp(sm_QueryHeap, 2); // for sm_RootScope
	Context.InsertTimeStamp(sm_QueryHeap, 0);
	sm_Fence = Context.Finish();
}

float GPUTimeManager::GetTime(uint32_t TimerIdx)
{
	//Time stamp readback buffer is not mapped
	assert(sm_TimeStampBuffer != nullptr);
	//Invalid GPU timer index
	assert(TimerIdx < sm_NumTimers);

	uint64_t TimeStamp1 = sm_TimeStampBuffer[TimerIdx * 2];
	uint64_t TimeStamp2 = sm_TimeStampBuffer[TimerIdx * 2 + 1];

	if (TimeStamp1 < sm_ValidTimeStart || TimeStamp2 > sm_ValidTimeEnd || TimeStamp2 <= TimeStamp1)
		return 0.0f;

	return static_cast<float>(sm_GpuTickDelta * (TimeStamp2 - TimeStamp1));
}

int32_t GPUTimeManager::GetTimeCount()
{
	return sm_NumTimers;
}

ID3D12QueryHeap* GPUTimeManager::GetTimeHeap()
{
	return sm_QueryHeap;
}
