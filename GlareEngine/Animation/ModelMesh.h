#pragma once

#include "L3DUtil.h"
#include "L3DVertex.h"


class ModelMesh
{
public:
    vector<PosNormalTangentTexc> vertices;
    vector<UINT> indices;
    vector<Texture> textures;
    ID3D12Device* dev;
    ID3D12GraphicsCommandList* pCommandList;
    MeshGeometry mMeshGeo;


    ModelMesh(ID3D12Device* dev, ID3D12GraphicsCommandList* CommandList, vector<PosNormalTangentTexc> vertices, vector<UINT> indices, vector<Texture> textures);
    ~ModelMesh();
private:
    void SetupMesh();
};

