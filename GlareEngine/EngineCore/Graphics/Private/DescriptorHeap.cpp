#include "DescriptorHeap.h"
#include "GraphicsCore.h"
#include "CommandListManager.h"

using namespace GlareEngine::DirectX12Graphics;

// DescriptorAllocator implementation
std::mutex DescriptorAllocator::sm_AllocationMutex;
std::vector<Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>> DescriptorAllocator::sm_DescriptorHeapPool;


D3D12_CPU_DESCRIPTOR_HANDLE DescriptorAllocator::Allocate(uint32_t Count)
{
	if (m_CurrentHeap == nullptr || m_RemainingFreeHandles < Count)
	{
		m_CurrentHeap = RequestNewHeap(m_Type);
		m_CurrentHandle = m_CurrentHeap->GetCPUDescriptorHandleForHeapStart();
		m_RemainingFreeHandles = sm_NumDescriptorsPerHeap;

		if (m_DescriptorSize == 0)
			m_DescriptorSize = DirectX12Graphics::g_Device->GetDescriptorHandleIncrementSize(m_Type);
	}

	D3D12_CPU_DESCRIPTOR_HANDLE ret = m_CurrentHandle;
	m_CurrentHandle.ptr += Count * m_DescriptorSize;
	m_RemainingFreeHandles -= Count;
	return ret;
}

void DescriptorAllocator::DestroyAll(void)
{
	sm_DescriptorHeapPool.clear();
}

ID3D12DescriptorHeap* DescriptorAllocator::RequestNewHeap(D3D12_DESCRIPTOR_HEAP_TYPE Type)
{
	std::lock_guard<std::mutex> LockGuard(sm_AllocationMutex);

	D3D12_DESCRIPTOR_HEAP_DESC Desc;
	Desc.Type = Type;
	Desc.NumDescriptors = sm_NumDescriptorsPerHeap;
	Desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	Desc.NodeMask = 1;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> pHeap;
	assert(DirectX12Graphics::g_Device->CreateDescriptorHeap(&Desc, IID_PPV_ARGS(&pHeap)));
	sm_DescriptorHeapPool.emplace_back(pHeap);
	return pHeap.Get();
}

void UserDescriptorHeap::Create(const std::wstring& DebugHeapName)
{
	assert(DirectX12Graphics::g_Device->CreateDescriptorHeap(&m_HeapDesc, IID_PPV_ARGS(m_Heap.ReleaseAndGetAddressOf())));
#ifdef RELEASE
	(void)DebugHeapName;
#else
	m_Heap->SetName(DebugHeapName.c_str());
#endif

	m_DescriptorSize = DirectX12Graphics::g_Device->GetDescriptorHandleIncrementSize(m_HeapDesc.Type);
	m_NumFreeDescriptors = m_HeapDesc.NumDescriptors;
	m_FirstHandle = DescriptorHandle(m_Heap->GetCPUDescriptorHandleForHeapStart(), m_Heap->GetGPUDescriptorHandleForHeapStart());
	m_NextFreeHandle = m_FirstHandle;
}

DescriptorHandle UserDescriptorHeap::Alloc(uint32_t Count)
{
	assert(HasAvailableSpace(Count), "Descriptor Heap out of space.  Increase heap size.");
	DescriptorHandle ret = m_NextFreeHandle;
	m_NextFreeHandle += Count * m_DescriptorSize;
	return ret;
}

bool UserDescriptorHeap::ValidateHandle(const DescriptorHandle& DHandle) const
{
	if (DHandle.GetCPUHandle().ptr < m_FirstHandle.GetCPUHandle().ptr ||
		DHandle.GetCPUHandle().ptr >= m_FirstHandle.GetCPUHandle().ptr + m_HeapDesc.NumDescriptors * m_DescriptorSize)
		return false;

	if (DHandle.GetGPUHandle().ptr - m_FirstHandle.GetGPUHandle().ptr !=
		DHandle.GetCPUHandle().ptr - m_FirstHandle.GetCPUHandle().ptr)
		return false;

	return true;
}
