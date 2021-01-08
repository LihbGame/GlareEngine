#include "ReadbackBuffer.h"
#include "GraphicsCore.h"
using namespace GlareEngine::Graphics;

void GlareEngine::ReadbackBuffer::Create(const std::wstring& name, uint32_t NumElements, uint32_t ElementSize)
{
	Destroy();

	m_ElementCount = NumElements;
	m_ElementSize = ElementSize;
	m_BufferSize = NumElements * ElementSize;
	m_UsageState = D3D12_RESOURCE_STATE_COPY_DEST;

	//创建一个足够大的回读缓冲区，以容纳所有的texel数据。 
	D3D12_HEAP_PROPERTIES HeapProps;
	HeapProps.Type = D3D12_HEAP_TYPE_READBACK;
	HeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	HeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	HeapProps.CreationNodeMask = 1;
	HeapProps.VisibleNodeMask = 1;

	//回读缓冲区必须是一维的，即 "buffer "而不是 "texture2d" 
	D3D12_RESOURCE_DESC ResourceDesc = {};
	ResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	ResourceDesc.Width = m_BufferSize;
	ResourceDesc.Height = 1;
	ResourceDesc.DepthOrArraySize = 1;
	ResourceDesc.MipLevels = 1;
	ResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	ResourceDesc.SampleDesc.Count = 1;
	ResourceDesc.SampleDesc.Quality = 0;
	ResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	ResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	ThrowIfFailed(g_Device->CreateCommittedResource(&HeapProps, D3D12_HEAP_FLAG_NONE, &ResourceDesc,
		D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_pResource)));

	m_GPUVirtualAddress = m_pResource->GetGPUVirtualAddress();

#ifdef RELEASE
	(name);
#else
	m_pResource->SetName(name.c_str());
#endif
}

void* GlareEngine::ReadbackBuffer::Map(void)
{
	void* Memory;
	m_pResource->Map(0, &CD3DX12_RANGE(0, m_BufferSize), &Memory);
	return Memory;
}

void GlareEngine::ReadbackBuffer::Unmap(void)
{
	m_pResource->Unmap(0, &CD3DX12_RANGE(0, 0));
}
