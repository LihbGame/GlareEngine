#include "ShadowMap.h"
#include "GraphicsCore.h"

//shader
#include "CompiledShaders/ModelShadowVS.h"
#include "CompiledShaders/ModelShadowPS.h"

using namespace GlareEngine;

GraphicsPSO ShadowMap::mShadowPSO;

ShadowMap::ShadowMap(XMFLOAT3 LightDirection, UINT width, UINT height)
	:mBaseLightDirection(LightDirection),
	mRotatedLightDirection(0, 0, 0),
	mLightPosW(0, 0, 0)
{
	mWidth = width;
	mHeight = height;

	mViewport = { 0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f };
	mScissorRect = { 0, 0, (int)width, (int)height };

	//Create shadow buffer
	mShadowBuffer.Create(L"Shadow Map", width, height, mFormat);
	mShadowMapIndex = AddToGlobalTextureSRVDescriptor(mShadowBuffer.GetSRV());

	//由于我们知道场景的构造方式，因此手动估算场景边界球;
	//网格是“最宽的对象”，宽度为20，深度为30.0f，以世界空间原点为中心; 
	//通常，您需要遍历每个世界空间顶点位置并计算边界球体;
	mSceneBounds.Center = XMFLOAT3(0.0f, 0.0f, 0.0f);
	mSceneBounds.Radius = 300.0f;
}


void ShadowMap::Update(float DeltaTime)
{
	// Animate the lights (and hence shadows).
//need light manager but we not have it now .now we just let shadow to manage it.
//we well add it when we coding scene manager.
//mLightRotationAngle += 0.01f * gt.DeltaTime();
	if (IsShadowTransformed)
	{
		XMMATRIX R = XMMatrixRotationY(mLightRotationAngle);
		XMVECTOR lightDir = XMLoadFloat3(&mBaseLightDirection);
		lightDir = XMVector3TransformNormal(lightDir, R);
		XMStoreFloat3(&mRotatedLightDirection, lightDir);

		// Only the first "main" light casts a shadow.
		DirectX::BoundingSphere TempBoundingSphere = this->mSceneBounds;
		XMVECTOR LightPos = -2.0f * this->mSceneBounds.Radius * lightDir;
		XMVECTOR targetPos = XMLoadFloat3(&this->mSceneBounds.Center);
		XMVECTOR lightUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
		XMMATRIX lightView = XMMatrixLookAtLH(LightPos, targetPos, lightUp);

		XMStoreFloat3(&this->mLightPosW, LightPos);

		// Transform bounding sphere to light space.
		XMFLOAT3 sphereCenterLS;
		XMStoreFloat3(&sphereCenterLS, XMVector3TransformCoord(targetPos, lightView));

		//Orthographic frustum in light space encloses scene.
		float l = sphereCenterLS.x - this->mSceneBounds.Radius;
		float b = sphereCenterLS.y - this->mSceneBounds.Radius;
		float n = sphereCenterLS.z - this->mSceneBounds.Radius;
		float r = sphereCenterLS.x + this->mSceneBounds.Radius;
		float t = sphereCenterLS.y + this->mSceneBounds.Radius;
		float f = sphereCenterLS.z + this->mSceneBounds.Radius;

		this->mLightNearZ = n;
		this->mLightFarZ = f;
		XMMATRIX lightProj = XMMatrixOrthographicOffCenterLH(l, r, b, t, n, f);

		// Transform NDC space [-1,+1]^2 to texture space [0,1]^2
		XMMATRIX T(
			0.5f, 0.0f, 0.0f, 0.0f,
			0.0f, -0.5f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.5f, 0.5f, 0.0f, 1.0f);

		XMMATRIX S = lightView * lightProj * T;
		XMStoreFloat4x4(&this->mLightView, lightView);
		XMStoreFloat4x4(&this->mLightProj, lightProj);
		XMStoreFloat4x4(&this->mShadowTransform, S);

		XMMATRIX ShadowViewProj = XMMatrixMultiply(lightView, lightProj);
		XMStoreFloat4x4(&mConstantBuffer.gShadowViewProj, XMMatrixTranspose(ShadowViewProj));

		IsShadowTransformed = false;
	}
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

void ShadowMap::Draw(GraphicsContext& Context,vector<RenderObject*> RenderObjects)
{
	//set Viewport And Scissor
	Context.SetViewportAndScissor(mViewport, mScissorRect);
	//set scene render target & Depth Stencil target
	Context.TransitionResource(mShadowBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);
	Context.ClearDepth(mShadowBuffer);
	Context.SetDepthStencilTarget(GetDSV());
	//set shadow constant buffer
	Context.SetDynamicConstantBufferView((int)RootSignatureType::eCommonConstantBuffer, sizeof(ShadowConstantBuffer), &mConstantBuffer);

	for (auto& object:RenderObjects)
	{
		if (object->GetVisible() && object->GetShadowFlag())
		{
			Context.PIXBeginEvent(object->GetName().c_str());
			object->DrawShadow(Context, &mShadowPSO);
			Context.PIXEndEvent();
		}
	}
	Context.TransitionResource(mShadowBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, true);
}

void ShadowMap::BuildPSO(const PSOCommonProperty CommonProperty)
{
	mShadowPSO.SetRootSignature(*CommonProperty.pRootSignature);
	mShadowPSO.SetRasterizerState(RasterizerShadow);
	mShadowPSO.SetBlendState(BlendDisable);
	mShadowPSO.SetDepthStencilState(DepthStateReadWrite);
	mShadowPSO.SetSampleMask(0xFFFFFFFF);
	mShadowPSO.SetInputLayout((UINT)InputLayout::PosNormalTangentTexc.size(), InputLayout::PosNormalTangentTexc.data());
	mShadowPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	mShadowPSO.SetVertexShader(g_pModelShadowVS, sizeof(g_pModelShadowVS));
	mShadowPSO.SetPixelShader(g_pModelShadowPS, sizeof(g_pModelShadowPS));
	mShadowPSO.SetRenderTargetFormats(0,&DefaultHDRColorFormat, DXGI_FORMAT_D32_FLOAT);
	mShadowPSO.Finalize();
}

void ShadowMap::OnResize(UINT newWidth, UINT newHeight)
{
	//resize
	mShadowBuffer.Create(L"Shadow Map", newWidth, newHeight);
	if (g_TextureSRV.size() > mShadowMapIndex)
	{
		g_TextureSRV[mShadowMapIndex] = mShadowBuffer.GetSRV();
	}
}