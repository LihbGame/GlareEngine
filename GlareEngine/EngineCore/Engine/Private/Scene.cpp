#include "Scene.h"
#include "ModelLoader.h"
/// Scene/////////////////////////////////////////////

Scene::Scene(string name, ID3D12GraphicsCommandList* pCommandList)
	:mName(name),
	m_pCommandList(pCommandList)
{
}


void Scene::Update(float DeltaTime)
{
}

void Scene::AddObjectToScene(RenderObject* Object)
{
	pRenderObjects.push_back(Object);
}

void Scene::RenderScene(RenderPipelineType Type, GraphicsContext& Context)
{
	switch (Type)
	{
	case RenderPipelineType::Forward:
	{
		ForwardRendering(Context);
		break;
	}

	case RenderPipelineType::Forward_Plus:
	{
		ForwardPlusRendering(Context);
		break;
	}
	case RenderPipelineType::Deferred:
	{
		DeferredRendering(Context);
		break;
	}
	default:
		break;
	}
	
}

void Scene::CreateModelInstance(string ModelName, int Num_X, int Num_Y)
{
	assert(m_pCommandList);
	const ModelRenderData* ModelData = ModelLoader::GetModelLoader(m_pCommandList)->GetModelRenderData(ModelName);

	InstanceRenderData InstanceData;
	InstanceData.mModelData = ModelData;
	InstanceData.mInstanceConstants.resize(ModelData->mSubModels.size());
	for (int SubMeshIndex = 0; SubMeshIndex < ModelData->mSubModels.size(); SubMeshIndex++)
	{
		for (int i = 0; i < Num_X; ++i)
		{
			for (int y = 0; y < Num_Y; ++y)
			{
				InstanceRenderConstants IRC;
				IRC.mMaterialIndex = ModelData->mSubModels[SubMeshIndex].mMaterial->mMatCBIndex;
				XMStoreFloat4x4(&IRC.mWorldTransform, XMMatrixTranspose(XMMatrixRotationY(MathHelper::RandF() * MathHelper::Pi) * XMMatrixScaling(0.2f, 0.2f, 0.2f) * XMMatrixTranslation((i - Num_X / 2) * 50.0f, 0, (y - Num_Y / 2) * 50.0f)));
				InstanceData.mInstanceConstants[SubMeshIndex].push_back(IRC);
			}
		}
	}
	mModels.push_back(InstanceModel(StringToWString(ModelName), InstanceData));
}

void Scene::CreateSimpleModelInstance(string ModelName, SimpleModelType Type, string MaterialName, int Num_X, int Num_Y)
{
	assert(m_pCommandList);
	const ModelRenderData* ModelData = SimpleModelGenerator::GetInstance(m_pCommandList)->CreateSimpleModelRanderData(ModelName, Type, MaterialName);
	InstanceRenderData InstanceData;
	InstanceData.mModelData = ModelData;
	InstanceData.mInstanceConstants.resize(ModelData->mSubModels.size());
	for (int SubMeshIndex = 0; SubMeshIndex < ModelData->mSubModels.size(); SubMeshIndex++)
	{
		for (int i = 0; i < Num_X; ++i)
		{
			for (int y = 0; y < Num_Y; ++y)
			{
				InstanceRenderConstants IRC;

				IRC.mMaterialIndex = ModelData->mSubModels[SubMeshIndex].mMaterial->mMatCBIndex;
				XMStoreFloat4x4(&IRC.mWorldTransform, XMMatrixTranspose(XMMatrixScaling(4, 4, 4) * XMMatrixTranslation(i * 60.0f, 0, y * 60.0f)));
				XMStoreFloat4x4(&IRC.mTexTransform, XMMatrixTranspose(XMMatrixScaling(10, 10, 10)));
				InstanceData.mInstanceConstants[SubMeshIndex].push_back(IRC);
			}
		}
	}
	mModels.push_back(InstanceModel(StringToWString(ModelName), InstanceData));
}

void Scene::CreateShadowMap(GraphicsContext& Context)
{


}

void Scene::ForwardRendering(GraphicsContext& Context)
{
	//Set Textures SRV
	Context.SetDynamicDescriptors(3, 0, (UINT)g_TextureSRV.size(), g_TextureSRV.data());
	//Set Material Data
	const vector<MaterialConstant>& MaterialData = MaterialManager::GetMaterialInstance()->GetMaterialsConstantBuffer();
	Context.SetDynamicSRV(4, sizeof(MaterialConstant) * MaterialData.size(), MaterialData.data());

	Context.PIXBeginEvent(L"Shadow Pass");
	CreateShadowMap(Context);
	Context.PIXBeginEvent(L"Shadow Pass");


	Context.PIXBeginEvent(L"Main Pass");
	for (auto& RenderObject : pRenderObjects)
	{
		Context.PIXBeginEvent(RenderObject->GetName().c_str());
		RenderObject->Draw(Context);
		Context.PIXEndEvent();
	}
	for (auto& Model : mModels)
	{
		Context.PIXBeginEvent(Model.GetName().c_str());
		Model.Draw(Context);
		Context.PIXEndEvent();
	}
	Context.PIXBeginEvent(L"Main Pass");
}

void Scene::ForwardPlusRendering(GraphicsContext& Context)
{
}

void Scene::DeferredRendering(GraphicsContext& Context)
{
}





