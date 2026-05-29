#include "ShadowMap.h"
#include "Graphics/GraphicsCore.h"
#include "Engine/Camera.h"

//shader
#include "CompiledShaders/ModelShadowVS.h"
#include "CompiledShaders/ModelShadowPS.h"

using namespace GlareEngine;

DXGI_FORMAT ShadowMap::mFormat = DXGI_FORMAT_D32_FLOAT;

ShadowMap::ShadowMap(XMFLOAT3 LightDirection, UINT width, UINT height)
	:mBaseLightDirection(LightDirection),
	mRotatedLightDirection(0, 0, 0),
	mLightPosW(0, 0, 0)
{
	mWidth = width;
	mHeight = height;

	mViewport = { 0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f };
	mScissorRect = { 0, 0, (int)width, (int)height };

	for (UINT cascadeIndex = 0; cascadeIndex < CSM_CASCADE_COUNT; ++cascadeIndex)
	{
		std::wstring bufferName = L"CSM Shadow Map " + std::to_wstring(cascadeIndex);
		mCascadeShadowBuffers[cascadeIndex].Create(bufferName, width, height, mFormat);
		mShadowMapIndices[cascadeIndex] = AddToGlobalTextureSRVDescriptor(mCascadeShadowBuffers[cascadeIndex].GetSRV());
		mCascadeViews[cascadeIndex] = MathHelper::Identity4x4();
		mCascadeProjs[cascadeIndex] = MathHelper::Identity4x4();
		mCascadeViewProjs[cascadeIndex] = MathHelper::Identity4x4();
		mCascadeShadowTransforms[cascadeIndex] = MathHelper::Identity4x4();
		mCascadeConstantBuffers[cascadeIndex].gShadowViewProj = MathHelper::Identity4x4();
	}
	mShadowMapIndex = mShadowMapIndices[0];

	mSceneBounds.Center = XMFLOAT3(0.0f, 0.0f, 0.0f);
	mSceneBounds.Radius = 700.0f;

	InitMaterial();
}

void ShadowMap::Update(float DeltaTime)
{
	Update(DeltaTime, nullptr);
}

