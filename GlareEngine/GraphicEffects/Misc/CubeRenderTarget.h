#pragma once
#include "Engine/EngineUtility.h"
#include "Graphics/ColorBuffer.h"
#include "Misc/ConstantBuffer.h"
#include "Engine/Camera.h"
#include "Graphics/DepthBuffer.h"

enum class CubeMapFace : int
{
	PositiveX = 0,
	NegativeX = 1,
	PositiveY = 2,
	NegativeY = 3,
	PositiveZ = 4,
	NegativeZ = 5,
	FaceCount = 6
};

class CubeRenderTarget
{
public:
	CubeRenderTarget(UINT width, UINT height, UINT MipMap = 1, XMFLOAT3 CameraPostion = XMFLOAT3(0.0f, 0.0f, 0.0f), DXGI_FORMAT format = DXGI_FORMAT_R11G11B10_FLOAT);
	~CubeRenderTarget();

	CubeRenderTarget(const CubeRenderTarget& rhs) = delete;
	CubeRenderTarget& operator=(const CubeRenderTarget& rhs) = delete;
public:
	void OnResize(UINT newWidth, UINT newHeight);
	D3D12_VIEWPORT Viewport(UINT mipmap=0)const;
	D3D12_RECT ScissorRect()const;
	ColorBuffer& Resource();
	DepthBuffer& DepthResource() { return mDepthBuffer; }

	D3D12_CPU_DESCRIPTOR_HANDLE SRV();
	D3D12_CPU_DESCRIPTOR_HANDLE RTV(int index);
	D3D12_CPU_DESCRIPTOR_HANDLE DSV() { return mDepthBuffer.GetDSV(); };

	CubeMapConstants GetCubeCameraCBV(int index) { return mCubeMapCBV[index]; }
	int GetSRVIndex() { return mSrvIndex; }

	void UpdateCubeMapPosition(XMFLOAT3 NewPosition);

	void CaptureScene(Scene& scene,GraphicsContext& Context);
private:
	void BuildDescriptors();
	void BuildResource();
	void BuildCubeFaceCamera();
	void BuildCubeMapFacePassCBs();
private:
	UINT mWidth = 0;
	UINT mHeight = 0;
	UINT mMipMap = 0;
	DXGI_FORMAT mFormat;
	XMFLOAT3 mCameraPostion;

	vector<D3D12_VIEWPORT> mViewport;
	D3D12_RECT mScissorRect;

	vector<D3D12_CPU_DESCRIPTOR_HANDLE> mRenderTargetDes;
	D3D12_CPU_DESCRIPTOR_HANDLE mCubeSrv;

	DepthBuffer	 mDepthBuffer;
	ColorBuffer  mCubeMap;
	unique_ptr<Camera> mCubeMapCamera[6];
	CubeMapConstants mCubeMapCBV[6];

	int mSrvIndex = 0;
};

