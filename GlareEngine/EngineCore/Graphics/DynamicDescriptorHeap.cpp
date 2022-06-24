#include "DynamicDescriptorHeap.h"
#include "CommandContext.h"
#include "GraphicsCore.h"
#include "CommandListManager.h"
#include "RootSignature.h"


#define MaxDescriptorsPerCopy  256

namespace GlareEngine
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
		RetireCurrentHeap();
		RetireUsedHeaps(fenceValue);
		m_GraphicsHandleCache.ClearCache();
		m_ComputeHandleCache.ClearCache();
	}

	D3D12_GPU_DESCRIPTOR_HANDLE DynamicDescriptorHeap::UploadDirect(D3D12_CPU_DESCRIPTOR_HANDLE Handles)
	{
		if (!HasSpace(1))
		{
			RetireCurrentHeap();
			UnbindAllValid();
		}

		m_OwningContext.SetDescriptorHeap(m_DescriptorType, GetHeapPointer());

		DescriptorHandle DestHandle = m_FirstDescriptor + m_CurrentOffset * m_DescriptorSize;
		m_CurrentOffset += 1;

		g_Device->CopyDescriptorsSimple(1, DestHandle, Handles, m_DescriptorType);

		return DestHandle;
	}

	ID3D12DescriptorHeap* DynamicDescriptorHeap::RequestDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE HeapType)
	{
		std::lock_guard<std::mutex> LockGuard(sm_Mutex);

		uint32_t idx = HeapType == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER ? 1 : 0;

		while (!sm_RetiredDescriptorHeaps[idx].empty() && g_CommandManager.IsFenceComplete(sm_RetiredDescriptorHeaps[idx].front().first))
		{
			sm_AvailableDescriptorHeaps[idx].push(sm_RetiredDescriptorHeaps[idx].front().second);
			sm_RetiredDescriptorHeaps[idx].pop();
		}

		if (!sm_AvailableDescriptorHeaps[idx].empty())
		{
			ID3D12DescriptorHeap* HeapPtr = sm_AvailableDescriptorHeaps[idx].front();
			sm_AvailableDescriptorHeaps[idx].pop();
			return HeapPtr;
		}
		else
		{
			D3D12_DESCRIPTOR_HEAP_DESC HeapDesc = {};
			HeapDesc.Type = HeapType;
			HeapDesc.NumDescriptors = NumDescriptorsPerHeap;
			HeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			HeapDesc.NodeMask = 1;
			Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> HeapPtr;
			ThrowIfFailed(g_Device->CreateDescriptorHeap(&HeapDesc, IID_PPV_ARGS(&HeapPtr)));
			sm_DescriptorHeapPool[idx].emplace_back(HeapPtr);
			return HeapPtr.Get();
		}
	}

	void DynamicDescriptorHeap::DiscardDescriptorHeaps(D3D12_DESCRIPTOR_HEAP_TYPE HeapType, uint64_t FenceValueForReset, const std::vector<ID3D12DescriptorHeap*>& UsedHeaps)
	{
		uint32_t idx = HeapType == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER ? 1 : 0;
		std::lock_guard<std::mutex> LockGuard(sm_Mutex);
		for (auto iter = UsedHeaps.begin(); iter != UsedHeaps.end(); ++iter)
			sm_RetiredDescriptorHeaps[idx].push(std::make_pair(FenceValueForReset, *iter));
	}

	void DynamicDescriptorHeap::RetireCurrentHeap(void)
	{
		// Don't retire unused heaps.
		if (m_CurrentOffset == 0)
		{
			assert(m_CurrentHeapPtr == nullptr);
			return;
		}

		assert(m_CurrentHeapPtr != nullptr);
		m_RetiredHeaps.push_back(m_CurrentHeapPtr);
		m_CurrentHeapPtr = nullptr;
		m_CurrentOffset = 0;
	}

	void DynamicDescriptorHeap::RetireUsedHeaps(uint64_t fenceValue)
	{
		DiscardDescriptorHeaps(m_DescriptorType, fenceValue, m_RetiredHeaps);
		m_RetiredHeaps.clear();
	}

	ID3D12DescriptorHeap* DynamicDescriptorHeap::GetHeapPointer()
	{
		if (m_CurrentHeapPtr == nullptr)
		{
			assert(m_CurrentOffset == 0);
			m_CurrentHeapPtr = RequestDescriptorHeap(m_DescriptorType);
			m_FirstDescriptor = DescriptorHandle(
				m_CurrentHeapPtr->GetCPUDescriptorHandleForHeapStart(),
				m_CurrentHeapPtr->GetGPUDescriptorHandleForHeapStart());
		}

		return m_CurrentHeapPtr;
	}

	void DynamicDescriptorHeap::CopyAndBindStagedTables(DescriptorHandleCache& HandleCache, ID3D12GraphicsCommandList* CmdList, void(__stdcall ID3D12GraphicsCommandList::* SetFunc)(UINT, D3D12_GPU_DESCRIPTOR_HANDLE))
	{
		uint32_t NeededSize = HandleCache.ComputeStagedSize();
		if (!HasSpace(NeededSize))
		{
			RetireCurrentHeap();
			UnbindAllValid();
			NeededSize = HandleCache.ComputeStagedSize();
		}

		// This can trigger the creation of a new heap
		m_OwningContext.SetDescriptorHeap(m_DescriptorType, GetHeapPointer());
		HandleCache.CopyAndBindStaleTables(m_DescriptorType, m_DescriptorSize, Allocate(NeededSize), CmdList, SetFunc);
	}

	void DynamicDescriptorHeap::UnbindAllValid(void)
	{
		m_GraphicsHandleCache.UnbindAllValid();
		m_ComputeHandleCache.UnbindAllValid();
	}

	void DynamicDescriptorHeap::DescriptorHandleCache::UnbindAllValid(void)
	{
		m_StaleRootParamsBitMap = 0;

		unsigned long TableParams = m_RootDescriptorTablesBitMap;
		unsigned long RootIndex;
		while (_BitScanForward(&RootIndex, TableParams))
		{
			TableParams ^= (1 << RootIndex);
			if (m_RootDescriptorTable[RootIndex].AssignedHandlesBitMap != 0)
				m_StaleRootParamsBitMap |= (1 << RootIndex);
		}
	}

	uint32_t DynamicDescriptorHeap::DescriptorHandleCache::ComputeStagedSize()
	{
		// Sum the maximum assigned offsets of stale descriptor tables to determine total needed space.
		uint32_t NeededSpace = 0;
		uint32_t RootIndex;
		uint32_t StaleParams = m_StaleRootParamsBitMap;
		while (_BitScanForward((unsigned long*)&RootIndex, StaleParams))
		{
			StaleParams ^= (1 << RootIndex);

			uint32_t MaxSetHandle = 0;
			//Root entry marked as stale but has no stale descriptors
			//assert(TRUE == _BitScanReverse((unsigned long*)&MaxSetHandle, (unsigned long)m_RootDescriptorTable[RootIndex].AssignedHandlesBitMap));

			NeededSpace += m_RootDescriptorTable[RootIndex].AssignedHandlesBitMap;//MaxSetHandle + 1;
		}
		return NeededSpace;
	}

	void DynamicDescriptorHeap::DescriptorHandleCache::CopyAndBindStaleTables(D3D12_DESCRIPTOR_HEAP_TYPE Type, uint32_t DescriptorSize,
		DescriptorHandle DestHandleStart, ID3D12GraphicsCommandList* CmdList,
		void(__stdcall ID3D12GraphicsCommandList::* SetFunc)(UINT, D3D12_GPU_DESCRIPTOR_HANDLE))
	{
		uint32_t StaleParamCount = 0;
		uint32_t TableSize[DescriptorHandleCache::MaxNumDescriptorTables];
		uint32_t RootIndices[DescriptorHandleCache::MaxNumDescriptorTables];
		uint32_t NeededSpace = 0;
		uint32_t RootIndex;

		// Sum the maximum assigned offsets of stale descriptor tables to determine total needed space.
		uint32_t StaleParams = m_StaleRootParamsBitMap;
		while (_BitScanForward((unsigned long*)&RootIndex, StaleParams))
		{
			RootIndices[StaleParamCount] = RootIndex;
			StaleParams ^= (1 << RootIndex);

			uint32_t MaxSetHandle = 0;
			//Root entry marked as stale but has no stale descriptors
			//assert(TRUE == _BitScanReverse((unsigned long*)&MaxSetHandle, (unsigned long)m_RootDescriptorTable[RootIndex].AssignedHandlesBitMap));

			NeededSpace += m_RootDescriptorTable[RootIndex].AssignedHandlesBitMap;// MaxSetHandle + 1;
			TableSize[StaleParamCount] = m_RootDescriptorTable[RootIndex].AssignedHandlesBitMap;// MaxSetHandle + 1;

			++StaleParamCount;
		}
		//We're only equipped to handle so many descriptor tables
		assert(StaleParamCount <= DescriptorHandleCache::MaxNumDescriptorTables);

		m_StaleRootParamsBitMap = 0;

		static const uint32_t kMaxDescriptorsPerCopy = MaxDescriptorsPerCopy;
		UINT NumDestDescriptorRanges = 0;
		D3D12_CPU_DESCRIPTOR_HANDLE pDestDescriptorRangeStarts[kMaxDescriptorsPerCopy];
		UINT pDestDescriptorRangeSizes[kMaxDescriptorsPerCopy];

		UINT NumSrcDescriptorRanges = 0;
		D3D12_CPU_DESCRIPTOR_HANDLE pSrcDescriptorRangeStarts[kMaxDescriptorsPerCopy];
		UINT pSrcDescriptorRangeSizes[kMaxDescriptorsPerCopy];

		for (uint32_t i = 0; i < StaleParamCount; ++i)
		{
			RootIndex = RootIndices[i];
			(CmdList->*SetFunc)(RootIndex, DestHandleStart);

			DescriptorTableCache& RootDescTable = m_RootDescriptorTable[RootIndex];

			D3D12_CPU_DESCRIPTOR_HANDLE* SrcHandles = RootDescTable.TableStart;
			uint32_t SetHandles = (uint64_t)RootDescTable.AssignedHandlesBitMap;
			D3D12_CPU_DESCRIPTOR_HANDLE CurDest = DestHandleStart;
			DestHandleStart += TableSize[i] * DescriptorSize;

			//unsigned long SkipCount;
			//while (_BitScanForward64(&SkipCount, SetHandles))
			//{
			//	// Skip over unset descriptor handles
			//	SetHandles >>= SkipCount;
			//	SrcHandles += SkipCount;
			//	CurDest.ptr += SkipCount * DescriptorSize;
			//
			//	unsigned long DescriptorCount;
			//	_BitScanForward64(&DescriptorCount, ~SetHandles);
			//	SetHandles >>= DescriptorCount;
			//
			//	// If we run out of temp room, copy what we've got so far
			//	if (NumSrcDescriptorRanges + DescriptorCount > kMaxDescriptorsPerCopy)
			//	{
			//		g_Device->CopyDescriptors(
			//			NumDestDescriptorRanges, pDestDescriptorRangeStarts, pDestDescriptorRangeSizes,
			//			NumSrcDescriptorRanges, pSrcDescriptorRangeStarts, pSrcDescriptorRangeSizes,
			//			Type);
			//
			//		NumSrcDescriptorRanges = 0;
			//		NumDestDescriptorRanges = 0;
			//	}
			//
			//	// Setup destination range
			//	pDestDescriptorRangeStarts[NumDestDescriptorRanges] = CurDest;
			//	pDestDescriptorRangeSizes[NumDestDescriptorRanges] = DescriptorCount;
			//	++NumDestDescriptorRanges;
			//
			//	// Setup source ranges (one descriptor each because we don't assume they are contiguous)
			//	for (uint32_t j = 0; j < DescriptorCount; ++j)
			//	{
			//		pSrcDescriptorRangeStarts[NumSrcDescriptorRanges] = SrcHandles[j];
			//		pSrcDescriptorRangeSizes[NumSrcDescriptorRanges] = 1;
			//		++NumSrcDescriptorRanges;
			//	}
			//
			//	// Move the destination pointer forward by the number of descriptors we will copy
			//	SrcHandles += DescriptorCount;
			//	CurDest.ptr += DescriptorCount * DescriptorSize;
			//}

			//not Skip
			{
				// If we run out of temp room, copy what we've got so far
				if (NumSrcDescriptorRanges + SetHandles > kMaxDescriptorsPerCopy)
				{
					g_Device->CopyDescriptors(
						NumDestDescriptorRanges, pDestDescriptorRangeStarts, pDestDescriptorRangeSizes,
						NumSrcDescriptorRanges, pSrcDescriptorRangeStarts, pSrcDescriptorRangeSizes,
						Type);

					NumSrcDescriptorRanges = 0;
					NumDestDescriptorRanges = 0;
				}

				// Setup destination range
				pDestDescriptorRangeStarts[NumDestDescriptorRanges] = CurDest;
				pDestDescriptorRangeSizes[NumDestDescriptorRanges] = SetHandles;
				++NumDestDescriptorRanges;

				// Setup source ranges (one descriptor each because we don't assume they are contiguous)
				for (uint32_t j = 0; j < SetHandles; ++j)
				{
					pSrcDescriptorRangeStarts[NumSrcDescriptorRanges] = SrcHandles[j];
					pSrcDescriptorRangeSizes[NumSrcDescriptorRanges] = 1;
					++NumSrcDescriptorRanges;
				}

				// Move the destination pointer forward by the number of descriptors we will copy
				SrcHandles += SetHandles;
				CurDest.ptr += SetHandles * DescriptorSize;
			}
		}

		g_Device->CopyDescriptors(
			NumDestDescriptorRanges, pDestDescriptorRangeStarts, pDestDescriptorRangeSizes,
			NumSrcDescriptorRanges, pSrcDescriptorRangeStarts, pSrcDescriptorRangeSizes,
			Type);
	}


	void DynamicDescriptorHeap::DescriptorHandleCache::StageDescriptorHandles(UINT RootIndex, UINT Offset, UINT NumHandles, const D3D12_CPU_DESCRIPTOR_HANDLE Handles[])
	{
		//Root parameter is not a CBV_SRV_UAV descriptor table
		assert(((1 << RootIndex) & m_RootDescriptorTablesBitMap) != 0);
		assert(Offset + NumHandles <= m_RootDescriptorTable[RootIndex].TableSize);

		DescriptorTableCache& TableCache = m_RootDescriptorTable[RootIndex];
		D3D12_CPU_DESCRIPTOR_HANDLE* CopyDest = TableCache.TableStart + Offset;
		for (UINT i = 0; i < NumHandles; ++i)
			CopyDest[i] = Handles[i];
		TableCache.AssignedHandlesBitMap += NumHandles;//|= (((LONG64)1 << NumHandles) - 1) << Offset;
		m_StaleRootParamsBitMap |= (1 << RootIndex);
	}

	void DynamicDescriptorHeap::DescriptorHandleCache::ParseRootSignature(D3D12_DESCRIPTOR_HEAP_TYPE Type, const RootSignature& RootSig)
	{
		UINT CurrentOffset = 0;
		//Maybe we need to support something greater
		assert(RootSig.m_NumParameters <= 16);

		m_StaleRootParamsBitMap = 0;
		m_RootDescriptorTablesBitMap = (Type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER ?
			RootSig.m_SamplerTableBitMap : RootSig.m_DescriptorTableBitMap);

		unsigned long TableParams = m_RootDescriptorTablesBitMap;
		unsigned long RootIndex;
		while (_BitScanForward(&RootIndex, TableParams))
		{
			TableParams ^= (1 << RootIndex);

			UINT TableSize = RootSig.m_DescriptorTableSize[RootIndex];
			assert(TableSize > 0);

			DescriptorTableCache& RootDescriptorTable = m_RootDescriptorTable[RootIndex];
			RootDescriptorTable.AssignedHandlesBitMap = 0;
			RootDescriptorTable.TableStart = m_HandleCache + CurrentOffset;
			RootDescriptorTable.TableSize = TableSize;

			CurrentOffset += TableSize;
		}

		m_MaxCachedDescriptors = CurrentOffset;
		//Exceeded user-supplied maximum cache size
		assert(m_MaxCachedDescriptors <= MaxNumDescriptors);
	}
}
