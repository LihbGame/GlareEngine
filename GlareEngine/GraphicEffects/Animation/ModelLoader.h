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
		ModelLoader();
		~ModelLoader();

		static ModelLoader* GetModelLoader(ID3D12GraphicsCommandList* CommandList);
		static void Release();

		void SetCommandList(ID3D12GraphicsCommandList* CommandList);

		//Load model mesh data
		bool LoadModel(std::string filename);

		const ModelRenderData* GetModelRenderData(std::string ModelName);

		
	private:
		static ModelLoader* g_ModelLoader;

		ID3D12GraphicsCommandList* m_pCommandList;
		std::unordered_map<std::string, std::unique_ptr<ModelRenderData>> mMeshes;

		std::string mDirectory;
		std::string mModelName;

		Assimp::Importer mImporter;
	private:

		void BuildMaterial(std::string MaterialName);

		void ProcessNode(aiNode* node, const aiScene* scene);
		
		ModelMesh ProcessMesh(aiMesh* mesh, const aiScene* scene);
		
		std::vector<Texture*> LoadPBRTexture(std::string texturename);
	};

}