#include "DescriptorHeap.h"
#include "GraphicsCore.h"
#include "CommandListManager.h"

using namespace GlareEngine;

// DescriptorAllocator implementation
std::mutex DescriptorAllocator::sm_AllocationMutex;
std::vector<Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>> DescriptorAllocator::sm_DescriptorHeapPool;


D3D12_CPU_DESCRIPTOR_HANDLE GlareEngine::DescriptorAllocator::Allocate(uint32_t Count)
{
	if (m_CurrentHeap == nullptr || m_RemainingFreeHandles < Count)
	{
		m_CurrentHeap = RequestNewHeap(m_Type);
		m_CurrentHandle = m_CurrentHeap->GetCPUDescriptorHandleForHeapStart();
		m_RemainingFreeHandles = sm_NumDescriptorsPerHeap;

		if (m_DescriptorSize == 0)
			m_DescriptorSize = Graphics::g_Device->GetDescriptorHandleIncrementSize(m_Type);
	}

	D3D12_CPU_DESCRIPTOR_HANDLE ret = m_CurrentHandle;
	m_CurrentHandle.ptr += Count * m_DescriptorSize;
	m_RemainingFreeHandles -= Count;
	return ret;
}

void GlareEngine::DescriptorAllocator::DestroyAll(void)
{
	sm_DescriptorHeapPool.clear();
}

ID3D12DescriptorHeap* GlareEngine::DescriptorAllocator::RequestNewHeap(D3D12_DESCRIPTOR_HEAP_TYPE Type)
{
	std::lock_guard<std::mutex> LockGuard(sm_AllocationMutex);

	D3D12_DESCRIPTOR_HEAP_DESC Desc;
	Desc.Type = Type;
	Desc.NumDescriptors = sm_NumDescriptorsPerHeap;
	Desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	Desc.NodeMask = 1;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> pHeap;
	assert(Graphics::g_Device->CreateDescriptorHeap(&Desc, IID_PPV_ARGS(&pHeap)));
	sm_DescriptorHeapPool.emplace_back(pHeap);
	return pHeap.Get();
}
