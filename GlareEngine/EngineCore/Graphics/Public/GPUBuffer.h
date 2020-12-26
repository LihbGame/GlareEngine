#pragma once
#include "GPUResource.h"

class CommandContext;

namespace GlareEngine
{
	class GPUBuffer :
		public GPUResource
	{
	public:
		virtual ~GPUBuffer() { Destroy(); }

		//创建一个缓冲区,如果提供了初始数据，则将使用默认命令上下文将其复制到缓冲区中。
		void Create(const std::wstring& name, 
			uint32_t NumElements, 
			uint32_t ElementSize,
			const void* initialData = nullptr);

		//从预先分配的堆中再分配一个缓冲区,如果提供了初始数据,则将使用默认命令上下文将其复制到缓冲区中。
		void CreatePlaced(const std::wstring& name, 
			ID3D12Heap* pBackingHeap, 
			uint32_t HeapOffset, 
			uint32_t NumElements, 
			uint32_t ElementSize,
			const void* initialData = nullptr);

		const D3D12_CPU_DESCRIPTOR_HANDLE& GetUAV(void) const { return m_UAV; }
		const D3D12_CPU_DESCRIPTOR_HANDLE& GetSRV(void) const { return m_SRV; }

		D3D12_GPU_VIRTUAL_ADDRESS RootConstantBufferView(void) const { return m_GpuVirtualAddress; }

		D3D12_CPU_DESCRIPTOR_HANDLE CreateConstantBufferView(uint32_t Offset, uint32_t Size) const;

		D3D12_VERTEX_BUFFER_VIEW VertexBufferView(size_t Offset, uint32_t Size, uint32_t Stride) const;
		D3D12_VERTEX_BUFFER_VIEW VertexBufferView(size_t BaseVertexIndex = 0) const
		{
			size_t Offset = BaseVertexIndex * m_ElementSize;
			return VertexBufferView(Offset, (uint32_t)(m_BufferSize - Offset), m_ElementSize);
		}

		D3D12_INDEX_BUFFER_VIEW IndexBufferView(size_t Offset, uint32_t Size, bool b32Bit = false) const;
		D3D12_INDEX_BUFFER_VIEW IndexBufferView(size_t StartIndex = 0) const
		{
			size_t Offset = StartIndex * m_ElementSize;
			return IndexBufferView(Offset, (uint32_t)(m_BufferSize - Offset), m_ElementSize == 4);
		}

		size_t GetBufferSize() const { return m_BufferSize; }
		uint32_t GetElementCount() const { return m_ElementCount; }
		uint32_t GetElementSize() const { return m_ElementSize; }

	protected:

		GPUBuffer(void) : m_BufferSize(0), m_ElementCount(0), m_ElementSize(0)
		{
			m_ResourceFlags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
			m_UAV.ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
			m_SRV.ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
		}

		D3D12_RESOURCE_DESC DescribeBuffer(void);
		virtual void CreateDerivedViews(void) = 0;

		D3D12_CPU_DESCRIPTOR_HANDLE m_UAV;
		D3D12_CPU_DESCRIPTOR_HANDLE m_SRV;

		size_t m_BufferSize;
		uint32_t m_ElementCount;
		uint32_t m_ElementSize;
		D3D12_RESOURCE_FLAGS m_ResourceFlags;
	};

	inline D3D12_VERTEX_BUFFER_VIEW GPUBuffer::VertexBufferView(size_t Offset, uint32_t Size, uint32_t Stride) const
	{
		D3D12_VERTEX_BUFFER_VIEW VBView;
		VBView.BufferLocation = m_GPUVirtualAddress + Offset;
		VBView.SizeInBytes = Size;
		VBView.StrideInBytes = Stride;
		return VBView;
	}

	inline D3D12_INDEX_BUFFER_VIEW GPUBuffer::IndexBufferView(size_t Offset, uint32_t Size, bool b32Bit) const
	{
		D3D12_INDEX_BUFFER_VIEW IBView;
		IBView.BufferLocation = m_GPUVirtualAddress + Offset;
		IBView.Format = b32Bit ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
		IBView.SizeInBytes = Size;
		return IBView;
	}

	class ByteAddressBuffer : public GPUBuffer
	{
	public:
		virtual void CreateDerivedViews(void) override;
	};

	class IndirectArgsBuffer : public ByteAddressBuffer
	{
	public:
		IndirectArgsBuffer(void)
		{
		}
	};

	class StructuredBuffer : public GPUBuffer
	{
	public:
		virtual void Destroy(void) override
		{
			m_CounterBuffer.Destroy();
			GPUBuffer::Destroy();
		}

		virtual void CreateDerivedViews(void) override;

		ByteAddressBuffer& GetCounterBuffer(void) { return m_CounterBuffer; }

		const D3D12_CPU_DESCRIPTOR_HANDLE& GetCounterSRV(CommandContext& Context);
		const D3D12_CPU_DESCRIPTOR_HANDLE& GetCounterUAV(CommandContext& Context);

	private:
		ByteAddressBuffer m_CounterBuffer;
	};

	class TypedBuffer : public GpuBuffer
	{
	public:
		TypedBuffer(DXGI_FORMAT Format) : m_DataFormat(Format) {}
		virtual void CreateDerivedViews(void) override;

	protected:
		DXGI_FORMAT m_DataFormat;
	};
}