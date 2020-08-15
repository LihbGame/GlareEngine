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
    ModelLoader();
    ~ModelLoader();


    bool Load(HWND hwnd, ID3D12Device* dev, ID3D12GraphicsCommandList* CommandList, std::string filename);

    void Close();

private:
    ID3D12Device* dev;
    ID3D12GraphicsCommandList* pCommandList;
    std::vector<ModelMesh> meshes;
    string directory;
    vector<Texture> textures_loaded;
    HWND hwnd;
private://functions
    void ProcessNode(aiNode* node, const aiScene* scene);
    ModelMesh ProcessMesh(aiMesh* mesh, const aiScene* scene);
   // vector<Texture> LoadMaterialTextures(aiMaterial* mat, aiTextureType type, string typeName, const aiScene* scene);
    string DetermineTextureType(const aiScene* scene, aiMaterial* mat);
    int GetTextureIndex(aiString* str);
    ID3D12Resource* GetTextureFromModel(const aiScene* scene, int textureindex);
};
