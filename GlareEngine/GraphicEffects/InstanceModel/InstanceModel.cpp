#include "InstanceModel.h"
#include "Graphics/Render.h"

//Shader
#include "CompiledShaders/InstanceModelVS.h"
#include "CompiledShaders/InstanceModelPS.h"


InstanceModel::InstanceModel(wstring Name,InstanceRenderData InstanceData)
	:mInstanceData(InstanceData)
{
	mObjectType = ObjectType::Model;
	SetName(Name);
	//Init Material
	InitMaterial();
}

void InstanceModel::Draw(GraphicsContext& Context, GraphicsPSO* SpecificPSO)
{
	for (int SubModelIndex = 0; SubModelIndex < mInstanceData.mInstanceConstants.size(); ++SubModelIndex)
	{
		if (SpecificPSO)
		{
			GraphicsPSO specificPSO = *SpecificPSO;
			Context.SetPipelineState(GET_PSO(specificPSO));
		}
		else
		{
			Context.SetPipelineState(mInstanceModelMaterial->GetGraphicsPSO());
		}

		//Set Instance data
		const InstanceConstants& InstanceData = mInstanceData.mInstanceConstants[SubModelIndex];
		Context.SetDynamicSRV((int)RootSignatureType::eInstanceConstantData, InstanceData.mDataSize* InstanceData.mDataNum, InstanceData.mDataPtr);
		
		Context.SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		std::string SubMeshName = mInstanceData.mModelData->mSubModels[SubModelIndex].mMeshData.pModelMeshName;
		const MeshGeometry& SubModelMeshGeo = mInstanceData.mModelData->mSubModels[SubModelIndex].mMeshData.mMeshGeo;
		Context.SetIndexBuffer(SubModelMeshGeo.IndexBufferView());
		Context.SetVertexBuffer(0, SubModelMeshGeo.VertexBufferView());
		UINT IndexCount =((MeshGeometry)SubModelMeshGeo).DrawArgs[SubMeshName].IndexCount;
		Context.DrawIndexedInstanced(IndexCount,(UINT)InstanceData.mDataNum, 0, 0, 0);
	}
}

void InstanceModel::DrawShadow(GraphicsContext& Context, GraphicsPSO* SpecificShadowPSO)
{
	for (int SubModelIndex = 0; SubModelIndex < mInstanceData.mInstanceConstants.size(); ++SubModelIndex)
	{
		//Set Instance data
		const InstanceConstants& InstanceData = mInstanceData.mInstanceConstants[SubModelIndex];
		Context.SetDynamicSRV((int)RootSignatureType::eInstanceConstantData, InstanceData.mDataSize * InstanceData.mDataNum, InstanceData.mDataPtr);

		GraphicsPSO specificShadowPSO = *SpecificShadowPSO;
		Context.SetPipelineState(GET_PSO(specificShadowPSO));

		Context.SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		std::string SubMeshName = mInstanceData.mModelData->mSubModels[SubModelIndex].mMeshData.pModelMeshName;
		const MeshGeometry& SubModelMeshGeo = mInstanceData.mModelData->mSubModels[SubModelIndex].mMeshData.mMeshGeo;
		Context.SetIndexBuffer(SubModelMeshGeo.IndexBufferView());
		Context.SetVertexBuffer(0, SubModelMeshGeo.VertexBufferView());
		UINT IndexCount = ((MeshGeometry)SubModelMeshGeo).DrawArgs[SubMeshName].IndexCount;
		Context.DrawIndexedInstanced(IndexCount, (UINT)InstanceData.mDataNum, 0, 0, 0);
	}
}

void InstanceModel::InitMaterial()
{
	mInstanceModelMaterial = RenderMaterialManager::GetInstance().GetMaterial("Instance Model Material");
	if (!mInstanceModelMaterial->IsInitialized)
	{
		mInstanceModelMaterial->BindPSOCreateFunc([&](const PSOCommonProperty CommonProperty) {
			D3D12_RASTERIZER_DESC Rasterizer = RasterizerTwoSidedCw;
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

			GraphicsPSO& InstanceModelPSO = mInstanceModelMaterial->GetGraphicsPSO();

			InstanceModelPSO.SetRootSignature(*CommonProperty.pRootSignature);
			InstanceModelPSO.SetRasterizerState(Rasterizer);
			InstanceModelPSO.SetBlendState(Blend);
			if (REVERSE_Z)
			{
				InstanceModelPSO.SetDepthStencilState(DepthStateReadWriteReversed);
			}
			else
			{
				InstanceModelPSO.SetDepthStencilState(DepthStateReadWrite);
			}

			if (CommonProperty.IsWireframe)
			{
				InstanceModelPSO.SetDepthStencilState(DepthStateDisabled);
			}

			InstanceModelPSO.SetSampleMask(0xFFFFFFFF);
			InstanceModelPSO.SetInputLayout((UINT)InputLayout::InstancePosNormalTangentTexc.size(), InputLayout::InstancePosNormalTangentTexc.data());
			InstanceModelPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
			InstanceModelPSO.SetVertexShader(g_pInstanceModelVS, sizeof(g_pInstanceModelVS));
			InstanceModelPSO.SetPixelShader(g_pInstanceModelPS, sizeof(g_pInstanceModelPS));
			InstanceModelPSO.SetRenderTargetFormat(DefaultHDRColorFormat, g_SceneDepthBuffer.GetFormat(), CommonProperty.MSAACount, CommonProperty.MSAAQuality);
			InstanceModelPSO.Finalize();
			});

		mInstanceModelMaterial->BindPSORuntimeModifyFunc([&]() {
			RuntimePSOManager::Get().RegisterPSO(&mInstanceModelMaterial->GetGraphicsPSO(), GET_SHADER_PATH("PBRInstanceModel/InstanceModelVS.hlsl"), D3D12_SHVER_VERTEX_SHADER);
			RuntimePSOManager::Get().RegisterPSO(&mInstanceModelMaterial->GetGraphicsPSO(), GET_SHADER_PATH("PBRInstanceModel/InstanceModelPS.hlsl"), D3D12_SHVER_PIXEL_SHADER);
			});

		mInstanceModelMaterial->IsInitialized = true;
	}
}
