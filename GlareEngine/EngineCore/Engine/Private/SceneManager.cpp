#include "SceneManager.h"
#include "RanderObject.h"

/// Scene/////////////////////////////////////////////

Scene::Scene(string name)
    :mName(name)
{
    //CreateModelInstance(5, 5);

}


void Scene::CreateModelInstance(string ModelName, int Num_X, int Num_Y)
{
    //ModelRenderData* 


    //InstanceRenderData InstanceData;

    for (int i = 0; i < Num_X; ++i)
    {
        for (int y = 0; y < Num_Y; ++y)
        {





        }
    }
    

}





/////Scene Manager///////////////////////////


void SceneManager::CreateScene(string name)
{
	auto scene = make_unique<Scene>(name);
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
