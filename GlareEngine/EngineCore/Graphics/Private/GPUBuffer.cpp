#include "GPUBuffer.h"
#include "GraphicsCore.h"
//#include "EsramAllocator.h"
//#include "CommandContext.h"
//#include "BufferManager.h"

using namespace GlareEngine;
using namespace GlareEngine::Graphics;

void GPUBuffer::Create(const std::wstring& name, 
	uint32_t NumElements, 
	uint32_t ElementSize, 
	const void* initialData)
{
	GPUResource::Destroy();

	m_ElementCount = NumElements;
	m_ElementSize = ElementSize;
	m_BufferSize = NumElements * ElementSize;

	D3D12_RESOURCE_DESC ResourceDesc = DescribeBuffer();

	m_UsageState = D3D12_RESOURCE_STATE_COMMON;

	D3D12_HEAP_PROPERTIES HeapProps;
	HeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
	HeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	HeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	HeapProps.CreationNodeMask = 1;
	HeapProps.VisibleNodeMask = 1;

	ThrowIfFailed(g_Device->CreateCommittedResource(&HeapProps, D3D12_HEAP_FLAG_NONE,
			&ResourceDesc, m_UsageState, nullptr, IID_PPV_ARGS(&m_pResource)));

	m_GPUVirtualAddress = m_pResource->GetGPUVirtualAddress();

	if (initialData)
		CommandContext::InitializeBuffer(*this, initialData, m_BufferSize);

#ifdef RELEASE
	(name);
#else
	m_pResource->SetName(name.c_str());
#endif

	CreateDerivedViews();
}

//从预先分配的堆中再分配一个缓冲区。 如果提供了初始数据，则将使用默认命令上下文将其复制到缓冲区中。
void GPUBuffer::CreatePlaced(const std::wstring& name,
	ID3D12Heap* pBackingHeap,
	uint32_t HeapOffset,
	uint32_t NumElements,
	uint32_t ElementSize,
	const void* initialData)
{
	m_ElementCount = NumElements;
	m_ElementSize = ElementSize;
	m_BufferSize = NumElements * ElementSize;

	D3D12_RESOURCE_DESC ResourceDesc = DescribeBuffer();

	m_UsageState = D3D12_RESOURCE_STATE_COMMON;

	ThrowIfFailed(g_Device->CreatePlacedResource(pBackingHeap, HeapOffset, &ResourceDesc, m_UsageState, nullptr, IID_PPV_ARGS(&m_pResource)));

	m_GPUVirtualAddress = m_pResource->GetGPUVirtualAddress();

	if (initialData)
		CommandContext::InitializeBuffer(*this, initialData, m_BufferSize);

#ifdef RELEASE
	(name);
#else
	m_pResource->SetName(name.c_str());
#endif

	CreateDerivedViews();

}

inline D3D12_CPU_DESCRIPTOR_HANDLE GlareEngine::GPUBuffer::CreateConstantBufferView(uint32_t Offset, uint32_t Size) const
{
	assert(Offset + Size <= m_BufferSize);

	Size = Math::AlignUp(Size, 16);

	D3D12_CONSTANT_BUFFER_VIEW_DESC CBVDesc;
	CBVDesc.BufferLocation = m_GPUVirtualAddress + (size_t)Offset;
	CBVDesc.SizeInBytes = Size;

	D3D12_CPU_DESCRIPTOR_HANDLE hCBV = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	g_Device->CreateConstantBufferView(&CBVDesc, hCBV);
	return hCBV;
}


inline D3D12_RESOURCE_DESC GlareEngine::GPUBuffer::DescribeBuffer(void)
{
	assert(m_BufferSize != 0);

	D3D12_RESOURCE_DESC Desc = {};
	Desc.Alignment = 0;
	Desc.DepthOrArraySize = 1;
	Desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	Desc.Flags = m_ResourceFlags;
	Desc.Format = DXGI_FORMAT_UNKNOWN;
	Desc.Height = 1;
	Desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	Desc.MipLevels = 1;
	Desc.SampleDesc.Count = 1;
	Desc.SampleDesc.Quality = 0;
	Desc.Width = (UINT64)m_BufferSize;
	return Desc;
}

void GlareEngine::ByteAddressBuffer::CreateDerivedViews(void)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
	SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	SRVDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	SRVDesc.Buffer.NumElements = (UINT)m_BufferSize / 4;
	SRVDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;

	if (m_SRV.ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
		m_SRV = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	g_Device->CreateShaderResourceView(m_pResource.Get(), &SRVDesc, m_SRV);

	D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
	UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	UAVDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	UAVDesc.Buffer.NumElements = (UINT)m_BufferSize / 4;
	UAVDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;

	if (m_UAV.ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
		m_UAV = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	g_Device->CreateUnorderedAccessView(m_pResource.Get(), nullptr, &UAVDesc, m_UAV);
}
