#include "Engine/EngineUtility.h"
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
    // Number of timers covered by the warmup at frame start.
    // Timers created after the warmup need placeholder EndQuery in the resolve CL.
    uint32_t sm_WarmupCount = 1;
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

    // Write initial timestamps for indices 0 and 1 so the first resolve has valid queries
    CommandContext& InitContext = CommandContext::Begin();
    InitContext.InsertTimeStamp(sm_QueryHeap, 0);
    InitContext.InsertTimeStamp(sm_QueryHeap, 1);
    InitContext.Finish();
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
    ThrowIfFailed(sm_ReadBackBuffer->Map(0, &Range, reinterpret_cast<void**>(&sm_TimeStampBuffer)));

    sm_ValidTimeStart = sm_TimeStampBuffer[0];
    sm_ValidTimeEnd = sm_TimeStampBuffer[1];

    if (sm_ValidTimeEnd < sm_ValidTimeStart)
    {
        sm_ValidTimeStart = 0ull;
        sm_ValidTimeEnd = 0ull;
    }
}

void GPUTimeManager::EndReadBack(void)
{
    D3D12_RANGE EmptyRange = {};
    sm_ReadBackBuffer->Unmap(0, &EmptyRange);
    sm_TimeStampBuffer = nullptr;

    // Record how many timers the warmup covers.
    // Timers created after this point (worker threads, first-time profiling blocks)
    // will be handled in Resolve().
    sm_WarmupCount = sm_NumTimers;

    // Warmup: write EndQuery for all currently allocated timer indices.
    // GPU timeline: warmup CL -> rendering CLs (overwrite with actual data) -> resolve CL.
    if (sm_NumTimers > 1)
    {
        CommandContext& Warmup = CommandContext::Begin();
        for (uint32_t i = 1; i < sm_NumTimers; ++i)
        {
            Warmup.InsertTimeStamp(sm_QueryHeap, i * 2);
            Warmup.InsertTimeStamp(sm_QueryHeap, i * 2 + 1);
        }
        Warmup.Finish();
    }
}

void GPUTimeManager::Resolve(void)
{
    CommandContext& Context = CommandContext::Begin();

    // Write placeholder EndQuery for any timers created AFTER the warmup.
    // These are new timers (from worker threads or first-time profiling blocks)
    // whose command lists may not have been submitted to the GPU yet.
    for (uint32_t i = sm_WarmupCount; i < sm_NumTimers; ++i)
    {
        Context.InsertTimeStamp(sm_QueryHeap, i * 2);
        Context.InsertTimeStamp(sm_QueryHeap, i * 2 + 1);
    }

    // Write frame-end timestamp at index 1
    Context.InsertTimeStamp(sm_QueryHeap, 1);
    Context.ResolveTimeStamps(sm_ReadBackBuffer, sm_QueryHeap, sm_NumTimers * 2);
    // Write frame-start timestamp at index 0 (for next frame)
    Context.InsertTimeStamp(sm_QueryHeap, 0);
    sm_Fence = Context.Finish();
}

float GPUTimeManager::GetTime(uint32_t TimerIdx)
{
    assert(sm_TimeStampBuffer != nullptr);
    assert(TimerIdx < sm_NumTimers);

    uint64_t TimeStamp1 = sm_TimeStampBuffer[TimerIdx * 2];
    uint64_t TimeStamp2 = sm_TimeStampBuffer[TimerIdx * 2 + 1];

    if (TimeStamp1 < sm_ValidTimeStart || TimeStamp2 > sm_ValidTimeEnd || TimeStamp2 <= TimeStamp1)
        return 0.0f;

    return static_cast<float>(sm_GpuTickDelta * (TimeStamp2 - TimeStamp1));
}
