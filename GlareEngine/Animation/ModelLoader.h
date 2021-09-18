#pragma once
#include "ModelMesh.h"
#include "TextureManager.h"
#include "Materials.h"
#include <assimp\Importer.hpp>
#include <assimp\scene.h>
#include <assimp\postprocess.h>

namespace GlareEngine
{
	class ModelLoader
	{
	public:
		ModelLoader(ID3D12GraphicsCommandList* CommandList);
		~ModelLoader();

		//Load model mesh data
		bool LoadModel(string filename);

		ModelRenderData& GetModelRenderData(string ModelName);

		
	private:
		ID3D12GraphicsCommandList* m_pCommandList;
		unordered_map<string, ModelRenderData> mMeshes;

		string mDirectory;
		string mModelName;

		Assimp::Importer mImporter;
	private:

		void BuildMaterial(string MaterialName);

		void ProcessNode(aiNode* node, const aiScene* scene);
		
		ModelMesh ProcessMesh(aiMesh* mesh, const aiScene* scene);
		
		vector<Texture*> LoadPBRTexture(string texturename);
	};

}