#pragma once
#include "Engine/EngineUtility.h"
#include "MeshStruct.h"
#include "Engine/Vertex.h"

namespace GlareEngine
{
	class ModelMesh
	{
	public:
		ModelMesh() {}
		ModelMesh(const char* name, vector<Vertices::PosNormalTangentTexc> vertices, vector<UINT> indices);
		~ModelMesh();
	public:
		MeshGeometry mMeshGeo;
		string pModelMeshName;

		MeshData& GetMeshData() { return mMeshData; }
	private:
		void SetupMesh(const char* name);
		void CreateDerivedViews(void);
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

