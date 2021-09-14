#include "Grass.h"
#include "Vertex.h"
#include "TextureManage.h"
#include "Material.h"

Grass::Grass(ID3D12Device* device, 
	ID3D12GraphicsCommandList* CommandList,
	TextureManage* TextureManage,
	float GrassWidth, float GrassDepth, 
	int VertRows, int VertCols)
:mDevice(device),
mNumVertRows(VertRows),
mNumVertCols(VertCols),
mGrassWidth(GrassWidth),
mGrassDepth(GrassDepth),
mCommandList(CommandList),
mTextureManage(TextureManage)
{
	BuildGrassVB();
}

Grass::~Grass()
{
}

void Grass::BuildGrassVB()
{
	std::vector<Vertices::Grass> vertices(mNumVertRows * mNumVertCols);
	float halfWidth = 0.5f * mGrassWidth;
	float halfDepth = 0.5f * mGrassDepth;

	float PerGrassWidth = mGrassWidth / (mNumVertCols - 1);
	float PerGrassDepth = mGrassDepth / (mNumVertRows - 1);

	float du = 1.0f / (mNumVertCols - 1);
	float dv = 1.0f / (mNumVertRows - 1);

	for (UINT i = 0; i < mNumVertRows; ++i)
	{
		float z = halfDepth - i * PerGrassWidth;
		for (UINT j = 0; j < mNumVertCols; ++j)
		{
			float x = -halfWidth + j * PerGrassDepth;
			vertices[i * mNumVertCols + j].Position = XMFLOAT3(x+MathHelper::RandF()*5, 0.0f, z+MathHelper::RandF()*5);
			// Stretch texture over grid.
			vertices[i * mNumVertCols + j].Tex.x = j * du;
			vertices[i * mNumVertCols + j].Tex.y = i * dv;
		}
	}

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertices::Grass);


	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "GrassGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);


	geo->VertexBufferGPU = EngineUtility::CreateDefaultBuffer(mDevice,
		mCommandList, vertices.data(), vbByteSize, geo->VertexBufferUploader);

	
	geo->VertexByteStride = sizeof(Vertices::Grass);
	geo->VertexBufferByteSize = vbByteSize;


	SubmeshGeometry submesh;
	submesh.VertexCount = (UINT)vertices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["Grass"] = submesh;

	mGeometries = std::move(geo);
}

void Grass::BuildMaterials()
{
	XMFLOAT4X4  MatTransform = MathHelper::Identity4x4();

	//PBRGrass Material
	Materials::GetMaterialInstance()->BuildMaterials(
		L"PBRGrass",
		0.01f,
		XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
		XMFLOAT3(0.1f, 0.1f, 0.1f),
		MatTransform,
		MaterialType::NormalPBRMat);
}

void Grass::FillSRVDescriptorHeap(int* SRVIndex, CD3DX12_CPU_DESCRIPTOR_HANDLE* hDescriptor)
{
	vector<ID3D12Resource*> PBRTexResource;
	PBRTexResource.resize(PBRTextureType::Count);
	wstring e = L"PBRGrass";
	PBRTexResource[PBRTextureType::DiffuseSrvHeapIndex] = mTextureManage->GetTexture(e + L"\\" + e + L"_albedo")->Resource.Get();
	PBRTexResource[PBRTextureType::NormalSrvHeapIndex] = mTextureManage->GetTexture(e + L"\\" + e + L"_normal")->Resource.Get();
	PBRTexResource[PBRTextureType::AoSrvHeapIndex] = mTextureManage->GetTexture(e + L"\\" + e + L"_ao")->Resource.Get();
	PBRTexResource[PBRTextureType::MetallicSrvHeapIndex] = mTextureManage->GetTexture(e + L"\\" + e + L"_metallic")->Resource.Get();
	PBRTexResource[PBRTextureType::RoughnessSrvHeapIndex] = mTextureManage->GetTexture(e + L"\\" + e + L"_roughness")->Resource.Get();
	PBRTexResource[PBRTextureType::HeightSrvHeapIndex] = mTextureManage->GetTexture(e + L"\\" + e + L"_height")->Resource.Get();

	mTextureManage->CreatePBRSRVinDescriptorHeap(PBRTexResource, SRVIndex, hDescriptor, e);

	//RandomTex
	auto RandomTex = mTextureManage->GetTexture(L"RGB_Noise")->Resource;

	D3D12_SHADER_RESOURCE_VIEW_DESC GrassSrvDesc = {};
	GrassSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	GrassSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	GrassSrvDesc.TextureCube.MostDetailedMip = 0;
	GrassSrvDesc.TextureCube.MipLevels = RandomTex->GetDesc().MipLevels;
	GrassSrvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
	GrassSrvDesc.Format = RandomTex->GetDesc().Format;
	mDevice->CreateShaderResourceView(RandomTex.Get(), &GrassSrvDesc, *hDescriptor);
	mRGBNoiseMapIndex = (*SRVIndex)++;
	// next descriptor
	(*hDescriptor).Offset(1, mTextureManage->GetCbvSrvDescriptorSize());
}


