#include "RaytracingAccelerationStructureBuffer.h"
#include "GraphicsCore.h"

namespace GlareEngine
{
    void RaytracingAccelerationStructureBuffer::Create(const std::wstring& name, uint64_t size,
        D3D12_RESOURCE_STATES initialState)
    {
        Destroy();

        D3D12_RESOURCE_DESC ResourceDesc = {};
        ResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        ResourceDesc.Width = size;
        ResourceDesc.Height = 1;
        ResourceDesc.DepthOrArraySize = 1;
        ResourceDesc.MipLevels = 1;
        ResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
        ResourceDesc.SampleDesc.Count = 1;
        ResourceDesc.SampleDesc.Quality = 0;
        ResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        ResourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

        D3D12_HEAP_PROPERTIES HeapProps;
        HeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
        HeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        HeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        HeapProps.CreationNodeMask = 1;
        HeapProps.VisibleNodeMask = 1;

        ThrowIfFailed(g_Device->CreateCommittedResource(
            &HeapProps,
            D3D12_HEAP_FLAG_NONE,
            &ResourceDesc,
            initialState,
            nullptr,
            IID_PPV_ARGS(&m_pResource)));

        m_GPUVirtualAddress = m_pResource->GetGPUVirtualAddress();
        m_UsageState = initialState;

#ifndef RELEASE
        m_pResource->SetName(name.c_str());
#else
        (name);
#endif
    }
}
