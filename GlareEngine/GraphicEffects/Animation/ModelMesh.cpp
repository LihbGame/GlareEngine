#include "ModelMesh.h"
#include "Graphics/GraphicsCore.h"

using namespace GlareEngine;

ModelMesh::ModelMesh(ID3D12GraphicsCommandList* CommandList, const char* name, vector<Vertices::PosNormalTangentTexc> vertices, vector<UINT> indices)
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

void ModelMesh::CreateDerivedViews(void)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
	SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	SRVDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	SRVDesc.Buffer.NumElements = (UINT)mMeshData.Vertices.size();
	SRVDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

	D3D12_CPU_DESCRIPTOR_HANDLE srv = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	g_Device->CreateShaderResourceView(mMeshGeo.VertexBufferGPU.Get(), &SRVDesc, srv);
	mMeshGeo.m_VertexSRVIndex = AddToGlobalRayTracingDescriptor(srv);

	SRVDesc.Buffer.NumElements = (UINT)mMeshData.Indices32.size();
	srv = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	g_Device->CreateShaderResourceView(mMeshGeo.IndexBufferGPU.Get(), &SRVDesc, srv);
	mMeshGeo.m_IndexSRVIndex = AddToGlobalRayTracingDescriptor(srv);
}
