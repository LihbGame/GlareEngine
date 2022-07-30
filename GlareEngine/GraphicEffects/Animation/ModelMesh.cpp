#include "ModelMesh.h"
#include "GraphicsCore.h"

using namespace GlareEngine;

ModelMesh::ModelMesh(ID3D12GraphicsCommandList* CommandList, const char* name, std::vector<Vertices::PosNormalTangentTexc> vertices, std::vector<UINT> indices)
	:pModelMeshName(name)
{
	mMeshData.Vertices = vertices;
	mMeshData.Indices32 = indices;
	this->SetupMesh(CommandList, name);
}

ModelMesh::~ModelMesh()
{
}

// Initializes all the buffer objects/arrays
void ModelMesh::SetupMesh(ID3D12GraphicsCommandList* pCommandList, const char* name)
{
	const UINT vbByteSize = (UINT)mMeshData.Vertices.size() * sizeof(Vertices::PosNormalTangentTexc);
	const UINT ibByteSize = (UINT)mMeshData.Indices32.size() * sizeof(UINT);

	mMeshGeo.Name = "Model Mesh";
	ThrowIfFailed(D3DCreateBlob(vbByteSize, &mMeshGeo.VertexBufferCPU));
	CopyMemory(mMeshGeo.VertexBufferCPU->GetBufferPointer(), mMeshData.Vertices.data(), vbByteSize);


	ThrowIfFailed(D3DCreateBlob(ibByteSize, &mMeshGeo.IndexBufferCPU));
	CopyMemory(mMeshGeo.IndexBufferCPU->GetBufferPointer(), mMeshData.Indices32.data(), ibByteSize);


	mMeshGeo.VertexBufferGPU = EngineUtility::CreateDefaultBuffer(g_Device,
		pCommandList, mMeshData.Vertices.data(), vbByteSize, mMeshGeo.VertexBufferUploader);


	mMeshGeo.IndexBufferGPU = EngineUtility::CreateDefaultBuffer(g_Device,
		pCommandList, mMeshData.Indices32.data(), ibByteSize, mMeshGeo.IndexBufferUploader);


	mMeshGeo.VertexByteStride = sizeof(Vertices::PosNormalTangentTexc);
	mMeshGeo.VertexBufferByteSize = vbByteSize;
	mMeshGeo.IndexFormat = DXGI_FORMAT_R32_UINT;
	mMeshGeo.IndexBufferByteSize = ibByteSize;


	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)mMeshData.Indices32.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;
	mMeshGeo.DrawArgs[name] = submesh;
}