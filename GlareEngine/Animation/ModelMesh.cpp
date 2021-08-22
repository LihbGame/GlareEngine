#include "ModelMesh.h"
#include "GraphicsCore.h"

using namespace GlareEngine;

ModelMesh::ModelMesh(ID3D12GraphicsCommandList* CommandList, vector<Vertice::PosNormalTangentTexc> vertices, vector<UINT> indices)
{
	this->mVertices = vertices;
	this->mIndices = indices;

	this->SetupMesh(CommandList);
}

ModelMesh::~ModelMesh()
{
}

// Initializes all the buffer objects/arrays
void ModelMesh::SetupMesh(ID3D12GraphicsCommandList* pCommandList)
{
	const UINT vbByteSize = (UINT)mVertices.size() * sizeof(Vertice::PosNormalTangentTexc);
	const UINT ibByteSize = (UINT)mIndices.size() * sizeof(UINT);

	mMeshGeo.Name = "Model Mesh";
	ThrowIfFailed(D3DCreateBlob(vbByteSize, &mMeshGeo.VertexBufferCPU));
	CopyMemory(mMeshGeo.VertexBufferCPU->GetBufferPointer(), mVertices.data(), vbByteSize);


	ThrowIfFailed(D3DCreateBlob(ibByteSize, &mMeshGeo.IndexBufferCPU));
	CopyMemory(mMeshGeo.IndexBufferCPU->GetBufferPointer(), mIndices.data(), ibByteSize);


	mMeshGeo.VertexBufferGPU = EngineUtility::CreateDefaultBuffer(DirectX12Graphics::g_Device,
		pCommandList, mVertices.data(), vbByteSize, mMeshGeo.VertexBufferUploader);


	mMeshGeo.IndexBufferGPU = EngineUtility::CreateDefaultBuffer(DirectX12Graphics::g_Device,
		pCommandList, mIndices.data(), ibByteSize, mMeshGeo.IndexBufferUploader);


	mMeshGeo.VertexByteStride = sizeof(Vertice::PosNormalTangentTexc);
	mMeshGeo.VertexBufferByteSize = vbByteSize;
	mMeshGeo.IndexFormat = DXGI_FORMAT_R32_UINT;
	mMeshGeo.IndexBufferByteSize = ibByteSize;


	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)mIndices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;
	mMeshGeo.DrawArgs["Model Mesh"] = submesh;
}