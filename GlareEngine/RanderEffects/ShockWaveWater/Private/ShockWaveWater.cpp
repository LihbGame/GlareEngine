#include "ShockWaveWater.h"
#include "L3DBaseApp.h"
ShockWaveWater::ShockWaveWater(ID3D12Device* device, UINT width, UINT height,bool Is4xMsaa)
	: mWidth(width),
	mHeight(height),
	mIs4xMsaa(Is4xMsaa),
	md3dDevice(device),
	mReflectionSRV(nullptr),
	mReflectionRTV(nullptr),
	mDepthMapDSV(nullptr)
{
	// Create the RTV heap.
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors =1;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mReflectionRTVHeap)))

	BuildResource();
}

ShockWaveWater::~ShockWaveWater()
{
}

ID3D12Resource* ShockWaveWater::ReflectionSRV()const
{
	return nullptr;
}

ID3D12Resource* ShockWaveWater::SingleReflectionSRV()const
{
	return nullptr;
}

ID3D12Resource* ShockWaveWater::SingleRefractionSRV()const
{
	return mSingleRefractionSRV.Get();
}

ID3D12Resource* ShockWaveWater::FoamSRV()const
{
	return nullptr;
}

ID3D12Resource* ShockWaveWater::ReflectionRTV()const
{
	return mReflectionRTV.Get();
}

ID3D12Resource* ShockWaveWater::ReflectionDepthMapDSV()const
{
	return nullptr;
}

D3D12_CPU_DESCRIPTOR_HANDLE ShockWaveWater::ReflectionDescriptor() const
{
	return mReflectionRTVHeap->GetCPUDescriptorHandleForHeapStart();
}

D3D12_CPU_DESCRIPTOR_HANDLE ShockWaveWater::RefractionDescriptor() const
{
	return mRefractionSRVDescriptor;
}

void ShockWaveWater::BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE RefractionSRVDescriptor, CD3DX12_CPU_DESCRIPTOR_HANDLE ReflectionSRVDescriptor)
{
	// Save references to the descriptors. 
	mRefractionSRVDescriptor = RefractionSRVDescriptor;
	mReflectionSRVDescriptor = ReflectionSRVDescriptor;

	//  Create the descriptors
	BuildDescriptors();
}

void ShockWaveWater::OnResize(UINT newWidth, UINT newHeight)
{
	if ((mWidth != newWidth) || (mHeight != newHeight))
	{
		mWidth = newWidth;
		mHeight = newHeight;

		BuildResource();
		// New resource, so we need new descriptors to that resource.
		BuildDescriptors();
	}
}

void ShockWaveWater::BuildDescriptors()
{
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;

	md3dDevice->CreateRenderTargetView(
		mReflectionRTV.Get(), &rtvDesc,
		mReflectionRTVHeap->GetCPUDescriptorHandleForHeapStart());

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	srvDesc.Texture2D.PlaneSlice = 0;
	md3dDevice->CreateShaderResourceView(mReflectionRTV.Get(), &srvDesc, mReflectionSRVDescriptor);

	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	md3dDevice->CreateShaderResourceView(mSingleRefractionSRV.Get(), &srvDesc, mRefractionSRVDescriptor);
}

void ShockWaveWater::BuildResource()
{
	UINT sampleCount = 1;
	if (mIs4xMsaa)
	{
		sampleCount = 4;
	}
	D3D12_RESOURCE_DESC MapDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		DXGI_FORMAT_R8G8B8A8_UNORM,
		mWidth,
		mHeight,
		1, // This render target view has only one texture.
		1, // Use a single mipmap level
		sampleCount
	);
#pragma region ReflectionMap
		MapDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

		D3D12_CLEAR_VALUE ReflectionClearValue = {};
		ReflectionClearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		ReflectionClearValue.Color[0] = { 0.0f };
		ReflectionClearValue.Color[1] = { 0.0f };
		ReflectionClearValue.Color[2] = { 0.0f };
		ReflectionClearValue.Color[3] = { 0.0f };

			ThrowIfFailed(md3dDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&MapDesc,
			D3D12_RESOURCE_STATE_RESOLVE_SOURCE,
			&ReflectionClearValue,
			IID_PPV_ARGS(mReflectionRTV.ReleaseAndGetAddressOf())
		));

		mReflectionRTV->SetName(L"Reflection Render Target");
#pragma endregion ReflectionMap
	
#pragma region RefractionMap

		D3D12_RESOURCE_DESC RefractionMapDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			DXGI_FORMAT_R8G8B8A8_UNORM,
			mWidth,
			mHeight,
			1, // This render target view has only one texture.
			1, // Use a single mipmap level
			1
		);


		RefractionMapDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		ThrowIfFailed(md3dDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&RefractionMapDesc,
			D3D12_RESOURCE_STATE_RESOLVE_DEST,
			nullptr,
			IID_PPV_ARGS(mSingleRefractionSRV.ReleaseAndGetAddressOf())
		));
#pragma endregion RefractionMap




}
