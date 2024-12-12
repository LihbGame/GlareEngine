#include "CubeRenderTarget.h"
#include "Graphics/GraphicsCore.h"
#include "Engine/EngineConfig.h"
#include "Graphics/Render.h"
#include "Model/MeshRenderPass.h"
#include "Engine/Scene.h"
#include "Engine/EngineProfiling.h"
#include "PostProcessing/SSAO.h"
#include "InstanceModel/glTFInstanceModel.h"
#include "Misc/LightManager.h"
#include "EngineGUI.h"
#include "Sky/Sky.h"

using namespace GlareEngine::Render;


CubeRenderTarget::CubeRenderTarget(UINT width, UINT height,UINT mipmap, XMFLOAT3 CameraPostion, DXGI_FORMAT format)
	:mWidth(width),
	mHeight(height),
	mMipMap(mipmap),
	mCameraPostion(CameraPostion),
	mFormat(format)
{
	for (UINT MipIndex = 0; MipIndex < mMipMap; ++MipIndex)
	{
		mViewport.push_back({ 0.0f, 0.0f, (float)(width>>MipIndex), (float)(height >> MipIndex), 0.0f, 1.0f });
	}
	mScissorRect = { 0, 0, (int)width, (int)height };

	BuildResource();
	BuildDescriptors();
	BuildCubeFaceCamera();
	BuildCubeMapFacePassCBs();
}

CubeRenderTarget::~CubeRenderTarget()
{
	mCubeMap.Destroy();
	mDepthBuffer.Destroy();
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
		mCubeMapCamera[i] = make_unique<Camera>(true);
		mCubeMapCamera[i]->LookAt(Center, Targets[i], Ups[i]);
		mCubeMapCamera[i]->SetLens(0.5f * XM_PI, 1.0f, 0.1f, 1000.0f);
		mCubeMapCamera[i]->UpdateViewMatrix();
	}
}

void CubeRenderTarget::BuildCubeMapFacePassCBs()
{
	for (int i = 0; i < 6; ++i)
	{
		CubeMapConstants& CubeFacePassCB = mCubeMapCBV[i];

		XMMATRIX view = mCubeMapCamera[i]->GetView();
		XMMATRIX proj = mCubeMapCamera[i]->GetProj();

		XMMATRIX viewProj = XMMatrixMultiply(view, proj);

		XMVECTOR Determinant = XMMatrixDeterminant(view);
		XMMATRIX invView = XMMatrixInverse(&Determinant, view);
		Determinant = XMMatrixDeterminant(proj);
		XMMATRIX invProj = XMMatrixInverse(&Determinant, proj);
		Determinant = XMMatrixDeterminant(viewProj);
		XMMATRIX invViewProj = XMMatrixInverse(&Determinant, viewProj);

		XMStoreFloat4x4(&CubeFacePassCB.View, XMMatrixTranspose(view));
		XMStoreFloat4x4(&CubeFacePassCB.Proj, XMMatrixTranspose(proj));
		XMStoreFloat4x4(&CubeFacePassCB.ViewProj, XMMatrixTranspose(viewProj));
		CubeFacePassCB.EyePosW = mCubeMapCamera[i]->GetPosition3f();
		CubeFacePassCB.RenderTargetSize = XMFLOAT2((float)mWidth, (float)mHeight);
		CubeFacePassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mWidth, 1.0f / mHeight);
		assert(GlobleSRVIndex::gSkyCubeSRVIndex >= 0);
		CubeFacePassCB.mSkyCubeIndex = GlobleSRVIndex::gSkyCubeSRVIndex;
	}
}

void CubeRenderTarget::UpdateCubeMapPosition(XMFLOAT3 NewPosition)
{
	if (Vector3(NewPosition) != Vector3(mCameraPostion))
	{
		mCameraPostion = NewPosition;
		BuildCubeFaceCamera();
		BuildCubeMapFacePassCBs();
	}
}

