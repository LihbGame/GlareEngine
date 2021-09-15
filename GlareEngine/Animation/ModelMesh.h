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
		ModelMesh(ID3D12GraphicsCommandList* CommandList,const char* name, vector<Vertices::PosNormalTangentTexc> vertices, vector<UINT> indices);
		~ModelMesh();

		MeshGeometry mMeshGeo;
	private:
		void SetupMesh(ID3D12GraphicsCommandList* CommandList, const char* name);
	
	private:
		const char* pModelMeshName = nullptr;
		MeshData mMeshData;
	};
}

