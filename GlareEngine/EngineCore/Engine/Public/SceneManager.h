#pragma once
#include "GraphicsCore.h"
#include "ModelMesh.h"
#include "InstanceModel.h"

class Scene
{
public:
	Scene(string name);
	~Scene() {};

	void RenderScene();

	void CreateModelInstance(string ModelName,int Num_X, int Num_Y);
private:

private:
	string mName;
	vector<InstanceModel> mModels;
};



class SceneManager
{
public:
	SceneManager() {};
	~SceneManager() {};

	void CreateScene(string Name);

	Scene* GetScene(string Name);

	void ReleaseScene(string name);
private:
	unordered_map<string,unique_ptr<Scene>> mScenes;
};

