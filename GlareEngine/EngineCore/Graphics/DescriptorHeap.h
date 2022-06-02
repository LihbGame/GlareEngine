#pragma once

#include <mutex>
#include <vector>
#include <queue>
#include <string>
#include "EngineUtility.h"
namespace GlareEngine
{
	namespace DirectX12Graphics
	{
		//这是一个无界资源描述符分配器。 旨在在创建资源时为CPU可见的资源描述符提供空间。 
		//对于那些需要使着色器可见的对象，需要将它们复制到UserDescriptorHeap或DynamicDescriptorHeap。
		class DescriptorAllocator
		{
		public:
			DescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE Type) : m_Type(Type), m_CurrentHeap(nullptr) {}

			D3D12_CPU_DESCRIPTOR_HANDLE Allocate(uint32_t Count);

			static void DestroyAll(void);

		protected:

			static const uint32_t sm_NumDescriptorsPerHeap = 256;
			static std::mutex sm_AllocationMutex;
			static std::vector<Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>> sm_DescriptorHeapPool;
			static ID3D12DescriptorHeap* RequestNewHeap(D3D12_DESCRIPTOR_HEAP_TYPE Type);

			D3D12_DESCRIPTOR_HEAP_TYPE m_Type;
			ID3D12DescriptorHeap* m_CurrentHeap;
			D3D12_CPU_DESCRIPTOR_HANDLE m_CurrentHandle;
			uint32_t m_DescriptorSize;
			uint32_t m_RemainingFreeHandles;
		};

		//DescriptorHandle
		class DescriptorHandle
		{
		public:
			DescriptorHandle()
			{
				m_CPUHandle.ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
				m_GPUHandle.ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
			}

			DescriptorHandle(D3D12_CPU_DESCRIPTOR_HANDLE CpuHandle)
				: m_CPUHandle(CpuHandle)
			{
				m_GPUHandle.ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
			}

			DescriptorHandle(D3D12_CPU_DESCRIPTOR_HANDLE CpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE GpuHandle)
				: m_CPUHandle(CpuHandle), m_GPUHandle(GpuHandle)
			{
			}

			DescriptorHandle operator+ (INT OffsetScaledByDescriptorSize) const
			{
				DescriptorHandle ret = *this;
				ret += OffsetScaledByDescriptorSize;
				return ret;
			}

			void operator += (INT OffsetScaledByDescriptorSize)
			{
				if (m_CPUHandle.ptr != D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
					m_CPUHandle.ptr += OffsetScaledByDescriptorSize;
				if (m_GPUHandle.ptr != D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
					m_GPUHandle.ptr += OffsetScaledByDescriptorSize;
			}

			D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle() const { return m_CPUHandle; }

			D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle() const { return m_GPUHandle; }

			bool IsNull() const { return m_CPUHandle.ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN; }
			bool IsShaderVisible() const { return m_GPUHandle.ptr != D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN; }

		private:
			D3D12_CPU_DESCRIPTOR_HANDLE m_CPUHandle;
			D3D12_GPU_DESCRIPTOR_HANDLE m_GPUHandle;
		};


		class UserDescriptorHeap
		{
		public:

			UserDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE Type, uint32_t MaxCount)
			{
				m_HeapDesc.Type = Type;
				m_HeapDesc.NumDescriptors = MaxCount;
				m_HeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
				m_HeapDesc.NodeMask = 1;
			}

			void Create(const std::wstring& DebugHeapName);

			bool HasAvailableSpace(uint32_t Count) const { return Count <= m_NumFreeDescriptors; }
			DescriptorHandle Alloc(uint32_t Count = 1);

			DescriptorHandle GetHandleAtOffset(uint32_t Offset) const { return m_FirstHandle + Offset * m_DescriptorSize; }

			bool ValidateHandle(const DescriptorHandle& DHandle) const;

			ID3D12DescriptorHeap* GetHeapPointer() const { return m_Heap.Get(); }

		private:

			Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_Heap;
			D3D12_DESCRIPTOR_HEAP_DESC m_HeapDesc;
			uint32_t m_DescriptorSize;
			uint32_t m_NumFreeDescriptors;
			DescriptorHandle m_FirstHandle;
			DescriptorHandle m_NextFreeHandle;
		};
	}//namespace Direct12Graphics
}//namespace GlareEngine