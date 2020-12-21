#include "Sky.h"
#include "L3DGeometryGenerator.h"
#include "L3DVertex.h"
#include "L3DMaterial.h"
#include "L3DTextureManage.h"
using namespace DirectX;
Sky::Sky(ID3D12Device* d3dDevice, ID3D12GraphicsCommandList* CommandList,float radius, int sliceCount, int stackCount, L3DTextureManage* TextureManage)
:md3dDevice(d3dDevice),
mCommandList(CommandList),
mRadius(radius),
mSliceCount(sliceCount),
mStackCount(stackCount),
pTextureManage(TextureManage)
{
}

Sky::~Sky()
{
}
void Sky::BuildSkyMesh()
{
	GeometryGenerator SphereGeo;
	GeometryGenerator::MeshData Sphere = SphereGeo.CreateSphere(mRadius, mSliceCount, mStackCount);

	std::vector<L3DVertice::Pos> vertices(Sphere.Vertices.size());
	for (size_t i = 0; i < Sphere.Vertices.size(); ++i)
	{
		vertices[i].Pos = Sphere.Vertices[i].Position;
	}

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(L3DVertice::Pos);

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

	mSkyMesh->VertexByteStride = sizeof(L3DVertice::Pos);
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

void Sky::BuildMaterials()
{
	L3DMaterial::GetL3DMaterialInstance()->BuildMaterials(
		L"sky",
		0.0f,
		XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
		XMFLOAT3(0.1f, 0.1f, 0.1f),
		MathHelper::Identity4x4(),
		MaterialType::SkyMat);
}

void Sky::FillSRVDescriptorHeap(int* SRVIndex, CD3DX12_CPU_DESCRIPTOR_HANDLE* hDescriptor)
{
	auto SkyTex = pTextureManage->GetTexture(L"HDRSky\\Sky")->Resource;

	D3D12_SHADER_RESOURCE_VIEW_DESC SkysrvDesc = {};
	SkysrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	SkysrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	SkysrvDesc.TextureCube.MostDetailedMip = 0;
	SkysrvDesc.TextureCube.MipLevels = SkyTex->GetDesc().MipLevels;
	SkysrvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
	SkysrvDesc.Format = SkyTex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(SkyTex.Get(), &SkysrvDesc, *hDescriptor);
	L3DMaterial::GetL3DMaterialInstance()->GetMaterial(L"sky")->PBRSrvHeapIndex[PBRTextureType::DiffuseSrvHeapIndex] = (*SRVIndex)++;
	// next descriptor
	(*hDescriptor).Offset(1, pTextureManage->GetCbvSrvDescriptorSize());
}
