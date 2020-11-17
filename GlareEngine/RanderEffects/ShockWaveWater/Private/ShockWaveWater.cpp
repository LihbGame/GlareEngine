#include "ShockWaveWater.h"
#include "L3DBaseApp.h"
ShockWaveWater::ShockWaveWater(ID3D12Device* device, UINT width, UINT height)
	: mWidth(width),
	mHeight(height),
	mReflectionSRV(nullptr),
	mReflectionRTV(nullptr),
	mDepthMapDSV(nullptr)
{
#pragma region ReflectionMap
	D3D12_RESOURCE_DESC ReflectionMapDesc=CD3DX12_RESOURCE_DESC::Tex2D(
		DXGI_FORMAT_R8G8B8A8_UNORM,
		mWidth,
		mHeight,
		1, // This render target view has only one texture.
		1, // Use a single mipmap level
		4
	);

	ReflectionMapDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	D3D12_CLEAR_VALUE ReflectionClearValue = {};
	ReflectionClearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&ReflectionMapDesc,
		D3D12_RESOURCE_STATE_RESOLVE_SOURCE,
		&ReflectionClearValue,
		IID_PPV_ARGS(mReflectionRTV.ReleaseAndGetAddressOf())
	));

	mReflectionRTV->SetName(L"Reflection Render Target");




#pragma endregion ReflectionMap

}

ShockWaveWater::~ShockWaveWater()
{
}

ID3D12Resource* ShockWaveWater::ReflectionSRV()
{
	return nullptr;
}

ID3D12Resource* ShockWaveWater::SingleReflectionSRV()
{
	return nullptr;
}

ID3D12Resource* ShockWaveWater::SingleRefractionSRV()
{
	return nullptr;
}

ID3D12Resource* ShockWaveWater::FoamSRV()
{
	return nullptr;
}

ID3D12Resource* ShockWaveWater::ReflectionRTV()
{
	return nullptr;
}

ID3D12Resource* ShockWaveWater::ReflectionDepthMapDSV()
{
	return nullptr;
}
