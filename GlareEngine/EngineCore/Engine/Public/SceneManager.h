#pragma once
#include "GraphicsCore.h"
#include "ModelMesh.h"
#include "InstanceModel.h"

class Scene
{
public:
	Scene(string name, ID3D12GraphicsCommandList* pCommandList);
	~Scene() {};

	void RenderScene();

	void CreateModelInstance(string ModelName,int Num_X, int Num_Y);
private:

private:
	ID3D12GraphicsCommandList* m_pCommandList = nullptr;
	string mName;
	vector<InstanceModel> mModels;
};



class SceneManager
{
public:
	SceneManager(ID3D12GraphicsCommandList* pCommandList):m_pCommandList(pCommandList) {};
	~SceneManager() {};

	void CreateScene(string Name);

	Scene* GetScene(string Name);

	void ReleaseScene(string name);
private:
	ID3D12GraphicsCommandList* m_pCommandList = nullptr;
	unordered_map<string,unique_ptr<Scene>> mScenes;
};

