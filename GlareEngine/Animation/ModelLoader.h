//#pragma once

#include "L3DUtil.h"
#include "ModelMesh.h"


//assimp head
#include <assimp\Importer.hpp>
#include <assimp\scene.h>
#include <assimp\postprocess.h>


class ModelLoader
{
public:
    ModelLoader(HWND hwnd, ID3D12Device* dev, ID3D12GraphicsCommandList* CommandList);
    ~ModelLoader();


    bool Load(string filename);

   
    void DrawModel(string ModelName);

    vector<ModelMesh>& GetModelMesh(string ModelName);

    void Close();

private:
    ID3D12Device* dev;
    ID3D12GraphicsCommandList* pCommandList;
    HWND hwnd;

    unordered_map<string,vector<ModelMesh>> meshes;

    string directory;
    string ModelName;
    vector<Texture> textures_loaded;

private://functions
    void ProcessNode(aiNode* node, const aiScene* scene);
    ModelMesh ProcessMesh(aiMesh* mesh, const aiScene* scene);
    vector<Texture> LoadMaterialTextures(aiMaterial* mat, aiTextureType type, string typeName, const aiScene* scene);
    string DetermineTextureType(const aiScene* scene, aiMaterial* mat);
    int GetTextureIndex(aiString* str);
    void GetTextureFromModel(const aiScene* scene, int textureindex, Texture& texture);



};
