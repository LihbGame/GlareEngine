#include "ShockWaveWater.h"
#include "L3DBaseApp.h"
ShockWaveWater::ShockWaveWater(ID3D12Device* device, UINT width, UINT height,bool Is4xMsaa, TextureManage* TextureManage)
	: mWidth(width),
	mHeight(height),
	mIs4xMsaa(Is4xMsaa),
	md3dDevice(device),
	mReflectionSRV(nullptr),
	mReflectionRTV(nullptr),
	mDepthMapDSV(nullptr),
	mTextureManage(TextureManage)
{
	// Create the RTV heap.
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors =1;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mReflectionRTVHeap)))

	// Create the DSV heap.
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc1 = {};
	srvHeapDesc1.NumDescriptors = 1;
	srvHeapDesc1.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	srvHeapDesc1.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc1, IID_PPV_ARGS(&mReflectionDSVHeap)))

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
	return mSingleReflectionSRV.Get();
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

D3D12_CPU_DESCRIPTOR_HANDLE ShockWaveWater::ReflectionDepthMapDSVDescriptor()const
{
	return mReflectionDSVHeap->GetCPUDescriptorHandleForHeapStart();
}

D3D12_CPU_DESCRIPTOR_HANDLE ShockWaveWater::ReflectionDescriptor() const
{
	return mReflectionRTVHeap->GetCPUDescriptorHandleForHeapStart();
}

D3D12_CPU_DESCRIPTOR_HANDLE ShockWaveWater::RefractionDescriptor() const
{
	return mRefractionSRVDescriptor;
}

void ShockWaveWater::BuildDescriptors(
	CD3DX12_CPU_DESCRIPTOR_HANDLE RefractionSRVDescriptor, 
	CD3DX12_CPU_DESCRIPTOR_HANDLE ReflectionSRVDescriptor,
	CD3DX12_CPU_DESCRIPTOR_HANDLE WavesBumpDescriptor)
{
	// Save references to the descriptors. 
	mRefractionSRVDescriptor = RefractionSRVDescriptor;
	mReflectionSRVDescriptor = ReflectionSRVDescriptor;
	mWavesBumpDescriptor = WavesBumpDescriptor;
	

	LoadTexture();
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

void ShockWaveWater::FillSRVDescriptorHeap(int* SRVIndex, CD3DX12_CPU_DESCRIPTOR_HANDLE* hDescriptor)
{
	UINT CbvSrvDescriptorSize = mTextureManage->GetCbvSrvDescriptorSize();
	CD3DX12_CPU_DESCRIPTOR_HANDLE RefractionSRVDescriptor = *hDescriptor;
	(*hDescriptor).Offset(1, CbvSrvDescriptorSize);
	mWaterRefractionMapIndex = (*SRVIndex)++;

	CD3DX12_CPU_DESCRIPTOR_HANDLE ReflectionRTVDescriptor = *hDescriptor;
	(*hDescriptor).Offset(1, CbvSrvDescriptorSize);
	mWaterReflectionMapIndex = (*SRVIndex)++;

	CD3DX12_CPU_DESCRIPTOR_HANDLE WavesBumpDescriptor = *hDescriptor;
	(*hDescriptor).Offset(1, CbvSrvDescriptorSize);
	mWaterDumpWaveIndex = (*SRVIndex)++;

	BuildDescriptors(
		RefractionSRVDescriptor,
		ReflectionRTVDescriptor,
		WavesBumpDescriptor);
}

void ShockWaveWater::BuildDescriptors()
{
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;

	md3dDevice->CreateRenderTargetView(
		mReflectionRTV.Get(), &rtvDesc,
		mReflectionRTVHeap->GetCPUDescriptorHandleForHeapStart());

	md3dDevice->CreateDepthStencilView(mDepthMapDSV.Get(), nullptr, mReflectionDSVHeap->GetCPUDescriptorHandleForHeapStart());

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	srvDesc.Texture2D.PlaneSlice = 0;
	md3dDevice->CreateShaderResourceView(mSingleReflectionSRV.Get(), &srvDesc, mReflectionSRVDescriptor);
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

		D3D12_RESOURCE_DESC MapDescDsv = CD3DX12_RESOURCE_DESC::Tex2D(
			DXGI_FORMAT_D24_UNORM_S8_UINT,
			mWidth,
			mHeight,
			1, // This render target view has only one texture.
			1, // Use a single mipmap level
			sampleCount
		);
		MapDescDsv.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		D3D12_CLEAR_VALUE optClear;
		optClear.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		optClear.DepthStencil.Depth = 1.0f;
		optClear.DepthStencil.Stencil = 0;
		ThrowIfFailed(md3dDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&MapDescDsv,
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&optClear,
			IID_PPV_ARGS(mDepthMapDSV.GetAddressOf())));


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

		ThrowIfFailed(md3dDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&RefractionMapDesc,
			D3D12_RESOURCE_STATE_RESOLVE_DEST,
			nullptr,
			IID_PPV_ARGS(mSingleReflectionSRV.ReleaseAndGetAddressOf())
		));


}

void ShockWaveWater::LoadTexture()
{
	auto WaterNormalTex = mTextureManage->GetTexture(L"Water\\242-normal")->Resource;
	D3D12_SHADER_RESOURCE_VIEW_DESC WaterNormalDesc = {};
	WaterNormalDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	WaterNormalDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	WaterNormalDesc.TextureCube.MostDetailedMip = 0;
	WaterNormalDesc.TextureCube.MipLevels = WaterNormalTex->GetDesc().MipLevels;
	WaterNormalDesc.TextureCube.ResourceMinLODClamp = 0.0f;
	WaterNormalDesc.Format = DXGI_FORMAT_BC1_UNORM;//linear space normal   //WaterNormalTex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(WaterNormalTex.Get(), &WaterNormalDesc, mWavesBumpDescriptor);
}
