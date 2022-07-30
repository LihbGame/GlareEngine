//#pragma once

#include "Animations.h"
#include "ModelMesh.h"
#include "..\EngineDemo(Old Framework)/TextureManage.h"
#include "../../../GlareEngine/GraphicEffects/Animation/ModelMesh.h"


class ModelLoader
{
public:
	ModelLoader(HWND hwnd, ID3D12Device* dev, ID3D12GraphicsCommandList* CommandList,TextureManage* TextureManage);
	~ModelLoader();
   
	//load model mesh data
	bool LoadModel(std::wstring filename);
	//load animation data
	bool LoadAnimation(std::wstring filename);
   
	void DrawModel(std::string ModelName);

	std::vector<::ModelMesh>& GetModelMesh(std::wstring ModelName);
	std::unordered_map<std::wstring, std::vector<Texture*>>& GetAllModelTextures();


	std::vector<std::wstring> GetModelTextureNames(std::wstring modelname);
	void Close();


	//animation string:model name 
	std::unordered_map<std::wstring, std::map<std::wstring, Animation>> mAnimations;

	void BuildMaterials();

	void FillSRVDescriptorHeap(int* SRVIndex,
		CD3DX12_CPU_DESCRIPTOR_HANDLE* hDescriptor);
private:
	ID3D12Device* dev;
	ID3D12GraphicsCommandList* pCommandList;
	HWND hwnd;
	std::map<std::wstring,int> mBoneNames;
	std::unordered_map<std::wstring, std::vector<::ModelMesh>> meshes;
	std::unordered_map<std::wstring, std::vector<Texture*>> ModelTextures;
	std::unordered_map<std::wstring, std::vector<std::wstring>> ModelTexturesName;
	std::wstring directory;
	std::wstring ModelName;
	std::wstring AnimeName;
	TextureManage* pTextureManage;

	//not use
	std::vector<Texture> Textures;

	Assimp::Importer importer;
	const aiScene* pAnimeScene;

   

	aiMatrix4x4 m_global_inverse_transform;
private:
	///mesh functions
	ModelLoader() {};
	void ProcessNode(aiNode* node, const aiScene* scene,bool isAniamtion);
	::ModelMesh ProcessMesh(aiMesh* mesh, const aiScene* scene);
	AnimationMesh ProcessAnimation(aiMesh* mesh, const aiScene* scene);
	std::vector<Texture> LoadMaterialTextures(aiMaterial* mat, aiTextureType type, std::string typeName, const aiScene* scene);
	std::string DetermineTextureType(const aiScene* scene, aiMaterial* mat);
	int GetTextureIndex(aiString* str);
	void GetTextureFromModel(const aiScene* scene, int textureindex, Texture& texture);
	void LoadPBRTexture(std::wstring texturename);
};