void ShadowMap::Update(float DeltaTime, const Camera* camera)
{
	// Recompute shadow transform every frame to track camera movement.
	XMMATRIX R = XMMatrixRotationY(mLightRotationAngle);
	XMVECTOR lightDir = XMLoadFloat3(&mBaseLightDirection);
	lightDir = XMVector3Normalize(XMVector3TransformNormal(lightDir, R));
	XMStoreFloat3(&mRotatedLightDirection, lightDir);

	// Transform NDC space [-1,+1]^2 to texture space [0,1]^2.
	XMMATRIX toTextureSpace(
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, -0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f);

	XMVECTOR lightUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	if (fabsf(XMVectorGetX(XMVector3Dot(lightDir, lightUp))) > 0.95f)
	{
		lightUp = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	}

	if (camera == nullptr)
	{
		XMVECTOR lightPos = -2.0f * mSceneBounds.Radius * lightDir;
		XMVECTOR targetPos = XMLoadFloat3(&mSceneBounds.Center);
		XMMATRIX lightView = XMMatrixLookAtLH(lightPos, targetPos, lightUp);

		XMStoreFloat3(&mLightPosW, lightPos);

		XMFLOAT3 sphereCenterLS;
		XMStoreFloat3(&sphereCenterLS, XMVector3TransformCoord(targetPos, lightView));

		float l = sphereCenterLS.x - mSceneBounds.Radius;
		float b = sphereCenterLS.y - mSceneBounds.Radius;
		float n = sphereCenterLS.z - mSceneBounds.Radius;
		float r = sphereCenterLS.x + mSceneBounds.Radius;
		float t = sphereCenterLS.y + mSceneBounds.Radius;
		float f = sphereCenterLS.z + mSceneBounds.Radius;

		mLightNearZ = n;
		mLightFarZ = f;
		XMMATRIX lightProj = XMMatrixOrthographicOffCenterLH(l, r, b, t, n, f);
		XMMATRIX shadowViewProj = lightView * lightProj;
		XMMATRIX shadowTransform = shadowViewProj * toTextureSpace;

		XMStoreFloat4x4(&mLightView, lightView);
		XMStoreFloat4x4(&mLightProj, lightProj);
		XMStoreFloat4x4(&mLightViewProj, shadowViewProj);
		XMStoreFloat4x4(&mShadowTransform, shadowTransform);
		XMStoreFloat4x4(&mConstantBuffer.gShadowViewProj, XMMatrixTranspose(shadowViewProj));

		for (UINT cascadeIndex = 0; cascadeIndex < CSM_CASCADE_COUNT; ++cascadeIndex)
		{
			mCascadeViews[cascadeIndex] = mLightView;
			mCascadeProjs[cascadeIndex] = mLightProj;
			mCascadeViewProjs[cascadeIndex] = mLightViewProj;
			mCascadeShadowTransforms[cascadeIndex] = mShadowTransform;
			mCascadeConstantBuffers[cascadeIndex] = mConstantBuffer;
		}
		mCascadeSplits = XMFLOAT4(mSceneBounds.Radius, mSceneBounds.Radius, mSceneBounds.Radius, mSceneBounds.Radius);
		return;
	}

	const float nearZ = camera->GetNearZ() > 0.1f ? camera->GetNearZ() : 0.1f;
	const float farZ = camera->GetFarZ();
	const float splitLambda = 0.75f;
	float splitDistances[CSM_CASCADE_COUNT] = {};
	float previousSplit = nearZ;

	XMVECTOR cameraPos = camera->GetPosition();
	XMVECTOR cameraRight = XMVector3Normalize(camera->GetRight());
	XMVECTOR cameraUp = XMVector3Normalize(camera->GetUp());
	XMVECTOR cameraLook = XMVector3Normalize(camera->GetLook());
	const float tanHalfFovY = tanf(camera->GetFovY() * 0.5f);
	const float aspect = camera->GetAspect();

	for (UINT cascadeIndex = 0; cascadeIndex < CSM_CASCADE_COUNT; ++cascadeIndex)
	{
		float p = static_cast<float>(cascadeIndex + 1) / static_cast<float>(CSM_CASCADE_COUNT);
		float logarithmic = nearZ * powf(farZ / nearZ, p);
		float uniform = nearZ + (farZ - nearZ) * p;
		float splitDistance = splitLambda * logarithmic + (1.0f - splitLambda) * uniform;
		splitDistances[cascadeIndex] = splitDistance;

		float cascadeNear = previousSplit;
		float cascadeFar = splitDistance;
		previousSplit = splitDistance;

		std::array<XMVECTOR, 8> frustumCorners;
		auto buildCorners = [&](float distance, UINT offset)
		{
			float halfHeight = tanHalfFovY * distance;
			float halfWidth = halfHeight * aspect;
			XMVECTOR center = cameraPos + cameraLook * distance;
			frustumCorners[offset + 0] = center - cameraRight * halfWidth + cameraUp * halfHeight;
			frustumCorners[offset + 1] = center + cameraRight * halfWidth + cameraUp * halfHeight;
			frustumCorners[offset + 2] = center - cameraRight * halfWidth - cameraUp * halfHeight;
			frustumCorners[offset + 3] = center + cameraRight * halfWidth - cameraUp * halfHeight;
		};
		buildCorners(cascadeNear, 0);
		buildCorners(cascadeFar, 4);

		XMVECTOR cascadeCenter = XMVectorZero();
		for (const XMVECTOR& corner : frustumCorners)
		{
			cascadeCenter += corner;
		}
		cascadeCenter /= 8.0f;

		float cascadeRadius = 0.0f;
		for (const XMVECTOR& corner : frustumCorners)
		{
			float cornerDistance = XMVectorGetX(XMVector3Length(corner - cascadeCenter));
			cascadeRadius = cascadeRadius > cornerDistance ? cascadeRadius : cornerDistance;
		}
		cascadeRadius = ceilf(cascadeRadius * 16.0f) / 16.0f;

		XMVECTOR lightPos = cascadeCenter - lightDir * (cascadeRadius + mSceneBounds.Radius);
		XMMATRIX lightView = XMMatrixLookAtLH(lightPos, cascadeCenter, lightUp);

		XMFLOAT3 minLS(FLT_MAX, FLT_MAX, FLT_MAX);
		XMFLOAT3 maxLS(-FLT_MAX, -FLT_MAX, -FLT_MAX);
		for (const XMVECTOR& corner : frustumCorners)
		{
			XMFLOAT3 cornerLS;
			XMStoreFloat3(&cornerLS, XMVector3TransformCoord(corner, lightView));
			minLS.x = minLS.x < cornerLS.x ? minLS.x : cornerLS.x;
			minLS.y = minLS.y < cornerLS.y ? minLS.y : cornerLS.y;
			minLS.z = minLS.z < cornerLS.z ? minLS.z : cornerLS.z;
			maxLS.x = maxLS.x > cornerLS.x ? maxLS.x : cornerLS.x;
			maxLS.y = maxLS.y > cornerLS.y ? maxLS.y : cornerLS.y;
			maxLS.z = maxLS.z > cornerLS.z ? maxLS.z : cornerLS.z;
		}

		float width = maxLS.x - minLS.x;
		float height = maxLS.y - minLS.y;
		float dimension = width > height ? width : height;
		float centerX = (minLS.x + maxLS.x) * 0.5f;
		float centerY = (minLS.y + maxLS.y) * 0.5f;
		float texelSize = dimension / static_cast<float>(mWidth);
		centerX = floorf(centerX / texelSize) * texelSize;
		centerY = floorf(centerY / texelSize) * texelSize;

		float l = centerX - dimension * 0.5f;
		float r = centerX + dimension * 0.5f;
		float b = centerY - dimension * 0.5f;
		float t = centerY + dimension * 0.5f;
		float n = minLS.z - mSceneBounds.Radius;
		float f = maxLS.z + mSceneBounds.Radius;

		XMMATRIX lightProj = XMMatrixOrthographicOffCenterLH(l, r, b, t, n, f);
		XMMATRIX shadowViewProj = lightView * lightProj;
		XMMATRIX shadowTransform = shadowViewProj * toTextureSpace;

		XMStoreFloat4x4(&mCascadeViews[cascadeIndex], lightView);
		XMStoreFloat4x4(&mCascadeProjs[cascadeIndex], lightProj);
		XMStoreFloat4x4(&mCascadeViewProjs[cascadeIndex], shadowViewProj);
		XMStoreFloat4x4(&mCascadeShadowTransforms[cascadeIndex], shadowTransform);
		XMStoreFloat4x4(&mCascadeConstantBuffers[cascadeIndex].gShadowViewProj, XMMatrixTranspose(shadowViewProj));

		if (cascadeIndex == 0)
		{
			XMStoreFloat3(&mLightPosW, lightPos);
			mLightNearZ = n;
			mLightFarZ = f;
			mLightView = mCascadeViews[cascadeIndex];
			mLightProj = mCascadeProjs[cascadeIndex];
			mLightViewProj = mCascadeViewProjs[cascadeIndex];
			mShadowTransform = mCascadeShadowTransforms[cascadeIndex];
			mConstantBuffer = mCascadeConstantBuffers[cascadeIndex];
		}
	}

	mCascadeSplits = XMFLOAT4(splitDistances[0], splitDistances[1], splitDistances[2], splitDistances[3]);
}

