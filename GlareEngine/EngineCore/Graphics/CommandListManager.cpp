#include "CommandListManager.h"
#include "Engine/EngineUtility.h"

namespace GlareEngine
{
	extern CommandListManager g_CommandManager;
}


GlareEngine::CommandQueue::CommandQueue(D3D12_COMMAND_LIST_TYPE Type)
:m_Type(Type),
m_CommandQueue(nullptr),
m_pFence(nullptr),
m_NextFenceValue((uint64_t)Type << 56 | 1),
m_LastCompletedFenceValue((uint64_t)Type << 56),
m_AllocatorPool(Type)
{
}

GlareEngine::CommandQueue::~CommandQueue()
{
	Shutdown();
}

void GlareEngine::CommandQueue::Create(ID3D12Device* pDevice)
{
	assert(pDevice != nullptr);
	assert(!IsReady());
	assert(m_AllocatorPool.Size() == 0);

	D3D12_COMMAND_QUEUE_DESC QueueDesc = {};
	QueueDesc.Type = m_Type;
	QueueDesc.NodeMask = 1;
	pDevice->CreateCommandQueue(&QueueDesc, IID_PPV_ARGS(&m_CommandQueue));
	m_CommandQueue->SetName(L"CommandListManager::m_CommandQueue");

	ThrowIfFailed(pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_pFence)));
	m_pFence->SetName(L"CommandListManager::m_pFence");
	m_pFence->Signal((uint64_t)m_Type << 56);

	m_FenceEventHandle = CreateEvent(nullptr, false, false, nullptr);
	assert(m_FenceEventHandle != NULL);

	m_AllocatorPool.Create(pDevice);

	assert(IsReady());
}

void GlareEngine::CommandQueue::Shutdown()
{
	if (m_CommandQueue == nullptr)
		return;

	m_AllocatorPool.Shutdown();

	CloseHandle(m_FenceEventHandle);

	m_pFence->Release();
	m_pFence = nullptr;

	m_CommandQueue->Release();
	m_CommandQueue = nullptr;
}

uint64_t GlareEngine::CommandQueue::IncrementFence(void)
{
	std::lock_guard<std::mutex> LockGuard(m_FenceMutex);
	m_CommandQueue->Signal(m_pFence, m_NextFenceValue);
	return m_NextFenceValue++;
}

bool GlareEngine::CommandQueue::IsFenceComplete(uint64_t FenceValue)
{
	//避免通过对最后看到的栅栏值进行测试来查询栅栏值.
	//max()是为了防止出现不可能的竞赛条件，导致最后完成的栅栏值发生退步。
	if (FenceValue > m_LastCompletedFenceValue)
		m_LastCompletedFenceValue = max(m_LastCompletedFenceValue, m_pFence->GetCompletedValue());

	return FenceValue <= m_LastCompletedFenceValue;
}

void GlareEngine::CommandQueue::StallForFence(uint64_t FenceValue)
{
	CommandQueue& Producer = g_CommandManager.GetQueue((D3D12_COMMAND_LIST_TYPE)(FenceValue >> 56));
	m_CommandQueue->Wait(Producer.m_pFence, FenceValue);
}

void GlareEngine::CommandQueue::StallForProducer(CommandQueue& Producer)
{
	assert(Producer.m_NextFenceValue > 0);
	m_CommandQueue->Wait(Producer.m_pFence, Producer.m_NextFenceValue - 1);
}

void GlareEngine::CommandQueue::WaitForFence(uint64_t FenceValue)
{
	if (IsFenceComplete(FenceValue))
		return;

	// TODO:  Think about how this might affect a multi-threaded situation.  Suppose thread A
	// wants to wait for fence 100, then thread B comes along and wants to wait for 99.  If
	// the fence can only have one event set on completion, then thread B has to wait for 
	// 100 before it knows 99 is ready.  Maybe insert sequential events?
	{
		std::lock_guard<std::mutex> LockGuard(m_EventMutex);

		m_pFence->SetEventOnCompletion(FenceValue, m_FenceEventHandle);
		WaitForSingleObject(m_FenceEventHandle, INFINITE);
		m_LastCompletedFenceValue = FenceValue;
	}
}

uint64_t GlareEngine::CommandQueue::ExecuteCommandList(ID3D12CommandList* List)
{
	std::lock_guard<std::mutex> LockGuard(m_FenceMutex);

	ThrowIfFailed(((ID3D12GraphicsCommandList*)List)->Close());

	// Kickoff the command list
	m_CommandQueue->ExecuteCommandLists(1, &List);

	// Signal the next fence value (with the GPU)
	m_CommandQueue->Signal(m_pFence, m_NextFenceValue);

	// And increment the fence value.  
	return m_NextFenceValue++;
}

ID3D12CommandAllocator* GlareEngine::CommandQueue::RequestAllocator(void)
{
	uint64_t CompletedFence = m_pFence->GetCompletedValue();
	return m_AllocatorPool.RequestAllocator(CompletedFence);
}

void GlareEngine::CommandQueue::DiscardAllocator(uint64_t FenceValueForReset, ID3D12CommandAllocator* Allocator)
{
	m_AllocatorPool.DiscardAllocator(FenceValueForReset, Allocator);
}


GlareEngine::CommandListManager::CommandListManager()
:m_Device(nullptr),
m_GraphicsQueue(D3D12_COMMAND_LIST_TYPE_DIRECT),
m_ComputeQueue(D3D12_COMMAND_LIST_TYPE_COMPUTE),
m_CopyQueue(D3D12_COMMAND_LIST_TYPE_COPY)
{
}

GlareEngine::CommandListManager::~CommandListManager()
{
	Shutdown();
}

void GlareEngine::CommandListManager::Create(ID3D12Device* pDevice)
{
	assert(pDevice != nullptr);

	m_Device = pDevice;

	m_GraphicsQueue.Create(pDevice);
	m_ComputeQueue.Create(pDevice);
	m_CopyQueue.Create(pDevice);
}

void GlareEngine::CommandListManager::Shutdown()
{
	m_GraphicsQueue.Shutdown();
	m_ComputeQueue.Shutdown();
	m_CopyQueue.Shutdown();
}

void GlareEngine::CommandListManager::CreateNewCommandList(D3D12_COMMAND_LIST_TYPE Type, ID3D12GraphicsCommandList** List, ID3D12CommandAllocator** Allocator)
{
	//Bundles are not yet supported
	assert(Type != D3D12_COMMAND_LIST_TYPE_BUNDLE);
	switch (Type)
	{
	case D3D12_COMMAND_LIST_TYPE_DIRECT: *Allocator = m_GraphicsQueue.RequestAllocator(); break;
	case D3D12_COMMAND_LIST_TYPE_BUNDLE: break;
	case D3D12_COMMAND_LIST_TYPE_COMPUTE: *Allocator = m_ComputeQueue.RequestAllocator(); break;
	case D3D12_COMMAND_LIST_TYPE_COPY: *Allocator = m_CopyQueue.RequestAllocator(); break;
	}

	ThrowIfFailed(m_Device->CreateCommandList(1, Type, *Allocator, nullptr, IID_PPV_ARGS(List)));
	(*List)->SetName(L"CommandList");
}


void GlareEngine::CommandListManager::WaitForFence(uint64_t FenceValue)
{
	CommandQueue& Producer = g_CommandManager.GetQueue((D3D12_COMMAND_LIST_TYPE)(FenceValue >> 56));
	Producer.WaitForFence(FenceValue);
}