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
	ModelRenderData* CreateSimpleModelRanderData(string ModelName,SimpleModelType Type,string MaterialName);
private:
	static ID3D12GraphicsCommandList* pCommandList;
	GeometryGenerator mGeometryGenerator;
	SimpleModelGenerator() {}
	static SimpleModelGenerator* S_SimpleModelGenerator;

	unordered_map<string, ModelRenderData> mMeshes;
};