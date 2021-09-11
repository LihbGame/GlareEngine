#pragma once
#include "ModelMesh.h"
#include "TextureManager.h"
#include "Material.h"
#include <assimp\Importer.hpp>
#include <assimp\scene.h>
#include <assimp\postprocess.h>

namespace GlareEngine
{
	struct SubMesh
	{
		SubMesh() {}

		ModelMesh                                  mMesh;
		UINT                                       mMaterialIndex;
		vector<CD3DX12_CPU_DESCRIPTOR_HANDLE>      mDescriptors;
	};


	struct Mesh 
	{
		vector<SubMesh> mSubMeshes;
	};


	class ModelLoader
	{
	public:
		ModelLoader(ID3D12GraphicsCommandList* CommandList);
		~ModelLoader();

		//Load model mesh data
		bool LoadModel(string filename);

		Mesh& GetModelMesh(string ModelName);

		
	private:
		ID3D12GraphicsCommandList* m_pCommandList;
		unordered_map<string, Mesh> mMeshes;

		string mDirectory;
		string mModelName;

		//not use
		vector<Texture > mTextures;
		Assimp::Importer mImporter;
	private:
		void CreateSRVDescriptor(vector<Texture*> texture);

		void BuildMaterial(string MaterialName);

		void ProcessNode(aiNode* node, const aiScene* scene);
		
		ModelMesh ProcessMesh(aiMesh* mesh, const aiScene* scene);
		
		void LoadPBRTexture(string texturename);
	};

}