#pragma once
#include "EngineUtility.h"
#include "Vertex.h"
namespace GlareEngine
{
	class ModelMesh
	{
	public:
		ModelMesh(ID3D12GraphicsCommandList* CommandList, vector<Vertice::PosNormalTangentTexc> vertices, vector<UINT> indices, vector<Texture> textures);
		~ModelMesh();

		vector<Texture> mTextures;
		MeshGeometry mMeshGeo;
	private:
		void SetupMesh(ID3D12GraphicsCommandList* CommandList);

		vector<Vertice::PosNormalTangentTexc> mVertices;
		vector<UINT> mIndices;
	};
}

