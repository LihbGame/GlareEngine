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
		ModelMesh(ID3D12GraphicsCommandList* CommandList, const char* name, std::vector<Vertices::PosNormalTangentTexc> vertices, std::vector<UINT> indices);
		~ModelMesh();
	public:
		MeshGeometry mMeshGeo;
		std::string pModelMeshName;
	private:
		void SetupMesh(ID3D12GraphicsCommandList* CommandList, const char* name);
	
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
		std::vector<ModelData> mSubModels;
	};
}

