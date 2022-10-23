#pragma once
#include "ModelMesh.h"
#include "Graphics/TextureManager.h"
#include "Engine/Materials.h"
#include <assimp\Importer.hpp>
#include <assimp\scene.h>
#include <assimp\postprocess.h>

namespace GlareEngine
{
	class ModelLoader
	{
	public:
		ModelLoader();
		~ModelLoader();

		static ModelLoader* GetModelLoader(ID3D12GraphicsCommandList* CommandList);
		static void Release();

		void SetCommandList(ID3D12GraphicsCommandList* CommandList);

		//Load model mesh data
		bool LoadModel(string filename);

		const ModelRenderData* GetModelRenderData(string ModelName);

		
	private:
		static ModelLoader* g_ModelLoader;

		ID3D12GraphicsCommandList* m_pCommandList;
		unordered_map<string, unique_ptr<ModelRenderData>> mMeshes;

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