#pragma once
#include "EngineUtility.h"
#include "MeshStruct.h"
#include "Vertex.h"

namespace GlareEngine
{
	class ModelMesh
	{
	public:
		ModelMesh() {}
		ModelMesh(ID3D12GraphicsCommandList* CommandList,string name, vector<Vertices::PosNormalTangentTexc> vertices, vector<UINT> indices);
		~ModelMesh();
	public:
		MeshGeometry mMeshGeo;
		string pModelMeshName;
	private:
		void SetupMesh(ID3D12GraphicsCommandList* CommandList, string name);
	
	private:

		MeshData mMeshData;
	};

	struct ModelData
	{
		ModelMesh mMeshData;
		const Material* mMaterial;
	};

	struct ModelRenderData
	{
		vector<ModelData> mSubModels;
	};
}