UINT ShadowMap::Width()const
{
	return mWidth;
}

UINT ShadowMap::Height()const
{
	return mHeight;
}

D3D12_VIEWPORT ShadowMap::Viewport()const
{
	return mViewport;
}

D3D12_RECT ShadowMap::ScissorRect()const
{
	return mScissorRect;
}

void ShadowMap::Draw(GraphicsContext& Context, vector<RenderObject*> RenderObjects)
{
	for (UINT cascadeIndex = 0; cascadeIndex < CSM_CASCADE_COUNT; ++cascadeIndex)
	{
		DrawCascade(Context, RenderObjects, cascadeIndex);
	}
}

void ShadowMap::DrawCascade(GraphicsContext& Context, vector<RenderObject*> RenderObjects, UINT cascadeIndex)
{
	cascadeIndex = ClampCSMCascadeIndex(cascadeIndex);
	mActiveCascadeIndex = cascadeIndex;
	ShadowBuffer& shadowBuffer = mCascadeShadowBuffers[cascadeIndex];

	Context.SetViewportAndScissor(mViewport, mScissorRect);
	Context.TransitionResource(shadowBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);
	Context.ClearDepth(shadowBuffer);
	Context.SetDepthStencilTarget(GetDSV(cascadeIndex));
	Context.SetDynamicConstantBufferView((int)RootSignatureType::eCommonConstantBuffer, sizeof(ShadowConstantBuffer), &mCascadeConstantBuffers[cascadeIndex]);

	for (auto& object : RenderObjects)
	{
		if (object->GetVisible() && object->GetShadowRenderFlag())
		{
			object->CacheShadowVP(mCascadeViewProjs[cascadeIndex]);
			Context.PIXBeginEvent(object->GetName().c_str());
			if (object->GetMaskFlag())
			{
				object->DrawShadow(Context, &mMaskShadowMaterial->GetGraphicsPSO());
			}
			else
			{
				object->DrawShadow(Context, &mShadowMaterial->GetGraphicsPSO());
			}
			Context.PIXEndEvent();
		}
	}
	Context.TransitionResource(shadowBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, true);
}

