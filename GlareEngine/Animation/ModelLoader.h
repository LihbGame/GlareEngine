//#pragma once

#include "Animations.h"
#include "ModelMesh.h"
#include "..\EngineDemo(Old Framework)/TextureManage.h"



class ModelLoader
{
public:
    ModelLoader(HWND hwnd, ID3D12Device* dev, ID3D12GraphicsCommandList* CommandList,TextureManage* TextureManage);
    ~ModelLoader();
   
    //load model mesh data
    bool LoadModel(string filename);
    //load animation data
    bool LoadAnimation(string filename);
   
    void DrawModel(string ModelName);

    vector<ModelMesh>& GetModelMesh(string ModelName);
    unordered_map<string, vector<Texture*>>& GetAllModelTextures();


    vector<string> GetModelTextureNames(string modelname);
    void Close();


    //animation string:model name 
    unordered_map<string, map<string, Animation>> mAnimations;

    void BuildMaterials();

	void FillSRVDescriptorHeap(int* SRVIndex,
		CD3DX12_CPU_DESCRIPTOR_HANDLE* hDescriptor);
private:
    ID3D12Device* dev;
    ID3D12GraphicsCommandList* pCommandList;
    HWND hwnd;
    map<string,int> mBoneNames;
    unordered_map<string,vector<ModelMesh>> meshes;
    unordered_map<string, vector<Texture*>> ModelTextures;
    unordered_map<string, vector<string>> ModelTexturesName;
    string directory;
    string ModelName;
    string AnimeName;
    TextureManage* pTextureManage;

    //not use
    vector<Texture > Textures;

    Assimp::Importer importer;
    const aiScene* pAnimeScene;

   

    aiMatrix4x4 m_global_inverse_transform;
private:
    ///mesh functions
    ModelLoader() {};
    void ProcessNode(aiNode* node, const aiScene* scene,bool isAniamtion);
    ModelMesh ProcessMesh(aiMesh* mesh, const aiScene* scene);
    AnimationMesh ProcessAnimation(aiMesh* mesh, const aiScene* scene);
    vector<Texture> LoadMaterialTextures(aiMaterial* mat, aiTextureType type, string typeName, const aiScene* scene);
    string DetermineTextureType(const aiScene* scene, aiMaterial* mat);
    int GetTextureIndex(aiString* str);
    void GetTextureFromModel(const aiScene* scene, int textureindex, Texture& texture);
    void LoadPBRTexture(string texturename);
};
