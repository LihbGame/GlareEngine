#include "Engine/EngineUtility.h"
#include "GraphicsCore.h"
#include "DynamicUploadBuffer.h"

void* DynamicUploadBuffer::Map(void)
{
	
	assert(m_CPUVirtualAddress == nullptr);//Buffer is already locked
	ThrowIfFailed(m_pResource->Map(0, nullptr, &m_CPUVirtualAddress));
	return m_CPUVirtualAddress;
}


void DynamicUploadBuffer::Unmap(void)
{
	assert(m_CPUVirtualAddress != nullptr);//Buffer is not locked
	m_pResource->Unmap(0, nullptr);
	m_CPUVirtualAddress = nullptr;
}


void DynamicUploadBuffer::Create(const std::wstring& name, uint32_t NumElements, uint32_t ElementSize)
{
	D3D12_HEAP_PROPERTIES HeapProps;
	HeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	HeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	HeapProps.CreationNodeMask = 1;
	HeapProps.VisibleNodeMask = 1;
	HeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

	D3D12_RESOURCE_DESC ResourceDesc;
	ResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	ResourceDesc.Alignment = 0;
	ResourceDesc.Height = 1;
	ResourceDesc.DepthOrArraySize = 1;
	ResourceDesc.MipLevels = 1;
	ResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	ResourceDesc.SampleDesc.Count = 1;
	ResourceDesc.SampleDesc.Quality = 0;
	ResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	ResourceDesc.Width = NumElements * ElementSize;
	ResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	ThrowIfFailed(g_Device->CreateCommittedResource(&HeapProps, D3D12_HEAP_FLAG_NONE, &ResourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_pResource)));

	m_pResource->SetName(name.c_str());

	m_GPUVirtualAddress = m_pResource->GetGPUVirtualAddress();
	m_CPUVirtualAddress = nullptr;
}

void DynamicUploadBuffer::Destroy(void)
{
	if (m_pResource.Get() != nullptr)
	{
		if (m_CPUVirtualAddress != nullptr)
			Unmap();

		m_pResource = nullptr;
		m_GPUVirtualAddress = D3D12_GPU_VIRTUAL_ADDRESS_NULL;
	}
}


D3D12_VERTEX_BUFFER_VIEW DynamicUploadBuffer::VertexBufferView(uint32_t NumVertices, uint32_t Stride, uint32_t Offset) const
{
	D3D12_VERTEX_BUFFER_VIEW vbv;
	vbv.BufferLocation = m_GPUVirtualAddress + Offset;
	vbv.SizeInBytes = NumVertices * Stride;
	vbv.StrideInBytes = Stride;
	return vbv;
}

D3D12_INDEX_BUFFER_VIEW DynamicUploadBuffer::IndexBufferView(uint32_t NumIndices, bool _32bit, uint32_t Offset) const
{
	D3D12_INDEX_BUFFER_VIEW ibv;
	ibv.BufferLocation = m_GPUVirtualAddress + Offset;
	ibv.Format = _32bit ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
	ibv.SizeInBytes = NumIndices * (_32bit ? 4 : 2);
	return ibv;
}
