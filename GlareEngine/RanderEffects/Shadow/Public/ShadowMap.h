#pragma once

#include "L3DUtil.h"
#include "L3DGameTimer.h"
class L3DTextureManage;
class ShadowMap
{
public:
	ShadowMap(ID3D12Device* device,
		UINT width, UINT height, L3DTextureManage* TextureManage);

	ShadowMap(const ShadowMap& rhs) = delete;
	ShadowMap& operator=(const ShadowMap& rhs) = delete;
	~ShadowMap() = default;

	UINT Width()const;
	UINT Height()const;
	ID3D12Resource* Resource();
	CD3DX12_CPU_DESCRIPTOR_HANDLE Dsv()const;

	D3D12_VIEWPORT Viewport()const;
	D3D12_RECT ScissorRect()const;

	void BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDsv);

	void OnResize(UINT newWidth, UINT newHeight);

	void SetSceneBoundCenter(XMFLOAT3 center) { mSceneBounds.Center = center; }

	void FillSRVDescriptorHeap(int* SRVIndex,
		CD3DX12_CPU_DESCRIPTOR_HANDLE* hDescriptor,
		D3D12_CPU_DESCRIPTOR_HANDLE DSVCPUStart,
		int DsvDescriptorSize);

	int GetShadowMapIndex() { return mShadowMapIndex; }
private:
	void BuildDescriptors();
	void BuildResource();

private:

	ID3D12Device* md3dDevice = nullptr;

	D3D12_VIEWPORT mViewport;
	D3D12_RECT mScissorRect;

	UINT mWidth = 0;
	UINT mHeight = 0;
	DXGI_FORMAT mFormat = DXGI_FORMAT_R32_TYPELESS;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhCpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhCpuDsv;
	//shadow map 用于DSV和SRV
	Microsoft::WRL::ComPtr<ID3D12Resource> mShadowMap = nullptr;

	bool IsShadowTransformed = true;
public:
	void UpdateShadowTransform(const GameTimer& gt);

	//光照的影响范围
	DirectX::BoundingSphere mSceneBounds;
	//光照投影近远面z值
	float mLightNearZ = 0.0f;
	float mLightFarZ = 0.0f;
	//光的世界位置
	XMFLOAT3 mLightPosW;
	//用于shadow map的转换矩阵
	XMFLOAT4X4 mLightView = MathHelper::Identity4x4();
	XMFLOAT4X4 mLightProj = MathHelper::Identity4x4();
	XMFLOAT4X4 mShadowTransform = MathHelper::Identity4x4();
	//光的旋转角度(可实时变化)
	float mLightRotationAngle = 0.0f;
	//基础光方向
	XMFLOAT3 mBaseLightDirections[3] = {
		XMFLOAT3(0.57735f, -0.57735f, 0.57735f),
		XMFLOAT3(-0.57735f, -0.57735f, 0.57735f),
		XMFLOAT3(0.0f, -0.707f, -0.707f)
	};
	//旋转后光的方向
	XMFLOAT3 mRotatedLightDirections[3];

	L3DTextureManage* pTextureManage = nullptr;

	int mShadowMapIndex;
};
