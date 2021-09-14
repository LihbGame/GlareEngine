#pragma once

#include "EngineUtility.h"
#include "Vertex.h"


class ModelMesh
{
public:
    vector<Vertices::PosNormalTangentTexc> vertices;
    vector<UINT> indices;
    vector<Texture> textures;
    ID3D12Device* dev;
    ID3D12GraphicsCommandList* pCommandList;
    MeshGeometry mMeshGeo;


    ModelMesh(ID3D12Device* dev, ID3D12GraphicsCommandList* CommandList, vector<Vertices::PosNormalTangentTexc> vertices, vector<UINT> indices, vector<Texture> textures);
    ~ModelMesh();
private:
    void SetupMesh();
};

