#include "CSky.h"
#include "GraphicsCore.h"
#include "GeometryGenerator.h"
#include "Vertex.h"

CSky::CSky(ID3D12GraphicsCommandList* CommandList,
	float radius, int sliceCount, int stackCount)
{
	BuildSkyMesh(CommandList, radius, sliceCount, stackCount);
}

CSky::~CSky()
{
}

void CSky::BuildSkyMesh(ID3D12GraphicsCommandList* CommandList, float radius, int sliceCount, int stackCount)
{
	GeometryGenerator SphereGeo;
	GeometryGenerator::MeshData Sphere = SphereGeo.CreateSphere(radius, sliceCount, stackCount);

	std::vector<Vertice::Pos> vertices(Sphere.Vertices.size());
	for (size_t i = 0; i < Sphere.Vertices.size(); ++i)
	{
		vertices[i].Pos = Sphere.Vertices[i].Position;
	}

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertice::Pos);

	std::vector<std::uint16_t> indices = Sphere.GetIndices16();
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	mSkyMesh = std::make_unique<MeshGeometry>();
	mSkyMesh->Name = "SkyGeo";

	//CPU Vertex Buffer
	ThrowIfFailed(D3DCreateBlob(vbByteSize, &mSkyMesh->VertexBufferCPU));
	CopyMemory(mSkyMesh->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);
	//CPU Index Buffer
	ThrowIfFailed(D3DCreateBlob(ibByteSize, &mSkyMesh->IndexBufferCPU));
	CopyMemory(mSkyMesh->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	//GPU Vertex Buffer
	mSkyMesh->VertexBufferGPU = EngineUtility::CreateDefaultBuffer(g_Device,
		CommandList, vertices.data(), vbByteSize, mSkyMesh->VertexBufferUploader);
	//GPU Index Buffer
	mSkyMesh->IndexBufferGPU = EngineUtility::CreateDefaultBuffer(g_Device,
		CommandList, indices.data(), ibByteSize, mSkyMesh->IndexBufferUploader);

	mSkyMesh->VertexByteStride = sizeof(Vertice::Pos);
	mSkyMesh->VertexBufferByteSize = vbByteSize;
	mSkyMesh->IndexFormat = DXGI_FORMAT_R16_UINT;
	mSkyMesh->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	mSkyMesh->DrawArgs["Sky"] = submesh;
}

void CSky::Draw()
{


}

void CSky::SetPSO()
{
}
