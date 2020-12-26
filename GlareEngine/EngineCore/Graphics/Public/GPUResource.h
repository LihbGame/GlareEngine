#pragma once
#include "L3DUtil.h"
namespace GlareEngine
{
	class GPUResource
	{
		friend class CommandContext;
		friend class GraphicsContext;
		friend class ComputeContext;

	public:
		GPUResource() :
			m_GPUVirtualAddress(D3D12_GPU_VIRTUAL_ADDRESS_NULL),
			m_UserAllocatedMemory(nullptr),
			m_UsageState(D3D12_RESOURCE_STATE_COMMON),
			m_TransitioningState((D3D12_RESOURCE_STATES)-1)
		{}

		GPUResource(ID3D12Resource* pResource, D3D12_RESOURCE_STATES CurrentState) :
			m_GPUVirtualAddress(D3D12_GPU_VIRTUAL_ADDRESS_NULL),
			m_UserAllocatedMemory(nullptr),
			m_pResource(pResource),
			m_UsageState(CurrentState),
			m_TransitioningState((D3D12_RESOURCE_STATES)-1)
		{
		}

		virtual void Destroy()
		{
			m_pResource = nullptr;
			m_GPUVirtualAddress = D3D12_GPU_VIRTUAL_ADDRESS_NULL;
			if (m_UserAllocatedMemory != nullptr)
			{
				VirtualFree(m_UserAllocatedMemory, 0, MEM_RELEASE);
				m_UserAllocatedMemory = nullptr;
			}
		}

		ID3D12Resource* operator->() { return m_pResource.Get(); }
		const ID3D12Resource* operator->() const { return m_pResource.Get(); }

		ID3D12Resource* GetResource() { return m_pResource.Get(); }
		const ID3D12Resource* GetResource() const { return m_pResource.Get(); }

		D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress() const { return m_GPUVirtualAddress; }

	protected:
		
		D3D12_GPU_VIRTUAL_ADDRESS m_GPUVirtualAddress;

		D3D12_RESOURCE_STATES m_UsageState;

		D3D12_RESOURCE_STATES m_TransitioningState;

		Microsoft::WRL::ComPtr<ID3D12Resource> m_pResource;

		//使用VirtualAlloc（）直接分配内存时，请在此处记录分配，以便可以释放它。 
		//GpuVirtualAddress可能会偏离实际分配开始位置。
		void* m_UserAllocatedMemory;
	};
}