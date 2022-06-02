#pragma once
#include "GraphicsCore.h"
#include "ModelMesh.h"
#include "InstanceModel/InstanceModel.h"
#include "InstanceModel/SimpleModelGenerator.h"
#include "Scene.h"


class SceneManager
{
public:
	SceneManager(ID3D12GraphicsCommandList* pCommandList):m_pCommandList(pCommandList) {};
	~SceneManager() {};

	Scene* CreateScene(string Name);

	Scene* GetScene(string Name);

	void ReleaseScene(string name);
private:
	ID3D12GraphicsCommandList* m_pCommandList = nullptr;
	unordered_map<string,unique_ptr<Scene>> mScenes;
};

