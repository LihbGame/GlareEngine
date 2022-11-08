#include "InstanceModel.h"
#include "Graphics/Render.h"

//Shader
#include "CompiledShaders/InstanceModelVS.h"
#include "CompiledShaders/InstanceModelPS.h"

GraphicsPSO InstanceModel::mPSO;

InstanceModel::InstanceModel(wstring Name,InstanceRenderData InstanceData)
	:mInstanceData(InstanceData)
{
	mObjectType = ObjectType::Model;
	SetName(Name);
}

void InstanceModel::Draw(GraphicsContext& Context, GraphicsPSO* SpecificPSO)
{
	for (int SubModelIndex = 0; SubModelIndex < mInstanceData.mInstanceConstants.size(); ++SubModelIndex)
	{
		//Set Instance data
		const vector<InstanceRenderConstants>& InstanceData = mInstanceData.mInstanceConstants[SubModelIndex];
		Context.SetDynamicSRV((int)RootSignatureType::eInstanceConstantData, sizeof(InstanceRenderConstants)* InstanceData.size(), (void*)InstanceData.data());
		
		if (SpecificPSO)
		{
			Context.SetPipelineState(*SpecificPSO);
		}
		else
		{
			Context.SetPipelineState(mPSO);
		}
		Context.SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		std::string SubMeshName = mInstanceData.mModelData->mSubModels[SubModelIndex].mMeshData.pModelMeshName;
		const MeshGeometry& SubModelMeshGeo = mInstanceData.mModelData->mSubModels[SubModelIndex].mMeshData.mMeshGeo;
		Context.SetIndexBuffer(SubModelMeshGeo.IndexBufferView());
		Context.SetVertexBuffer(0, SubModelMeshGeo.VertexBufferView());
		UINT IndexCount =((MeshGeometry)SubModelMeshGeo).DrawArgs[SubMeshName].IndexCount;
		Context.DrawIndexedInstanced(IndexCount,(UINT)InstanceData.size(), 0, 0, 0);
	}
}

void InstanceModel::DrawShadow(GraphicsContext& Context, GraphicsPSO* SpecificShadowPSO)
{
	for (int SubModelIndex = 0; SubModelIndex < mInstanceData.mInstanceConstants.size(); ++SubModelIndex)
	{
		//Set Instance data
		const vector<InstanceRenderConstants>& InstanceData = mInstanceData.mInstanceConstants[SubModelIndex];
		Context.SetDynamicSRV((int)RootSignatureType::eInstanceConstantData, sizeof(InstanceRenderConstants) * InstanceData.size(), (void*)InstanceData.data());

		Context.SetPipelineState(*SpecificShadowPSO);

		Context.SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		std::string SubMeshName = mInstanceData.mModelData->mSubModels[SubModelIndex].mMeshData.pModelMeshName;
		const MeshGeometry& SubModelMeshGeo = mInstanceData.mModelData->mSubModels[SubModelIndex].mMeshData.mMeshGeo;
		Context.SetIndexBuffer(SubModelMeshGeo.IndexBufferView());
		Context.SetVertexBuffer(0, SubModelMeshGeo.VertexBufferView());
		UINT IndexCount = ((MeshGeometry)SubModelMeshGeo).DrawArgs[SubMeshName].IndexCount;
		Context.DrawIndexedInstanced(IndexCount, (UINT)InstanceData.size(), 0, 0, 0);
	}
}

void InstanceModel::BuildPSO(const PSOCommonProperty CommonProperty)
{
	D3D12_RASTERIZER_DESC Rasterizer = RasterizerTwoSided;
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
	mPSO.SetRootSignature(*CommonProperty.pRootSignature);
	mPSO.SetRasterizerState(Rasterizer);
	mPSO.SetBlendState(Blend);
	if (REVERSE_Z)
	{
		mPSO.SetDepthStencilState(DepthStateReadWriteReversed);
	}
	else
	{
		mPSO.SetDepthStencilState(DepthStateReadWrite);
	}

	mPSO.SetSampleMask(0xFFFFFFFF);
	mPSO.SetInputLayout((UINT)InputLayout::InstancePosNormalTangentTexc.size(), InputLayout::InstancePosNormalTangentTexc.data());
	mPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	mPSO.SetVertexShader(g_pInstanceModelVS, sizeof(g_pInstanceModelVS));
	mPSO.SetPixelShader(g_pInstanceModelPS, sizeof(g_pInstanceModelPS));
	mPSO.SetRenderTargetFormat(DefaultHDRColorFormat, g_SceneDepthBuffer.GetFormat(), CommonProperty.MSAACount, CommonProperty.MSAAQuality);
	mPSO.Finalize();
}
