#pragma once
#include "Engine/EngineUtility.h"
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
			m_UsageState(D3D12_RESOURCE_STATE_COMMON),
			m_TransitioningState((D3D12_RESOURCE_STATES)-1)
		{}

		GPUResource(ID3D12Resource* pResource, D3D12_RESOURCE_STATES CurrentState) :
			m_GPUVirtualAddress(D3D12_GPU_VIRTUAL_ADDRESS_NULL),
			m_pResource(pResource),
			m_UsageState(CurrentState),
			m_TransitioningState((D3D12_RESOURCE_STATES)-1)
		{
		}

		~GPUResource() { Destroy(); }

		virtual void Destroy()
		{
			if (m_pResource != nullptr)
			{
				m_pResource = nullptr;
				m_GPUVirtualAddress = D3D12_GPU_VIRTUAL_ADDRESS_NULL;
				++m_VersionID;
			}
		}

		ID3D12Resource* operator->() { return m_pResource.Get(); }
		const ID3D12Resource* operator->() const { return m_pResource.Get(); }

		ID3D12Resource* GetResource() { return m_pResource.Get(); }
		const ID3D12Resource* GetResource() const { return m_pResource.Get(); }

		D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress() const { return m_GPUVirtualAddress; }

		ID3D12Resource** GetAddressOf() { return m_pResource.GetAddressOf(); }

		uint32_t GetVersionID() const { return m_VersionID; }
	protected:
		D3D12_GPU_VIRTUAL_ADDRESS m_GPUVirtualAddress;

		D3D12_RESOURCE_STATES m_UsageState;

		D3D12_RESOURCE_STATES m_TransitioningState;

		Microsoft::WRL::ComPtr<ID3D12Resource> m_pResource;

		// Used to identify when a resource changes so descriptors can be copied etc.
		uint32_t m_VersionID = 0;
	};
}