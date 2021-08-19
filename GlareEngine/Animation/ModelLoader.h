#pragma once
#include "ModelMesh.h"
#include "TextureManager.h"
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

		void DrawModel(string ModelName);

		vector<ModelMesh>& GetModelMesh(string ModelName);
		
		unordered_map<string, vector<Texture*>>& GetAllModelTextures();

		vector<string> GetModelTextureNames(string modelname);
		
		void Close();

		void BuildMaterials();

		void FillSRVDescriptorHeap(int* SRVIndex,
			CD3DX12_CPU_DESCRIPTOR_HANDLE* hDescriptor);
	private:
		ID3D12GraphicsCommandList* m_pCommandList;
		unordered_map<string, vector<ModelMesh>> mMeshes;
		unordered_map<string, vector<Texture*>> mModelTextures;
		unordered_map<string, vector<string>> mModelTexturesName;
		string mDirectory;
		string mModelName;

		//not use
		vector<Texture > mTextures;
		Assimp::Importer mImporter;
	private:
		///mesh functions
		ModelLoader() {};
		
		void ProcessNode(aiNode* node, const aiScene* scene);
		
		ModelMesh ProcessMesh(aiMesh* mesh, const aiScene* scene);

		vector<Texture> LoadMaterialTextures(aiMaterial* mat, aiTextureType type, string typeName, const aiScene* scene);
		
		string DetermineTextureType(const aiScene* scene, aiMaterial* mat);
		
		int GetTextureIndex(aiString* str);
		
		void GetTextureFromModel(const aiScene* scene, int textureindex, Texture& texture);
		
		void LoadPBRTexture(string texturename);
	};

}