void ShadowMap::InitMaterial()
{
	mShadowMaterial= RenderMaterialManager::GetInstance().GetMaterial("Shadow Material");
	mMaskShadowMaterial = RenderMaterialManager::GetInstance().GetMaterial("Mask Shadow Material");
	if (!mShadowMaterial->IsInitialized)
	{
		mShadowMaterial->BindPSOCreateFunc([&](const PSOCommonProperty CommonProperty) {
			GraphicsPSO& ShadowMaterialPSO = mShadowMaterial->GetGraphicsPSO();
			ShadowMaterialPSO.SetRootSignature(*CommonProperty.pRootSignature);
			ShadowMaterialPSO.SetRasterizerState(RasterizerShadowCW);
			ShadowMaterialPSO.SetBlendState(BlendDisable);
			ShadowMaterialPSO.SetDepthStencilState(DepthStateReadWrite);
			ShadowMaterialPSO.SetSampleMask(0xFFFFFFFF);
			ShadowMaterialPSO.SetInputLayout((UINT)InputLayout::PosNormalTangentTexc.size(), InputLayout::PosNormalTangentTexc.data());
			ShadowMaterialPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
			ShadowMaterialPSO.SetVertexShader(g_pModelShadowVS, sizeof(g_pModelShadowVS));
			ShadowMaterialPSO.SetPixelShader(g_pModelShadowPS, sizeof(g_pModelShadowPS));
			ShadowMaterialPSO.SetRenderTargetFormats(0, &DefaultHDRColorFormat, DXGI_FORMAT_D32_FLOAT);
			ShadowMaterialPSO.Finalize();
			});

		mShadowMaterial->BindPSORuntimeModifyFunc([&]() {
			RuntimePSOManager::Get().RegisterPSO(&mShadowMaterial->GetGraphicsPSO(), GET_SHADER_PATH("Shadow/ModelShadowVS.hlsl"), D3D12_SHVER_VERTEX_SHADER); });
	}

	if (!mMaskShadowMaterial->IsInitialized)
	{
		mMaskShadowMaterial->BindPSOCreateFunc([&](const PSOCommonProperty CommonProperty) {
			GraphicsPSO& MaskShadowMaterialPSO = mMaskShadowMaterial->GetGraphicsPSO();
			MaskShadowMaterialPSO.SetRootSignature(*CommonProperty.pRootSignature);
			MaskShadowMaterialPSO.SetRasterizerState(RasterizerShadowCW);
			MaskShadowMaterialPSO.SetBlendState(BlendDisable);
			MaskShadowMaterialPSO.SetDepthStencilState(DepthStateReadWrite);
			MaskShadowMaterialPSO.SetSampleMask(0xFFFFFFFF);
			MaskShadowMaterialPSO.SetInputLayout((UINT)InputLayout::PosNormalTangentTexc.size(), InputLayout::PosNormalTangentTexc.data());
			MaskShadowMaterialPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
			MaskShadowMaterialPSO.SetVertexShader(g_pModelShadowVS, sizeof(g_pModelShadowVS));
			MaskShadowMaterialPSO.SetPixelShader(g_pModelShadowPS, sizeof(g_pModelShadowPS));
			MaskShadowMaterialPSO.SetRenderTargetFormats(0, &DefaultHDRColorFormat, DXGI_FORMAT_D32_FLOAT);
			MaskShadowMaterialPSO.Finalize();
			});

		mShadowMaterial->BindPSORuntimeModifyFunc([&]() {
			RuntimePSOManager::Get().RegisterPSO(&mMaskShadowMaterial->GetGraphicsPSO(), GET_SHADER_PATH("Shadow/ModelShadowVS.hlsl"), D3D12_SHVER_VERTEX_SHADER);
			RuntimePSOManager::Get().RegisterPSO(&mMaskShadowMaterial->GetGraphicsPSO(), GET_SHADER_PATH("Shadow/ModelShadowPS.hlsl"), D3D12_SHVER_PIXEL_SHADER); });
	}

	mShadowMaterial->IsInitialized = true;
	mMaskShadowMaterial->IsInitialized = true;
}

void ShadowMap::BuildPSO(const PSOCommonProperty CommonProperty)
{
	
}

void ShadowMap::OnResize(UINT newWidth, UINT newHeight)
{
	mWidth = newWidth;
	mHeight = newHeight;
	mViewport = { 0.0f, 0.0f, (float)newWidth, (float)newHeight, 0.0f, 1.0f };
	mScissorRect = { 0, 0, (int)newWidth, (int)newHeight };

	for (UINT cascadeIndex = 0; cascadeIndex < CSM_CASCADE_COUNT; ++cascadeIndex)
	{
		std::wstring bufferName = L"CSM Shadow Map " + std::to_wstring(cascadeIndex);
		mCascadeShadowBuffers[cascadeIndex].Create(bufferName, newWidth, newHeight, mFormat);
		if (g_TextureSRV.size() > mShadowMapIndices[cascadeIndex])
		{
			g_TextureSRV[mShadowMapIndices[cascadeIndex]] = mCascadeShadowBuffers[cascadeIndex].GetSRV();
		}
	}
}
