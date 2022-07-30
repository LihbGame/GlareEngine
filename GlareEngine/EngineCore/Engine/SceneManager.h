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

	Scene* CreateScene(std::string Name);

	Scene* GetScene(std::string Name);

	void ReleaseScene(std::string name);
private:
	ID3D12GraphicsCommandList* m_pCommandList = nullptr;
	std::unordered_map<std::string, std::unique_ptr<Scene>> mScenes;
};

