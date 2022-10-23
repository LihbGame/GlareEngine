#pragma once
#include "GPUResource.h"
#include <vector>
#include <queue>
#include <mutex>


// 常量块必须是16个常量的倍数，每个常量为16字节。
#define DEFAULT_ALIGN 256


enum LinearAllocatorType
{
	InvalidAllocator = -1,
	GPUExclusive = 0,       // DEFAULT  GPU-writeable (via UAV)
	CPUWritable = 1,        // UPLOAD CPU-writeable (but write combined)
	NumAllocatorTypes
};

enum
{
	GPUAllocatorPageSize = 0x10000,    // 64K
	CPUAllocatorPageSize = 0x200000    // 2MB
};

namespace GlareEngine
{
	//各种类型的分配可能包含NULL指针。 如果你不确定，请在取消引用前进行检查。
	struct DynamicAlloc
	{
		DynamicAlloc(GPUResource& BaseResource, size_t ThisOffset, size_t ThisSize)
			: Buffer(BaseResource), Offset(ThisOffset), Size(ThisSize) {}

		GPUResource& Buffer;      // 与该内存相关联的D3D缓冲区。
		size_t Offset;            // 缓冲区资源开始的偏移量.
		size_t Size;              // 本次分配的保留大小
		void* DataPtr;            // CPU可写地址
		D3D12_GPU_VIRTUAL_ADDRESS GPUAddress;    // GPU可见地址
	};


	class LinearAllocationPage : public GPUResource
	{
	public:
		LinearAllocationPage(ID3D12Resource* pResource, D3D12_RESOURCE_STATES Usage) : GPUResource()
		{
			m_pResource.Attach(pResource);
			m_UsageState = Usage;
			m_GPUVirtualAddress = m_pResource->GetGPUVirtualAddress();
			m_pResource->Map(0, nullptr, &m_CPUVirtualAddress);
		}

		~LinearAllocationPage()
		{
			Unmap();
		}

		void Map(void)
		{
			if (m_CPUVirtualAddress == nullptr)
			{
				m_pResource->Map(0, nullptr, &m_CPUVirtualAddress);
			}
		}

		void Unmap(void)
		{
			if (m_CPUVirtualAddress != nullptr)
			{
				m_pResource->Unmap(0, nullptr);
				m_CPUVirtualAddress = nullptr;
			}
		}

		void* m_CPUVirtualAddress;
		D3D12_GPU_VIRTUAL_ADDRESS m_GPUVirtualAddress;
	};

	//Linear Allocator Page Manager
	class LinearAllocatorPageManager
	{
	public:

		LinearAllocatorPageManager();
		LinearAllocationPage* RequestPage(void);
		LinearAllocationPage* CreateNewPage(size_t PageSize = 0);

		// Discarded pages will get recycled.  This is for fixed size pages.
		void DiscardPages(uint64_t FenceID, const std::vector<LinearAllocationPage*>& Pages);

		// Freed pages will be destroyed once their fence has passed.  This is for single-use,"large" pages.
		void FreeLargePages(uint64_t FenceID, const std::vector<LinearAllocationPage*>& Pages);

		void Destroy(void) { m_PagePool.clear(); }

	private:

		static LinearAllocatorType sm_AutoType;

		LinearAllocatorType m_AllocationType;
		std::vector<std::unique_ptr<LinearAllocationPage> > m_PagePool;
		std::queue<std::pair<uint64_t, LinearAllocationPage*> > m_RetiredPages;
		std::queue<std::pair<uint64_t, LinearAllocationPage*> > m_DeletionQueue;
		std::queue<LinearAllocationPage*> m_AvailablePages;
		std::mutex m_Mutex;
	};



	class LinearAllocator
	{
	public:

		LinearAllocator(LinearAllocatorType Type) : m_AllocationType(Type), m_PageSize(0), m_CurOffset(~(size_t)0), m_CurPage(nullptr)
		{
			assert(Type > InvalidAllocator && Type < NumAllocatorTypes);
			m_PageSize = (Type == GPUExclusive ? GPUAllocatorPageSize : CPUAllocatorPageSize);
		}

		DynamicAlloc Allocate(size_t SizeInBytes, size_t Alignment = DEFAULT_ALIGN);

		void CleanupUsedPages(uint64_t FenceID);

		static void DestroyAll(void)
		{
			sm_PageManager[0].Destroy();
			sm_PageManager[1].Destroy();
		}

	private:

		DynamicAlloc AllocateLargePage(size_t SizeInBytes);

		static LinearAllocatorPageManager sm_PageManager[2];

		LinearAllocatorType m_AllocationType;
		size_t m_PageSize;
		size_t m_CurOffset;
		LinearAllocationPage* m_CurPage;
		std::vector<LinearAllocationPage*> m_RetiredPages;
		std::vector<LinearAllocationPage*> m_LargePageList;
	};
}
