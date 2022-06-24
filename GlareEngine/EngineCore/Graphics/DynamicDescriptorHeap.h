#pragma once

#include "DescriptorHeap.h"
#include "RootSignature.h"
#include <vector>
#include <queue>


namespace GlareEngine
{
	extern ID3D12Device* g_Device;

	class CommandContext;
	//这个类是一个动态生成描述符表的线性分配系统。 
	//它在内部缓存CPU描述符句柄，这样当当前堆中没有足够的空间时，
	//可以将必要的描述符重新复制到新的堆中。
	class DynamicDescriptorHeap
	{
	public:
		DynamicDescriptorHeap(CommandContext& OwningContext, D3D12_DESCRIPTOR_HEAP_TYPE HeapType);
		~DynamicDescriptorHeap();

		static void DestroyAll(void)
		{
			sm_DescriptorHeapPool[0].clear();
			sm_DescriptorHeapPool[1].clear();
		}

		void CleanupUsedHeaps(uint64_t fenceValue);

		//将多个句柄复制到为指定根参数保留的缓存区域。 
		void SetGraphicsDescriptorHandles(UINT RootIndex, UINT Offset, UINT NumHandles, const D3D12_CPU_DESCRIPTOR_HANDLE Handles[])
		{
			m_GraphicsHandleCache.StageDescriptorHandles(RootIndex, Offset, NumHandles, Handles);
		}

		void SetComputeDescriptorHandles(UINT RootIndex, UINT Offset, UINT NumHandles, const D3D12_CPU_DESCRIPTOR_HANDLE Handles[])
		{
			m_ComputeHandleCache.StageDescriptorHandles(RootIndex, Offset, NumHandles, Handles);
		}

		//绕过缓存，直接上传到shader可见堆。 
		D3D12_GPU_DESCRIPTOR_HANDLE UploadDirect(D3D12_CPU_DESCRIPTOR_HANDLE Handles);

		//推导出支持根签名所需的描述符表所需的缓存布局。 
		void ParseGraphicsRootSignature(const RootSignature& RootSig)
		{
			m_GraphicsHandleCache.ParseRootSignature(m_DescriptorType, RootSig);
		}

		void ParseComputeRootSignature(const RootSignature& RootSig)
		{
			m_ComputeHandleCache.ParseRootSignature(m_DescriptorType, RootSig);
		}

		//将缓存中的任何新描述符上传到shader-visible堆。 
		inline void CommitGraphicsRootDescriptorTables(ID3D12GraphicsCommandList* CmdList)
		{
			if (m_GraphicsHandleCache.m_StaleRootParamsBitMap != 0)
				CopyAndBindStagedTables(m_GraphicsHandleCache, CmdList, &ID3D12GraphicsCommandList::SetGraphicsRootDescriptorTable);
		}

		inline void CommitComputeRootDescriptorTables(ID3D12GraphicsCommandList* CmdList)
		{
			if (m_ComputeHandleCache.m_StaleRootParamsBitMap != 0)
				CopyAndBindStagedTables(m_ComputeHandleCache, CmdList, &ID3D12GraphicsCommandList::SetComputeRootDescriptorTable);
		}

	private:

		// Static members
		static const uint32_t NumDescriptorsPerHeap = 1024;
		static std::mutex sm_Mutex;
		static std::vector<Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>> sm_DescriptorHeapPool[2];
		static std::queue<std::pair<uint64_t, ID3D12DescriptorHeap*>> sm_RetiredDescriptorHeaps[2];
		static std::queue<ID3D12DescriptorHeap*> sm_AvailableDescriptorHeaps[2];

		// Static methods
		static ID3D12DescriptorHeap* RequestDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE HeapType);
		static void DiscardDescriptorHeaps(D3D12_DESCRIPTOR_HEAP_TYPE HeapType, uint64_t FenceValueForReset, const std::vector<ID3D12DescriptorHeap*>& UsedHeaps);

		// Non-static members
		CommandContext& m_OwningContext;
		ID3D12DescriptorHeap* m_CurrentHeapPtr;
		const D3D12_DESCRIPTOR_HEAP_TYPE m_DescriptorType;
		uint32_t m_DescriptorSize;
		uint32_t m_CurrentOffset;
		DescriptorHandle m_FirstDescriptor;
		std::vector<ID3D12DescriptorHeap*> m_RetiredHeaps;

		//描述描述符表项：句柄缓存的一个区域，以及已经设置了哪些句柄。 
		struct DescriptorTableCache
		{
			DescriptorTableCache() : AssignedHandlesBitMap(0) {}
			uint32_t AssignedHandlesBitMap;
			D3D12_CPU_DESCRIPTOR_HANDLE* TableStart;
			uint32_t TableSize;
		};

		struct DescriptorHandleCache
		{
			DescriptorHandleCache()
			{
				ClearCache();
			}

			void ClearCache()
			{
				m_RootDescriptorTablesBitMap = 0;
				m_StaleRootParamsBitMap = 0;
				m_MaxCachedDescriptors = 0;
			}

			uint32_t m_RootDescriptorTablesBitMap;
			uint32_t m_StaleRootParamsBitMap;
			uint32_t m_MaxCachedDescriptors;

			static const uint32_t MaxNumDescriptors = 512;
			static const uint32_t MaxNumDescriptorTables = 16;

			uint32_t ComputeStagedSize();
			void CopyAndBindStaleTables(D3D12_DESCRIPTOR_HEAP_TYPE Type, uint32_t DescriptorSize, DescriptorHandle DestHandleStart, ID3D12GraphicsCommandList* CmdList,
				void (STDMETHODCALLTYPE ID3D12GraphicsCommandList::* SetFunc)(UINT, D3D12_GPU_DESCRIPTOR_HANDLE));

			DescriptorTableCache m_RootDescriptorTable[MaxNumDescriptorTables];
			D3D12_CPU_DESCRIPTOR_HANDLE m_HandleCache[MaxNumDescriptors];

			void UnbindAllValid();
			void StageDescriptorHandles(UINT RootIndex, UINT Offset, UINT NumHandles, const D3D12_CPU_DESCRIPTOR_HANDLE Handles[]);
			void ParseRootSignature(D3D12_DESCRIPTOR_HEAP_TYPE Type, const RootSignature& RootSig);
		};

		DescriptorHandleCache m_GraphicsHandleCache;
		DescriptorHandleCache m_ComputeHandleCache;

		bool HasSpace(uint32_t Count)
		{
			return (m_CurrentHeapPtr != nullptr && m_CurrentOffset + Count <= NumDescriptorsPerHeap);
		}

		void RetireCurrentHeap(void);
		void RetireUsedHeaps(uint64_t fenceValue);
		ID3D12DescriptorHeap* GetHeapPointer();

		DescriptorHandle Allocate(UINT Count)
		{
			DescriptorHandle ret = m_FirstDescriptor + m_CurrentOffset * m_DescriptorSize;
			m_CurrentOffset += Count;
			return ret;
		}

		void CopyAndBindStagedTables(DescriptorHandleCache& HandleCache, ID3D12GraphicsCommandList* CmdList,
			void (STDMETHODCALLTYPE ID3D12GraphicsCommandList::* SetFunc)(UINT, D3D12_GPU_DESCRIPTOR_HANDLE));

		//将缓存中的所有描述符标记为陈旧的、需要重新上传的描述符。 
		void UnbindAllValid(void);

	};
}

