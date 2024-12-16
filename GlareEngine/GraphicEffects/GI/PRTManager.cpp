#include "PRTManager.h"
#include "Graphics/CommandContext.h"
#include "Misc/RenderObject.h"
#include "InstanceModel/SimpleModelGenerator.h"
#include "Engine/EngineProfiling.h"
#include "Graphics/Render.h"
#include "Engine/Scene.h"
#include "EngineGUI.h"
#include "Misc/LightManager.h"

//shader
#include "CompiledShaders/SHProbeDebugVS.h"
#include "CompiledShaders/SHProbeDebugPS.h"
#include "CompiledShaders/WireframePS.h"

PRTManager::PRTManager()
	:mCubeTarget(PROBE_TARGET_RESOLUTION, PROBE_TARGET_RESOLUTION)
{
	InitMaterial();
}

PRTManager::~PRTManager()
{
}

void PRTManager::Initialize(AxisAlignedBox ProbeAABB, uint16_t ProbeCellSize, float ProbeDensityScale)
{
	mProbeAABB = ProbeAABB;
	mProbeCellSize = ProbeCellSize / ProbeDensityScale;
	mProbeDensityScale = ProbeDensityScale;

	Vector3 MinPosition = ProbeAABB.GetMin();
	Vector3 size = ProbeAABB.GetDimensions() / mProbeCellSize + Vector3(1);

	uint32_t X = ceil(XMVECTOR(size).m128_f32[0]);
	uint32_t Y = ceil(XMVECTOR(size).m128_f32[1]);
	uint32_t Z = ceil(XMVECTOR(size).m128_f32[2]);

	Vector3 ProbePosition;

	for (size_t IndexX = 0; IndexX < X; IndexX++)
	{
		for (size_t IndexY = 0; IndexY < Y; IndexY++)
		{
			for (size_t IndexZ = 0; IndexZ < Z; IndexZ++)
			{
				ProbePosition = ProbeAABB.GetCenter();// MinPosition + Vector3(IndexX, IndexY, IndexZ) * mProbeCellSize;
				mSHProbes.push_back(SHProbe(XMFLOAT3(ProbePosition), mCubeTarget));
			}
		}
	}
	CreateDebugModel();
}

void PRTManager::GenerateSHProbes(Scene& scene)
{
	if (mSHProbes.size() > 0)
	{
		RenderPipelineType NewRenderPipelineType = (Render::RenderPipelineType)scene.m_pGUI->GetRenderPipelineIndex();

		if (NewRenderPipelineType != RenderPipelineType::TBFR)
		{
			Render::gRenderPipelineType = RenderPipelineType::TBFR;
			Render::BuildPSOs();
		}
		GraphicsContext& Context = GraphicsContext::Begin(L"Generate SH Probes");

		for (int SHIndex = 0; SHIndex < mSHProbes.size(); ++SHIndex)
		{
			if (SHIndex == 312)
			{
				mCubeTarget.UpdateCubeMapPosition(mSHProbes[SHIndex].GetPosition());
				mSHProbes[SHIndex].PrepareCubeMap(scene, Context);
			}
		}
		Context.Finish(true);
		Lighting::UpdateLightingDataChange(true);
	}
}

void PRTManager::CreateDebugModel(float ModelScale)
{
	if (mSHProbes.size() > 0)
	{
		GraphicsContext& Context = GraphicsContext::Begin(L"Init PRT Manager");

		const ModelRenderData* ModelData = SimpleModelGenerator::GetInstance(Context.GetCommandList())->CreateSimpleModelRanderData("PRT Debug", SimpleModelType::Sphere, "PBRrocky_shoreline1");
		InstanceRenderData InstanceData;
		InstanceData.mModelData = ModelData;
		InstanceData.mInstanceConstants.resize(ModelData->mSubModels.size());
		for (int SubMeshIndex = 0; SubMeshIndex < ModelData->mSubModels.size(); SubMeshIndex++)
		{
			for (int ModelIndex = 0; ModelIndex < mSHProbes.size(); ++ModelIndex)
			{
				InstanceRenderConstants IRC;

				XMFLOAT3 position = mSHProbes[ModelIndex].GetPosition();
				IRC.mMaterialIndex = ModelData->mSubModels[SubMeshIndex].mMaterial->mMatCBIndex;
				XMStoreFloat4x4(&IRC.mWorldTransform, XMMatrixTranspose(XMMatrixScaling(ModelScale, ModelScale, ModelScale) * XMMatrixTranslation(position.x, position.y, position.z)));
				mIRC.push_back(IRC);
			}
			InstanceConstants IC;
			IC.mDataPtr = mIRC.data();
			IC.mDataNum = mIRC.size();
			IC.mDataSize = sizeof(InstanceRenderConstants);
			InstanceData.mInstanceConstants[SubMeshIndex]= IC;
		}
		mDebugModel = make_unique<InstanceModel>(StringToWString("PRT Debug"), InstanceData);
		mDebugModel->SetShadowFlag(false);

		Context.Finish();
	}
}

