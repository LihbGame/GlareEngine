#include "DescriptorHeap.h"
#include "GraphicsCore.h"
#include "CommandListManager.h"

namespace GlareEngine
{
	namespace DirectX12Graphics
	{
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
			ThrowIfFailed(DirectX12Graphics::g_Device->CreateDescriptorHeap(&Desc, IID_PPV_ARGS(&pHeap)));
			sm_DescriptorHeapPool.emplace_back(pHeap);
			return pHeap.Get();
		}


		//
		// DescriptorHeap
		//
		void DescriptorHeap::Create(const std::wstring& Name, D3D12_DESCRIPTOR_HEAP_TYPE Type, uint32_t MaxCount)
		{
			m_HeapDesc.Type = Type;
			m_HeapDesc.NumDescriptors = MaxCount;
			m_HeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			m_HeapDesc.NodeMask = 1;

			ThrowIfFailed(g_Device->CreateDescriptorHeap(&m_HeapDesc, IID_PPV_ARGS(m_Heap.ReleaseAndGetAddressOf())));

#ifdef RELEASE
			(void)Name;
#else
			m_Heap->SetName(Name.c_str());
#endif

			m_DescriptorSize = g_Device->GetDescriptorHandleIncrementSize(m_HeapDesc.Type);
			m_NumFreeDescriptors = m_HeapDesc.NumDescriptors;
			m_FirstHandle = DescriptorHandle(
				m_Heap->GetCPUDescriptorHandleForHeapStart(),
				m_Heap->GetGPUDescriptorHandleForHeapStart());
			m_NextFreeHandle = m_FirstHandle;
		}

		DescriptorHandle DescriptorHeap::Alloc(uint32_t Count)
		{
			//IF Descriptor Heap out of space.Increase heap size.
			assert(HasAvailableSpace(Count));
			DescriptorHandle retHandle = m_NextFreeHandle;
			m_NextFreeHandle += Count * m_DescriptorSize;
			m_NumFreeDescriptors -= Count;
			return retHandle;
		}

		bool DescriptorHeap::ValidateHandle(const DescriptorHandle& DHandle) const
		{
			if (DHandle.GetCPUPtr() < m_FirstHandle.GetCPUPtr() ||
				DHandle.GetCPUPtr() >= m_FirstHandle.GetCPUPtr() + m_HeapDesc.NumDescriptors * m_DescriptorSize)
				return false;

			if (DHandle.GetGPUPtr() - m_FirstHandle.GetGPUPtr() !=
				DHandle.GetCPUPtr() - m_FirstHandle.GetCPUPtr())
				return false;

			return true;
		}
	}
}
