#include "SceneManager.h"
#include "RanderObject.h"
#include "ModelLoader.h"
using namespace  GlareEngine;

/// Scene/////////////////////////////////////////////

Scene::Scene(string name, ID3D12GraphicsCommandList* pCommandList)
    :mName(name),
    m_pCommandList(pCommandList)
{
    CreateModelInstance("mercMaleMarksman/mercMaleMarksman.FBX", 5, 5);
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
               XMStoreFloat4x4(&IRC.mWorldTransform, XMMatrixTranslation(Num_X * 100.0f, Num_Y * 100.0f, 0));
               InstanceData.mInstanceConstants[SubMeshIndex].push_back(IRC);
           }
       }
   }
   mModels.push_back(InstanceModel(InstanceData));
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
