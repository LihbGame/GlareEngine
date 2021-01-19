#include "DynamicDescriptorHeap.h"

GlareEngine::DirectX12Graphics::DynamicDescriptorHeap::DynamicDescriptorHeap(CommandContext& OwningContext, D3D12_DESCRIPTOR_HEAP_TYPE HeapType)
{
}

GlareEngine::DirectX12Graphics::DynamicDescriptorHeap::~DynamicDescriptorHeap()
{
}

void GlareEngine::DirectX12Graphics::DynamicDescriptorHeap::CleanupUsedHeaps(uint64_t fenceValue)
{
}

D3D12_GPU_DESCRIPTOR_HANDLE GlareEngine::DirectX12Graphics::DynamicDescriptorHeap::UploadDirect(D3D12_CPU_DESCRIPTOR_HANDLE Handles)
{
	return D3D12_GPU_DESCRIPTOR_HANDLE();
}

ID3D12DescriptorHeap* GlareEngine::DirectX12Graphics::DynamicDescriptorHeap::RequestDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE HeapType)
{
	return nullptr;
}

void GlareEngine::DirectX12Graphics::DynamicDescriptorHeap::DiscardDescriptorHeaps(D3D12_DESCRIPTOR_HEAP_TYPE HeapType, uint64_t FenceValueForReset, const std::vector<ID3D12DescriptorHeap*>& UsedHeaps)
{
}

void GlareEngine::DirectX12Graphics::DynamicDescriptorHeap::RetireCurrentHeap(void)
{
}

void GlareEngine::DirectX12Graphics::DynamicDescriptorHeap::RetireUsedHeaps(uint64_t fenceValue)
{
}

ID3D12DescriptorHeap* GlareEngine::DirectX12Graphics::DynamicDescriptorHeap::GetHeapPointer()
{
	return nullptr;
}

void GlareEngine::DirectX12Graphics::DynamicDescriptorHeap::CopyAndBindStagedTables(DescriptorHandleCache& HandleCache, ID3D12GraphicsCommandList* CmdList, void(__stdcall ID3D12GraphicsCommandList::* SetFunc)(UINT, D3D12_GPU_DESCRIPTOR_HANDLE))
{
}

void GlareEngine::DirectX12Graphics::DynamicDescriptorHeap::UnbindAllValid(void)
{
}

uint32_t GlareEngine::DirectX12Graphics::DynamicDescriptorHeap::DescriptorHandleCache::ComputeStagedSize()
{
	return uint32_t();
}

void GlareEngine::DirectX12Graphics::DynamicDescriptorHeap::DescriptorHandleCache::CopyAndBindStaleTables(D3D12_DESCRIPTOR_HEAP_TYPE Type, uint32_t DescriptorSize, DescriptorHandle DestHandleStart, ID3D12GraphicsCommandList* CmdList, void(__stdcall ID3D12GraphicsCommandList::* SetFunc)(UINT, D3D12_GPU_DESCRIPTOR_HANDLE))
{
}

void GlareEngine::DirectX12Graphics::DynamicDescriptorHeap::DescriptorHandleCache::UnbindAllValid()
{
}

void GlareEngine::DirectX12Graphics::DynamicDescriptorHeap::DescriptorHandleCache::StageDescriptorHandles(UINT RootIndex, UINT Offset, UINT NumHandles, const D3D12_CPU_DESCRIPTOR_HANDLE Handles[])
{
}

void GlareEngine::DirectX12Graphics::DynamicDescriptorHeap::DescriptorHandleCache::ParseRootSignature(D3D12_DESCRIPTOR_HEAP_TYPE Type, const RootSignature& RootSig)
{
}
