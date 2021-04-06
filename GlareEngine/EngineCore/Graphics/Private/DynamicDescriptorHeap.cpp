#include "DynamicDescriptorHeap.h"
#include "CommandContext.h"
#include "GraphicsCore.h"
#include "CommandListManager.h"
#include "RootSignature.h"

namespace GlareEngine
{
	namespace DirectX12Graphics
	{
		std::mutex DynamicDescriptorHeap::sm_Mutex;
		std::vector<Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>> DynamicDescriptorHeap::sm_DescriptorHeapPool[2];
		std::queue<std::pair<uint64_t, ID3D12DescriptorHeap*>> DynamicDescriptorHeap::sm_RetiredDescriptorHeaps[2];
		std::queue<ID3D12DescriptorHeap*> DynamicDescriptorHeap::sm_AvailableDescriptorHeaps[2];

		DynamicDescriptorHeap::DynamicDescriptorHeap(CommandContext& OwningContext, D3D12_DESCRIPTOR_HEAP_TYPE HeapType)
			: m_OwningContext(OwningContext), m_DescriptorType(HeapType)
		{
			m_CurrentHeapPtr = nullptr;
			m_CurrentOffset = 0;
			m_DescriptorSize = g_Device->GetDescriptorHandleIncrementSize(HeapType);
		}

		DynamicDescriptorHeap::~DynamicDescriptorHeap()
		{
		}

		void DynamicDescriptorHeap::CleanupUsedHeaps(uint64_t fenceValue)
		{
		}

		D3D12_GPU_DESCRIPTOR_HANDLE DynamicDescriptorHeap::UploadDirect(D3D12_CPU_DESCRIPTOR_HANDLE Handles)
		{
			return D3D12_GPU_DESCRIPTOR_HANDLE();
		}

		ID3D12DescriptorHeap* DynamicDescriptorHeap::RequestDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE HeapType)
		{
			return nullptr;
		}

		void DynamicDescriptorHeap::DiscardDescriptorHeaps(D3D12_DESCRIPTOR_HEAP_TYPE HeapType, uint64_t FenceValueForReset, const std::vector<ID3D12DescriptorHeap*>& UsedHeaps)
		{
		}

		void DynamicDescriptorHeap::RetireCurrentHeap(void)
		{
		}

		void DynamicDescriptorHeap::RetireUsedHeaps(uint64_t fenceValue)
		{
		}

		ID3D12DescriptorHeap* DynamicDescriptorHeap::GetHeapPointer()
		{
			return nullptr;
		}

		void DynamicDescriptorHeap::CopyAndBindStagedTables(DescriptorHandleCache& HandleCache, ID3D12GraphicsCommandList* CmdList, void(__stdcall ID3D12GraphicsCommandList::* SetFunc)(UINT, D3D12_GPU_DESCRIPTOR_HANDLE))
		{
		}

		void DynamicDescriptorHeap::UnbindAllValid(void)
		{
		}

		uint32_t DynamicDescriptorHeap::DescriptorHandleCache::ComputeStagedSize()
		{
			return uint32_t();
		}

		void DynamicDescriptorHeap::DescriptorHandleCache::CopyAndBindStaleTables(D3D12_DESCRIPTOR_HEAP_TYPE Type, uint32_t DescriptorSize, DescriptorHandle DestHandleStart, ID3D12GraphicsCommandList* CmdList, void(__stdcall ID3D12GraphicsCommandList::* SetFunc)(UINT, D3D12_GPU_DESCRIPTOR_HANDLE))
		{
		}

		void DynamicDescriptorHeap::DescriptorHandleCache::UnbindAllValid()
		{
		}

		void DynamicDescriptorHeap::DescriptorHandleCache::StageDescriptorHandles(UINT RootIndex, UINT Offset, UINT NumHandles, const D3D12_CPU_DESCRIPTOR_HANDLE Handles[])
		{
		}

		void DynamicDescriptorHeap::DescriptorHandleCache::ParseRootSignature(D3D12_DESCRIPTOR_HEAP_TYPE Type, const RootSignature& RootSig)
		{
		}
	}
}
