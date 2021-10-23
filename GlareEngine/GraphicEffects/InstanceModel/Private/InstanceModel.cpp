#include "InstanceModel.h"

//Shader
#include "CompiledShaders/InstanceModelVS.h"
#include "CompiledShaders/InstanceModelPS.h"

GraphicsPSO InstanceModel::mPSO;

InstanceModel::InstanceModel(wstring Name,InstanceRenderData InstanceData)
	:mInstanceData(InstanceData)
{
	SetName(Name);
}

void InstanceModel::Draw(GraphicsContext& Context, GraphicsPSO* SpecificPSO)
{
	for (int SubModelIndex = 0; SubModelIndex < mInstanceData.mInstanceConstants.size(); ++SubModelIndex)
	{
		//Set Instance data
		const vector<InstanceRenderConstants>& InstanceData = mInstanceData.mInstanceConstants[SubModelIndex];
		Context.SetDynamicSRV(5, sizeof(InstanceRenderConstants)* InstanceData.size(), (void*)InstanceData.data());
		
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

void InstanceModel::BuildPSO(const RootSignature& rootSignature)
{
	mPSO.SetRootSignature(rootSignature);
	mPSO.SetRasterizerState(RasterizerTwoSided);
	mPSO.SetBlendState(BlendDisable);
	mPSO.SetDepthStencilState(CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT));
	mPSO.SetSampleMask(0xFFFFFFFF);
	mPSO.SetInputLayout((UINT)InputLayout::InstancePosNormalTangentTexc.size(), InputLayout::InstancePosNormalTangentTexc.data());
	mPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	mPSO.SetVertexShader(g_pInstanceModelVS, sizeof(g_pInstanceModelVS));
	mPSO.SetPixelShader(g_pInstanceModelPS, sizeof(g_pInstanceModelPS));
	mPSO.SetRenderTargetFormat(DefaultHDRColorFormat, g_SceneDepthBuffer.GetFormat());
	mPSO.Finalize();
}
