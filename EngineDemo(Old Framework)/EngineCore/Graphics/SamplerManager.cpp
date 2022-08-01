#include "SamplerManager.h"
#include "GraphicsCore.h"
#include "Hash.h"

using namespace GlareEngine::DirectX12Graphics;
namespace
{
	std::map< size_t, D3D12_CPU_DESCRIPTOR_HANDLE > s_SamplerCache;
}


D3D12_CPU_DESCRIPTOR_HANDLE SamplerDesc::CreateDescriptor()
{
	size_t hashValue = HashState(this);
	auto iter = s_SamplerCache.find(hashValue);
	if (iter != s_SamplerCache.end())
	{
		return iter->second;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE Handle = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
	g_Device->CreateSampler(this, Handle);
	return Handle;
}

void SamplerDesc::CreateDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE& Handle)
{
	g_Device->CreateSampler(this, Handle);
}