#include "SceneManager.h"
#include "RanderObject.h"
#include "ModelLoader.h"
using namespace  GlareEngine;

/// Scene/////////////////////////////////////////////

Scene::Scene(string name, ID3D12GraphicsCommandList* pCommandList)
    :mName(name),
    m_pCommandList(pCommandList)
{
    CreateModelInstance("BlueTree/Blue_Tree_02a.FBX", 5, 5);
}


void Scene::RenderScene(GraphicsContext& Context)
{
    //Set Textures SRV
    Context.SetDynamicDescriptors(3, 0, (UINT)g_TextureSRV.size(), g_TextureSRV.data());
    //Set Material Data
   const vector<MaterialConstant>& MaterialData = MaterialManager::GetMaterialInstance()->GetMaterialsConstantBuffer();
   Context.SetDynamicSRV(4,sizeof(MaterialConstant) * MaterialData.size(), MaterialData.data());


    for (auto& e : mModels)
    {
        Context.PIXBeginEvent(e.GetName().c_str());
        e.Draw(Context);
        Context.PIXEndEvent();
    }
}

void Scene::CreateModelInstance(string ModelName, int Num_X, int Num_Y)
{
    assert(m_pCommandList);
    const ModelRenderData* ModelData=ModelLoader::GetModelLoader(m_pCommandList)->GetModelRenderData(ModelName);

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
               XMStoreFloat4x4(&IRC.mWorldTransform,XMMatrixTranspose(XMMatrixScaling(0.5f, 0.5f, 0.5f) * XMMatrixTranslation(i * 100.0f, -200, y * 100.0f)));
               InstanceData.mInstanceConstants[SubMeshIndex].push_back(IRC);
           }
       }
   }
   mModels.push_back(InstanceModel(StringToWString(ModelName), InstanceData));
}





/////Scene Manager///////////////////////////


void SceneManager::CreateScene(string name)
{
    auto scene = make_unique<Scene>(name, m_pCommandList);
	mScenes[name] = std::move(scene);

}

Scene* SceneManager::GetScene(string Name)
{
    if (mScenes.find(Name) == mScenes.end())
    {
        CreateScene(Name);
    }
    return mScenes[Name].get();
}

void SceneManager::ReleaseScene(string name)
{
    if (mScenes.find(name) != mScenes.end())
    {
        mScenes[name] = nullptr;
        mScenes.erase(name);
    }

}