void PRTManager::DebugVisual(GraphicsContext& context)
{
	if (mSHProbes.size() > 0 && mDebugModel != nullptr)
	{
		ScopedTimer DebugVisualScope(L"SH Probe Debug Visual", context);

		//Set Material Data
		const vector<MaterialConstant>& MaterialData = MaterialManager::GetMaterialInstance()->GetMaterialsConstantBuffer();
		context.SetDynamicSRV((int)RootSignatureType::eMaterialConstantData, sizeof(MaterialConstant) * MaterialData.size(), MaterialData.data());
		context.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		context.SetRenderTarget(g_SceneColorBuffer.GetRTV(), g_SceneDepthBuffer.GetDSV());
		mDebugModel->Draw(context, &mDebugMaterial->GetGraphicsPSO());
	} 
}

void PRTManager::InitMaterial()
{
	mDebugMaterial = RenderMaterialManager::GetInstance().GetMaterial("PRT Debug Material");
	if (!mDebugMaterial->IsInitialized)
	{
		mDebugMaterial->BindPSOCreateFunc([&](const PSOCommonProperty CommonProperty) {
			D3D12_RASTERIZER_DESC Rasterizer = RasterizerDefaultCw;
			D3D12_BLEND_DESC Blend = BlendDisable;
			if (CommonProperty.IsWireframe)
			{
				Rasterizer.CullMode = D3D12_CULL_MODE_NONE;
				Rasterizer.FillMode = D3D12_FILL_MODE_WIREFRAME;
			}
			if (CommonProperty.IsMSAA)
			{
				Rasterizer.MultisampleEnable = true;
				Blend.AlphaToCoverageEnable = true;
			}

			GraphicsPSO& DebugPSO = mDebugMaterial->GetGraphicsPSO();

			DebugPSO.SetRootSignature(*CommonProperty.pRootSignature);
			DebugPSO.SetRasterizerState(Rasterizer);
			DebugPSO.SetBlendState(Blend);
			if (REVERSE_Z)
			{
				DebugPSO.SetDepthStencilState(DepthStateReadWriteReversed);
			}
			else
			{
				DebugPSO.SetDepthStencilState(DepthStateReadWrite);
			}

			DebugPSO.SetSampleMask(0xFFFFFFFF);
			DebugPSO.SetInputLayout((UINT)InputLayout::InstancePosNormalTangentTexc.size(), InputLayout::InstancePosNormalTangentTexc.data());
			DebugPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
			if (CommonProperty.IsWireframe)
			{
				DebugPSO.SetPixelShader(g_pWireframePS, sizeof(g_pWireframePS));
				DebugPSO.SetDepthStencilState(DepthStateDisabled);
			}
			else
			{
				DebugPSO.SetVertexShader(g_pSHProbeDebugVS, sizeof(g_pSHProbeDebugVS));
				DebugPSO.SetPixelShader(g_pSHProbeDebugPS, sizeof(g_pSHProbeDebugPS));
			}
			DebugPSO.SetRenderTargetFormat(DefaultHDRColorFormat, g_SceneDepthBuffer.GetFormat(), CommonProperty.MSAACount, CommonProperty.MSAAQuality);
			DebugPSO.Finalize();
			});

		mDebugMaterial->BindPSORuntimeModifyFunc([&]() {
			RuntimePSOManager::Get().RegisterPSO(&mDebugMaterial->GetGraphicsPSO(), GET_SHADER_PATH("GI/SHProbeDebugVS.hlsl"), D3D12_SHVER_VERTEX_SHADER);
			RuntimePSOManager::Get().RegisterPSO(&mDebugMaterial->GetGraphicsPSO(), GET_SHADER_PATH("GI/SHProbeDebugPS.hlsl"), D3D12_SHVER_PIXEL_SHADER); });

		mDebugMaterial->IsInitialized = true;
	}
}