#pragma once
#include "Engine/EngineUtility.h"
#include "Graphics/ShadowBuffer.h"
#include "Misc/RenderObject.h"
#include "Engine/RenderMaterial.h"

struct ShadowConstantBuffer 
{
	XMFLOAT4X4 gShadowViewProj = MathHelper::Identity4x4();
};


class ShadowMap
	: public RenderObject
{
public:
	ShadowMap(XMFLOAT3 LightDirection, UINT width, UINT height);

	ShadowMap(const ShadowMap& rhs) = delete;
	ShadowMap& operator=(const ShadowMap& rhs) = delete;
	~ShadowMap() = default;

	UINT Width()const;
	UINT Height()const;
	D3D12_VIEWPORT Viewport()const;
	D3D12_RECT ScissorRect()const;

	void Draw(GraphicsContext& Context, vector<RenderObject*> RenderObjects);

	virtual void DrawUI() {}

	virtual void InitMaterial();

	static void BuildPSO(const PSOCommonProperty CommonProperty);

	void OnResize(UINT newWidth, UINT newHeight);

	void SetSceneBoundCenter(XMFLOAT3 center) { mSceneBounds.Center = center; }
	void SetSceneBoundRadius(float Radius) { mSceneBounds.Radius = Radius; }

	int GetShadowMapIndex() { return mShadowMapIndex; }

	virtual void Update(float DeltaTime);

	XMFLOAT3 GetShadowedLightDir()const { return mBaseLightDirection; }

	ShadowBuffer& GetShadowBuffer() { return mShadowBuffer; }

	XMFLOAT4X4 GetView() const { return mLightView; }

	XMFLOAT4X4 GetProj() const { return mLightProj; }

	XMFLOAT4X4 GetViewProj() const { return mConstantBuffer.gShadowViewProj; }

	XMFLOAT4X4 GetViewProjNoTranspose() const { return mLightViewProj; }

	XMFLOAT4X4 GetShadowTransform()const { return mShadowTransform; }
private:
	D3D12_CPU_DESCRIPTOR_HANDLE GetDSV()const { return mShadowBuffer.GetDSV(); }

	D3D12_CPU_DESCRIPTOR_HANDLE GetSRV()const { return mShadowBuffer.GetSRV(); }
public:
	static DXGI_FORMAT mFormat;

private:
	RenderMaterial* mShadowMaterial = nullptr;
	RenderMaterial* mMaskShadowMaterial = nullptr;

	D3D12_VIEWPORT mViewport;
	D3D12_RECT mScissorRect;

	UINT mWidth = 0;
	UINT mHeight = 0;

	ShadowBuffer mShadowBuffer;

	bool IsShadowTransformed = true;

	// Light influence bounds
	DirectX::BoundingSphere mSceneBounds;
	// Light projection depth range
	float mLightNearZ = 0.0f;
	float mLightFarZ = 0.0f;
	// Light world-space position
	XMFLOAT3 mLightPosW;
	// Shadow map transform matrices
	XMFLOAT4X4 mLightView = MathHelper::Identity4x4();
	XMFLOAT4X4 mLightProj = MathHelper::Identity4x4();
	XMFLOAT4X4 mLightViewProj = MathHelper::Identity4x4();
	XMFLOAT4X4 mShadowTransform = MathHelper::Identity4x4();
	// Light rotation angle
	float mLightRotationAngle = 0.0f;
	// Base light direction
	XMFLOAT3 mBaseLightDirection;
	// Rotated light direction
	XMFLOAT3 mRotatedLightDirection;
	// Constant buffer
	ShadowConstantBuffer mConstantBuffer;
	int mShadowMapIndex = 0;
};

