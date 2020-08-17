//#pragma once

#include "L3DUtil.h"
#include "ModelMesh.h"
#include "L3DTextureManage.h"

//assimp head
#include <assimp\Importer.hpp>
#include <assimp\scene.h>
#include <assimp\postprocess.h>


class ModelLoader
{
public:
    ModelLoader(HWND hwnd, ID3D12Device* dev, ID3D12GraphicsCommandList* CommandList,L3DTextureManage* TextureManage);
    ~ModelLoader();


    bool Load(string filename);

   
    void DrawModel(string ModelName);

    vector<ModelMesh>& GetModelMesh(string ModelName);
    unordered_map<string, vector<Texture*>>& GetAllModelTextures();


    vector<string> GetModelTextureNames(string modelname);
    void Close();

private:
    ID3D12Device* dev;
    ID3D12GraphicsCommandList* pCommandList;
    HWND hwnd;

    unordered_map<string,vector<ModelMesh>> meshes;
    unordered_map<string, vector<Texture*>> ModelTextures;
    unordered_map<string, vector<string>> ModelTexturesName;
    string directory;
    string ModelName;
    L3DTextureManage* pTextureManage;

    //not use
    vector<Texture > Textures;
private://functions
    void ProcessNode(aiNode* node, const aiScene* scene);
    ModelMesh ProcessMesh(aiMesh* mesh, const aiScene* scene);
    vector<Texture> LoadMaterialTextures(aiMaterial* mat, aiTextureType type, string typeName, const aiScene* scene);
    string DetermineTextureType(const aiScene* scene, aiMaterial* mat);
    int GetTextureIndex(aiString* str);
    void GetTextureFromModel(const aiScene* scene, int textureindex, Texture& texture);

    
    void LoadPBRTexture(string texturename);
};
