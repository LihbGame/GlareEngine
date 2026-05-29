#pragma once
#include "Engine/EngineUtility.h"
#include "Graphics/ShadowBuffer.h"
#include "Misc/RenderObject.h"
#include "Engine/RenderMaterial.h"
#include <array>

class Camera;

constexpr UINT CSM_CASCADE_COUNT = 4;

inline UINT ClampCSMCascadeIndex(UINT cascadeIndex)
{
	return cascadeIndex < CSM_CASCADE_COUNT ? cascadeIndex : CSM_CASCADE_COUNT - 1;
}

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

	UINT GetCascadeCount() const { return CSM_CASCADE_COUNT; }
	void SetActiveCascadeIndex(UINT cascadeIndex) { mActiveCascadeIndex = ClampCSMCascadeIndex(cascadeIndex); }
	UINT GetActiveCascadeIndex() const { return mActiveCascadeIndex; }

	void Draw(GraphicsContext& Context, vector<RenderObject*> RenderObjects);

	virtual void DrawUI() {}

	virtual void InitMaterial();

	static void BuildPSO(const PSOCommonProperty CommonProperty);

	void OnResize(UINT newWidth, UINT newHeight);

	void SetSceneBoundCenter(XMFLOAT3 center) { mSceneBounds.Center = center; }
	void SetSceneBoundRadius(float Radius) { mSceneBounds.Radius = Radius; }

	int GetShadowMapIndex() { return mShadowMapIndex; }
	int GetShadowMapIndex(UINT cascadeIndex) const { return mShadowMapIndices[ClampCSMCascadeIndex(cascadeIndex)]; }
	const std::array<int, CSM_CASCADE_COUNT>& GetShadowMapIndices() const { return mShadowMapIndices; }

	virtual void Update(float DeltaTime);
	void Update(float DeltaTime, const Camera* camera);

	XMFLOAT3 GetShadowedLightDir()const { return mBaseLightDirection; }

	ShadowBuffer& GetShadowBuffer() { return mCascadeShadowBuffers[0]; }
	ShadowBuffer& GetShadowBuffer(UINT cascadeIndex) { return mCascadeShadowBuffers[ClampCSMCascadeIndex(cascadeIndex)]; }

	XMFLOAT4X4 GetView() const { return mCascadeViews[mActiveCascadeIndex]; }
	XMFLOAT4X4 GetView(UINT cascadeIndex) const { return mCascadeViews[ClampCSMCascadeIndex(cascadeIndex)]; }

	XMFLOAT4X4 GetProj() const { return mCascadeProjs[mActiveCascadeIndex]; }
	XMFLOAT4X4 GetProj(UINT cascadeIndex) const { return mCascadeProjs[ClampCSMCascadeIndex(cascadeIndex)]; }

	XMFLOAT4X4 GetViewProj() const { return mCascadeConstantBuffers[mActiveCascadeIndex].gShadowViewProj; }
	XMFLOAT4X4 GetViewProj(UINT cascadeIndex) const { return mCascadeConstantBuffers[ClampCSMCascadeIndex(cascadeIndex)].gShadowViewProj; }

	XMFLOAT4X4 GetViewProjNoTranspose() const { return mCascadeViewProjs[mActiveCascadeIndex]; }
	XMFLOAT4X4 GetViewProjNoTranspose(UINT cascadeIndex) const { return mCascadeViewProjs[ClampCSMCascadeIndex(cascadeIndex)]; }

	XMFLOAT4X4 GetShadowTransform()const { return mCascadeShadowTransforms[mActiveCascadeIndex]; }
	XMFLOAT4X4 GetShadowTransform(UINT cascadeIndex) const { return mCascadeShadowTransforms[ClampCSMCascadeIndex(cascadeIndex)]; }
	XMFLOAT4 GetCascadeSplits() const { return mCascadeSplits; }
private:
	D3D12_CPU_DESCRIPTOR_HANDLE GetDSV(UINT cascadeIndex)const { return mCascadeShadowBuffers[ClampCSMCascadeIndex(cascadeIndex)].GetDSV(); }

	D3D12_CPU_DESCRIPTOR_HANDLE GetSRV(UINT cascadeIndex)const { return mCascadeShadowBuffers[ClampCSMCascadeIndex(cascadeIndex)].GetSRV(); }

	void DrawCascade(GraphicsContext& Context, vector<RenderObject*> RenderObjects, UINT cascadeIndex);
public:
	static DXGI_FORMAT mFormat;

private:
	RenderMaterial* mShadowMaterial = nullptr;
	RenderMaterial* mMaskShadowMaterial = nullptr;

	D3D12_VIEWPORT mViewport;
	D3D12_RECT mScissorRect;

	UINT mWidth = 0;
	UINT mHeight = 0;

	std::array<ShadowBuffer, CSM_CASCADE_COUNT> mCascadeShadowBuffers;
	std::array<int, CSM_CASCADE_COUNT> mShadowMapIndices = {};

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
	std::array<XMFLOAT4X4, CSM_CASCADE_COUNT> mCascadeViews = {};
	std::array<XMFLOAT4X4, CSM_CASCADE_COUNT> mCascadeProjs = {};
	std::array<XMFLOAT4X4, CSM_CASCADE_COUNT> mCascadeViewProjs = {};
	std::array<XMFLOAT4X4, CSM_CASCADE_COUNT> mCascadeShadowTransforms = {};
	std::array<ShadowConstantBuffer, CSM_CASCADE_COUNT> mCascadeConstantBuffers = {};
	XMFLOAT4 mCascadeSplits = { 0.0f, 0.0f, 0.0f, 0.0f };
	UINT mActiveCascadeIndex = 0;
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

