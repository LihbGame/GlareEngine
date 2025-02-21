#pragma once
#include "Animation/ModelMesh.h"
#include "Engine/GeometryGenerator.h"

using namespace GlareEngine;

enum class SimpleModelType
{
	Box,
	Sphere,
	Cylinder,
	Cone,
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
	ModelRenderData* CreateSimpleModelRanderData(string ModelName, SimpleModelType Type, string MaterialName = "None");
	ModelRenderData* CreateGridRanderData(string ModelName, float width, float height, uint32 m, uint32 n, string MaterialName = "None");
private:
	static ID3D12GraphicsCommandList* pCommandList;
	GeometryGenerator mGeometryGenerator;
	SimpleModelGenerator() {}
	static SimpleModelGenerator* S_SimpleModelGenerator;

	unordered_map<string, ModelRenderData> mMeshes;
};