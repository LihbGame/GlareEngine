#include "Sky.h"
#include "L3DGeometryGenerator.h"
#include "L3DVertex.h"
using namespace DirectX;
Sky::Sky(ID3D12Device* d3dDevice, ID3D12GraphicsCommandList* CommandList,float radius, int sliceCount, int stackCount)
:md3dDevice(d3dDevice),
mCommandList(CommandList),
mRadius(radius),
mSliceCount(sliceCount),
mStackCount(stackCount)
{
}

Sky::~Sky()
{
}
void Sky::BuildSkyMesh()
{
	GeometryGenerator SphereGeo;
	GeometryGenerator::MeshData Sphere = SphereGeo.CreateSphere(mRadius, mSliceCount, mStackCount);

	std::vector<Pos> vertices(Sphere.Vertices.size());
	for (size_t i = 0; i < Sphere.Vertices.size(); ++i)
	{
		vertices[i].Pos = Sphere.Vertices[i].Position;
	}

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Pos);

	std::vector<std::uint16_t> indices = Sphere.GetIndices16();
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	mSkyMesh= std::make_unique<MeshGeometry>();
	mSkyMesh->Name = "SphereGeo";

	//CPU Vertex Buffer
	ThrowIfFailed(D3DCreateBlob(vbByteSize, &mSkyMesh->VertexBufferCPU));
	CopyMemory(mSkyMesh->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);
	//CPU Index Buffer
	ThrowIfFailed(D3DCreateBlob(ibByteSize, &mSkyMesh->IndexBufferCPU));
	CopyMemory(mSkyMesh->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);
	//GPU Buffer
	mSkyMesh->VertexBufferGPU = L3DUtil::CreateDefaultBuffer(md3dDevice,
		mCommandList, vertices.data(), vbByteSize, mSkyMesh->VertexBufferUploader);

	mSkyMesh->IndexBufferGPU = L3DUtil::CreateDefaultBuffer(md3dDevice,
		mCommandList, indices.data(), ibByteSize, mSkyMesh->IndexBufferUploader);

	mSkyMesh->VertexByteStride = sizeof(Pos);
	mSkyMesh->VertexBufferByteSize = vbByteSize;
	mSkyMesh->IndexFormat = DXGI_FORMAT_R16_UINT;
	mSkyMesh->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	mSkyMesh->DrawArgs["Sphere"] = submesh;

	
}

std::unique_ptr<MeshGeometry>& Sky::GetSkyMesh()
{
	return mSkyMesh;
}

void Sky::BuildSkyMaterials(int MatCBIndex)
{
	auto sky = std::make_unique<Material>();
	sky->Name = L"sky";
	sky->MatCBIndex = MatCBIndex;
	sky->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	sky->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	mMaterial=std::move(sky);
}

std::unique_ptr<Material>& Sky::GetSkyMaterial()
{
	return mMaterial;
}
