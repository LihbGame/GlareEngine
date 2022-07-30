#pragma once
#include "ModelMesh.h"
#include "GeometryGenerator.h"

using namespace GlareEngine;

enum class SimpleModelType
{
	Box,
	Sphere,
	Cylinder,
	Grid,
	ScreenQuad,
};


class SimpleModelGenerator
{
public:
	static SimpleModelGenerator* GetInstance(ID3D12GraphicsCommandList* CommandList) 
	{ 
		pCommandList = CommandList;
		return S_SimpleModelGenerator; 
	}
	static void Release() { delete  S_SimpleModelGenerator; }
	void CreatePBRMaterials();
	ModelRenderData* CreateSimpleModelRanderData(std::string ModelName,SimpleModelType Type, std::string MaterialName);
private:
	static ID3D12GraphicsCommandList* pCommandList;
	GeometryGenerator mGeometryGenerator;
	SimpleModelGenerator() {}
	static SimpleModelGenerator* S_SimpleModelGenerator;

	std::unordered_map<std::string, ModelRenderData> mMeshes;
};