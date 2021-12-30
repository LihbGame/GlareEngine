#include "CubeRenderTarget.h"
#include "GraphicsCore.h"
CubeRenderTarget::CubeRenderTarget(UINT width, UINT height,UINT mipmap, XMFLOAT3 CameraPostion, DXGI_FORMAT format)
	:mWidth(width),
	mHeight(height),
	mMipMap(mipmap),
	mCameraPostion(CameraPostion),
	mFormat(format)
{
	mViewport = { 0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f };
	mScissorRect = { 0, 0, (int)width, (int)height };

	BuildResource();
	BuildDescriptors();
	BuildCubeFaceCamera();
	BuildCubeMapFacePassCBs();
}

CubeRenderTarget::~CubeRenderTarget()
{
	mCubeMap.Destroy();
}

ColorBuffer& CubeRenderTarget::Resource()
{
	return mCubeMap;
}

D3D12_CPU_DESCRIPTOR_HANDLE CubeRenderTarget::SRV()
{
	return mCubeSrv;
}

D3D12_CPU_DESCRIPTOR_HANDLE CubeRenderTarget::RTV(int faceIndex)
{
	assert(faceIndex < mRenderTargetDes.size());
	return mRenderTargetDes[faceIndex];
}

void CubeRenderTarget::BuildCubeFaceCamera()
{
	// Generate the cube map about the given position.
	XMFLOAT3 Center(mCameraPostion);
	XMFLOAT3 WorldUp(0.0f, 1.0f, 0.0f);

	// Look along each coordinate axis.
	XMFLOAT3 Targets[6] =
	{
		XMFLOAT3(mCameraPostion.x + 1.0f, mCameraPostion.y, mCameraPostion.z), // +X
		XMFLOAT3(mCameraPostion.x - 1.0f, mCameraPostion.y, mCameraPostion.z), // -X
		XMFLOAT3(mCameraPostion.x, mCameraPostion.y + 1.0f, mCameraPostion.z), // +Y
		XMFLOAT3(mCameraPostion.x, mCameraPostion.y - 1.0f, mCameraPostion.z), // -Y
		XMFLOAT3(mCameraPostion.x, mCameraPostion.y, mCameraPostion.z + 1.0f), // +Z
		XMFLOAT3(mCameraPostion.x, mCameraPostion.y, mCameraPostion.z - 1.0f)  // -Z
	};

	// 对除 +Y/-Y 之外的所有方向使用世界向上矢量 (0,1,0)。 
	//在这些情况下，我们向下看 +Y 或 -Y,所以我们需要一个不同的“向上”向量。
	XMFLOAT3 Ups[6] =
	{
		XMFLOAT3(0.0f, 1.0f, 0.0f),  // +X
		XMFLOAT3(0.0f, 1.0f, 0.0f),  // -X
		XMFLOAT3(0.0f, 0.0f, -1.0f), // +Y
		XMFLOAT3(0.0f, 0.0f, +1.0f), // -Y
		XMFLOAT3(0.0f, 1.0f, 0.0f),	 // +Z
		XMFLOAT3(0.0f, 1.0f, 0.0f)	 // -Z
	};

	for (int i = 0; i < 6; ++i)
	{
		mCubeMapCamera[i].LookAt(Center, Targets[i], Ups[i]);
		mCubeMapCamera[i].SetLens(0.5f * XM_PI, 1.0f, 0.1f, 1000.0f);
		mCubeMapCamera[i].UpdateViewMatrix();
	}
}

void CubeRenderTarget::BuildCubeMapFacePassCBs()
{
	for (int i = 0; i < 6; ++i)
	{
		CubeMapConstants& CubeFacePassCB = mCubeMapCBV[i];

		XMMATRIX view = mCubeMapCamera[i].GetView();
		XMMATRIX proj = mCubeMapCamera[i].GetProj();

		XMMATRIX viewProj = XMMatrixMultiply(view, proj);
		XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
		XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
		XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

		XMStoreFloat4x4(&CubeFacePassCB.View, XMMatrixTranspose(view));
		XMStoreFloat4x4(&CubeFacePassCB.Proj, XMMatrixTranspose(proj));
		XMStoreFloat4x4(&CubeFacePassCB.ViewProj, XMMatrixTranspose(viewProj));
		CubeFacePassCB.EyePosW = mCubeMapCamera[i].GetPosition3f();
		CubeFacePassCB.RenderTargetSize = XMFLOAT2((float)mWidth, (float)mHeight);
		CubeFacePassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mWidth, 1.0f / mHeight);
		assert(GlobleSRVIndex::gSkyCubeSRVIndex >= 0);
		CubeFacePassCB.mSkyCubeIndex = GlobleSRVIndex::gSkyCubeSRVIndex;
	}
}


void CubeRenderTarget::BuildDescriptors()
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = mFormat;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.TextureCube.MostDetailedMip = 0;
	srvDesc.TextureCube.MipLevels = 1;
	srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;

	// Create SRV to the entire cube map resource.
	mCubeSrv = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	g_Device->CreateShaderResourceView(mCubeMap.GetResource(), &srvDesc, mCubeSrv);

	mSrvIndex = AddToGlobalCubeSRVDescriptor(mCubeSrv);

	// Create RTV to each cube face.
	mRenderTargetDes.resize((size_t)6 * mMipMap);
	for (UINT i = 0; i < 6; ++i)
	{
		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc;
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
		rtvDesc.Format = mFormat;

		rtvDesc.Texture2DArray.PlaneSlice = 0;

		// Render target to ith element.
		rtvDesc.Texture2DArray.FirstArraySlice = i;

		// Only view one element of the array.
		rtvDesc.Texture2DArray.ArraySize = 1;

		for (UINT mip = 0; mip < mMipMap; ++mip)
		{
			rtvDesc.Texture2DArray.MipSlice = mip;
			// Create RTV to ith cube map face.
			UINT DesIndex = i * mMipMap + mip;
			mRenderTargetDes[DesIndex] = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
			g_Device->CreateRenderTargetView(mCubeMap.GetResource(), &rtvDesc, mRenderTargetDes[DesIndex]);
		}
	}
}

void CubeRenderTarget::BuildResource()
{
	mCubeMap.CreateArray(L"Cube Map", mWidth, mHeight, mMipMap, 6, mFormat);
}

void CubeRenderTarget::OnResize(UINT newWidth, UINT newHeight)
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

D3D12_VIEWPORT CubeRenderTarget::Viewport() const
{
	return mViewport;
}

D3D12_RECT CubeRenderTarget::ScissorRect() const
{
	return mScissorRect;
}

