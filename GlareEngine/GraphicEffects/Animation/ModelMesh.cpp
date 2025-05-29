#include "ModelMesh.h"
#include "Graphics/GraphicsCore.h"
#include "Graphics/CommandContext.h"

using namespace GlareEngine;

ModelMesh::ModelMesh(const char* name, vector<Vertices::PosNormalTangentTexc> vertices, vector<UINT> indices)
	:pModelMeshName(name)
{
	mMeshData.Vertices = vertices;
	mMeshData.Indices32 = indices;
	SetupMesh(name);
	CreateDerivedViews();
}

ModelMesh::~ModelMesh()
{
}

// Initializes all the buffer objects/arrays
void ModelMesh::SetupMesh(const char* name)
{
	GraphicsContext& Context = GraphicsContext::Begin(L"ModelMesh SetupMesh");

	const UINT vbByteSize = (UINT)mMeshData.Vertices.size() * sizeof(Vertices::PosNormalTangentTexc);
	const UINT ibByteSize = (UINT)mMeshData.Indices32.size() * sizeof(UINT);

	mMeshGeo.Name = "Model Mesh";
	ThrowIfFailed(D3DCreateBlob(vbByteSize, &mMeshGeo.VertexBufferCPU));
	CopyMemory(mMeshGeo.VertexBufferCPU->GetBufferPointer(), mMeshData.Vertices.data(), vbByteSize);


	ThrowIfFailed(D3DCreateBlob(ibByteSize, &mMeshGeo.IndexBufferCPU));
	CopyMemory(mMeshGeo.IndexBufferCPU->GetBufferPointer(), mMeshData.Indices32.data(), ibByteSize);


	mMeshGeo.VertexBufferGPU = EngineUtility::CreateDefaultBuffer(g_Device,
		Context.GetCommandList(), mMeshData.Vertices.data(), vbByteSize, mMeshGeo.VertexBufferUploader);


	mMeshGeo.IndexBufferGPU = EngineUtility::CreateDefaultBuffer(g_Device,
		Context.GetCommandList(), mMeshData.Indices32.data(), ibByteSize, mMeshGeo.IndexBufferUploader);


	mMeshGeo.VertexByteStride = sizeof(Vertices::PosNormalTangentTexc);
	mMeshGeo.VertexBufferByteSize = vbByteSize;
	mMeshGeo.IndexFormat = DXGI_FORMAT_R32_UINT;
	mMeshGeo.IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)mMeshData.Indices32.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;
	mMeshGeo.DrawArgs[name] = submesh;

	Context.Finish(true);
}

void ModelMesh::CreateDerivedViews(void)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
	SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	SRVDesc.Format = DXGI_FORMAT_UNKNOWN;
	SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	SRVDesc.Buffer.NumElements = (UINT)mMeshData.Vertices.size();
	SRVDesc.Buffer.StructureByteStride = sizeof(Vertices::PosNormalTangentTexc);
	SRVDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

	mMeshGeo.m_VertexSRV = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	g_Device->CreateShaderResourceView(mMeshGeo.VertexBufferGPU.Get(), &SRVDesc, mMeshGeo.m_VertexSRV);
	mMeshGeo.m_VertexSRVIndex = AddToGlobalRayTracingDescriptor(mMeshGeo.m_VertexSRV);

	SRVDesc.Buffer.NumElements = (UINT)mMeshData.Indices32.size();
	SRVDesc.Buffer.StructureByteStride = sizeof(UINT);
	mMeshGeo.m_IndexSRV = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	g_Device->CreateShaderResourceView(mMeshGeo.IndexBufferGPU.Get(), &SRVDesc, mMeshGeo.m_IndexSRV);
	mMeshGeo.m_IndexSRVIndex = AddToGlobalRayTracingDescriptor(mMeshGeo.m_IndexSRV);
}