void CubeRenderTarget::CaptureScene(Scene& scene, GraphicsContext& Context)
{
	Context.SetRootSignature(*scene.GetRootSignature());
    //Set Material Data
	const vector<MaterialConstant>& MaterialData = MaterialManager::GetMaterialInstance()->GetMaterialsConstantBuffer();
	Context.SetDynamicSRV((int)RootSignatureType::eMaterialConstantData, sizeof(MaterialConstant) * MaterialData.size(), MaterialData.data());

	Context.TransitionResource(mCubeMap, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
	Context.ClearRenderTarget(mCubeMap);

	SceneView sceneView= scene.GetSceneView();

	sceneView.mMainConstants.RenderTargetSize = XMFLOAT2(mWidth,mHeight);
	sceneView.mMainConstants.InvRenderTargetSize = XMFLOAT2(1.0f / mWidth, 1.0f / mHeight);

	sceneView.mMainConstants.TileCount[0] = Math::DivideByMultiple(mWidth, Lighting::LightGridDimension);
	sceneView.mMainConstants.TileCount[1] = Math::DivideByMultiple(mHeight, Lighting::LightGridDimension);

	sceneView.mMainConstants.EyePosW = mCameraPostion;

	for (unsigned int FaceIndex = 0; FaceIndex < (uint32_t)CubeMapFace::FaceCount; ++FaceIndex)
	{
		ScopedTimer PrePassScope(L"CaptureScene", Context);

		Render::SetCommonSRVHeapOffset(GBUFFER_TEXTURE_REGISTER_COUNT);

		//update constant buffer
		sceneView.mMainConstants.View = mCubeMapCBV[FaceIndex].View;
		sceneView.mMainConstants.ViewProj = mCubeMapCBV[FaceIndex].ViewProj;

		//update light VS POSITION
		Lighting::UpdateViewSpacePosition(*mCubeMapCamera[FaceIndex]);

		//default batch
		MeshRenderPass DefaultMeshPass(MeshRenderPass::eDefault);
		DefaultMeshPass.SetCamera(*mCubeMapCamera[FaceIndex]);
		DefaultMeshPass.SetViewport(Viewport());
		DefaultMeshPass.SetScissor(ScissorRect());

		DefaultMeshPass.SetDepthStencilTarget(mDepthBuffer);
		DefaultMeshPass.AddRenderTarget(mCubeMap);
		DefaultMeshPass.ReplaceRenderTarget(RTV(FaceIndex), 0);

		//Culling and add  Render Objects
		for (auto& model : scene.GetRenderObjects())
		{
			if (model->GetVisible())
			{
				dynamic_cast<glTFInstanceModel*>(model)->GetModel()->AddToRender(DefaultMeshPass);
			}
		}

		//Sort Visible objects
		DefaultMeshPass.Sort();

		//Depth PrePass
		{
			ScopedTimer PrePassScope(L"CaptureScene: Depth PrePass", Context);

			//Clear Depth And Stencil
			Context.TransitionResource(mDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE);
			Context.ClearDepthAndStencil(mDepthBuffer, REVERSE_Z ? 0.0f : 1.0f);
			DefaultMeshPass.RenderMeshes(MeshRenderPass::eZPass, Context, sceneView.mMainConstants);
			//Context.BeginResourceTransition(mDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		}

		//no SSAO
		D3D12_CPU_DESCRIPTOR_HANDLE DefaultMaskTexture = GetDefaultTexture(eWhiteMask2D);
		CopyBufferDescriptors(&DefaultMaskTexture, 1, &gTextureHeap[Render::GetCommonSRVHeapOffset()]);
		Render::SetCommonSRVHeapOffset(Render::GetCommonSRVHeapOffset() + 1);


		//Light Culling

		Lighting::FillLightGrid_Internal(Context, *mCubeMapCamera[FaceIndex], mCubeMap, mDepthBuffer);
		Context.Flush(true);

		//Copy PBR SRV
		D3D12_CPU_DESCRIPTOR_HANDLE PBR_SRV[] =
		{
			Lighting::m_LightGrid.GetSRV(),
			Lighting::m_LightBuffer.GetSRV(),
			Lighting::m_LightShadowArray.GetSRV(),
			Lighting::m_ClusterLightGrid.GetSRV(),
			Lighting::m_GlobalLightIndexList.GetSRV()
		};

		CopyBufferDescriptors(PBR_SRV, ArraySize(PBR_SRV), &gTextureHeap[Render::GetCommonSRVHeapOffset()]);
		Render::SetCommonSRVHeapOffset(Render::GetCommonSRVHeapOffset() + ArraySize(PBR_SRV));

		Context.SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, gTextureHeap.GetHeapPointer());
		//Set Common SRV
		Context.SetDescriptorTable((int)RootSignatureType::eCommonSRVs, gTextureHeap[0]);
		//Set Cube SRV
		Context.SetDescriptorTable((int)RootSignatureType::eCubeTextures, gTextureHeap[COMMONSRVSIZE + COMMONUAVSIZE]);
		//Set Textures SRV
		Context.SetDescriptorTable((int)RootSignatureType::ePBRTextures, gTextureHeap[MAXCUBESRVSIZE + COMMONSRVSIZE + COMMONUAVSIZE]);


		{
			ScopedTimer MainRenderScope(L"Main Render", Context);

			//Sun Shadow Map
			static bool IsShadowMapCreate = false;
			if (!IsShadowMapCreate)
			{
				scene.CreateShadowMapForGLTF(Context);
				IsShadowMapCreate = true;
			}

			//Base Pass
			{
				ScopedTimer RenderColorScope(L"Render Color", Context);

				//Sky
				if (scene.GetRenderObjects(ObjectType::Sky).front()->GetVisible())
				{
					Context.TransitionResource(mDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_READ);
					Context.SetRenderTarget(RTV(FaceIndex), mDepthBuffer.GetDSV_DepthReadOnly());
					Context.SetViewportAndScissor(Viewport(), ScissorRect());
					Context.SetDynamicConstantBufferView((int)RootSignatureType::eMainConstantBuffer, sizeof(MainConstants), &sceneView.mMainConstants);
					scene.GetRenderObjects(ObjectType::Sky).front()->Draw(Context);
				}

				DefaultMeshPass.RenderMeshes(MeshRenderPass::eOpaque, Context, sceneView.mMainConstants);
			}

			//Transparent Pass
			DefaultMeshPass.RenderMeshes(MeshRenderPass::eTransparent, Context, sceneView.mMainConstants);
		}
#ifdef DEBUG
		//EngineGUI::AddRenderPassVisualizeTexture("SSS", WStringToString(mCubeMap.GetName()), mCubeMap.GetHeight(), mCubeMap.GetWidth(), SRV());
#endif // DEBUG
	}
}

void CubeRenderTarget::BuildDescriptors()
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = mFormat;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.TextureCube.MostDetailedMip = 0;
	srvDesc.TextureCube.MipLevels = mMipMap;
	srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;

	// Create SRV to the entire cube map resource.
	mCubeSrv = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	g_Device->CreateShaderResourceView(mCubeMap.GetResource(), &srvDesc, mCubeSrv);

	mSrvIndex = AddToGlobalCubeSRVDescriptor(mCubeSrv);

	// Create RTV to each cube face.
	mRenderTargetDes.resize((size_t)CubeMapFace::FaceCount * mMipMap);
	for (UINT i = 0; i < (uint32_t)CubeMapFace::FaceCount; ++i)
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
	mDepthBuffer.Create(L"Depth buffer", mWidth, mHeight, DXGI_FORMAT_R32_TYPELESS, REVERSE_Z);
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

D3D12_VIEWPORT CubeRenderTarget::Viewport(UINT mipmap) const
{
	return mViewport.at(mipmap);
}

D3D12_RECT CubeRenderTarget::ScissorRect() const
{
	return mScissorRect;
}

