#include "ModelMesh.h"


ModelMesh::ModelMesh(ID3D12Device* dev, ID3D12GraphicsCommandList* CommandList, std::vector<Vertices::PosNormalTangentTex> vertices, std::vector<UINT> indices, std::vector<Texture> textures)
{
    this->vertices = vertices;
    this->indices = indices;
    this->textures = textures;

    this->dev = dev;
    this->pCommandList = CommandList;

    this->SetupMesh();
}


ModelMesh::~ModelMesh()
{
}



// Initializes all the buffer objects/arrays
void ModelMesh::SetupMesh()
{
    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertices::PosNormalTangentTex);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(UINT);

    mMeshGeo.Name = L"Model Mesh";
    ThrowIfFailed(D3DCreateBlob(vbByteSize, &mMeshGeo.VertexBufferCPU));
    CopyMemory(mMeshGeo.VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);


    ThrowIfFailed(D3DCreateBlob(ibByteSize, &mMeshGeo.IndexBufferCPU));
    CopyMemory(mMeshGeo.IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);


    mMeshGeo.VertexBufferGPU = EngineUtility::CreateDefaultBuffer(dev,
        pCommandList, vertices.data(), vbByteSize, mMeshGeo.VertexBufferUploader);


    mMeshGeo.IndexBufferGPU = EngineUtility::CreateDefaultBuffer(dev,
        pCommandList, indices.data(), ibByteSize, mMeshGeo.IndexBufferUploader);


    mMeshGeo.VertexByteStride = sizeof(Vertices::PosNormalTangentTex);
    mMeshGeo.VertexBufferByteSize = vbByteSize;
    mMeshGeo.IndexFormat = DXGI_FORMAT_R32_UINT;
    mMeshGeo.IndexBufferByteSize = ibByteSize;


   
    SubmeshGeometry submesh;
    submesh.IndexCount = (UINT)indices.size();
    submesh.StartIndexLocation = 0;
    submesh.BaseVertexLocation = 0;
    mMeshGeo.DrawArgs[L"Model Mesh"] = submesh;
}