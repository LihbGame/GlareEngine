#include "SimpleGeoInstance.h"
#include "Material.h"
#include "TextureManage.h"
#include "ModelMesh.h"
using namespace GlareEngine;

SimpleGeoInstance::SimpleGeoInstance(ID3D12GraphicsCommandList* pCommandList, ID3D12Device* pd3dDevice,TextureManage* TextureManage)
{
	this->pd3dDevice = pd3dDevice;
	this->pCommandList = pCommandList;
	this->pTextureManage = TextureManage;
}





SimpleGeoInstance::~SimpleGeoInstance()
{

}



void SimpleGeoInstance::BuildSimpleGeometryMesh(SimpleGeometry GeoType)
{
	GeometryGenerator Geo;
	MeshData SimpleGeoMesh;
	std::wstring MeshName;
	XMFLOAT3 BoundsExtents = XMFLOAT3(0.0f, 0.0f, 0.0f);

	switch (GeoType)
	{
	case SimpleGeometry::Box:
	{
		SimpleGeoMesh = Geo.CreateBox(5, 5, 5, 0);
		MeshName = L"Box";
		BoundsExtents = XMFLOAT3(2.5f, 2.5f, 2.5f);
		break;
	}
	case SimpleGeometry::Sphere:
	{
		SimpleGeoMesh = Geo.CreateSphere(10.0f, 50, 50);
		BoundsExtents = XMFLOAT3(5.0f, 5.0f, 5.0f);
		MeshName = L"Sphere";
		break;
	}
	case SimpleGeometry::Cylinder:
	{
		SimpleGeoMesh = Geo.CreateCylinder(10.0f, 5.0f, 20.0f, 10, 10);
		BoundsExtents = XMFLOAT3(5.0f, 10.0f, 5.0f);
		MeshName = L"Cylinder";
		break;
	}
	case SimpleGeometry::Grid:
	{
		SimpleGeoMesh = Geo.CreateGrid(100, 100, 10, 10);
		BoundsExtents = XMFLOAT3(50.0f, 1.0f, 50.0f);
		MeshName = L"Grid";
		break;
	}
	case SimpleGeometry::Quad:
	{
		SimpleGeoMesh = Geo.CreateQuad(0.0f, 0.0f, 200.0f, 200.0f, 0.0f);
		MeshName = L"Quad";
		break;
	}
	default:
		break;
	}


	std::vector<Vertices::PosNormalTangentTex> vertices(SimpleGeoMesh.Vertices.size());
	for (size_t i = 0; i < SimpleGeoMesh.Vertices.size(); ++i)
	{
		vertices[i].Position = SimpleGeoMesh.Vertices[i].Position;
		vertices[i].Normal = SimpleGeoMesh.Vertices[i].Normal;
		vertices[i].Tangent = SimpleGeoMesh.Vertices[i].Tangent;
		vertices[i].Texc = SimpleGeoMesh.Vertices[i].Texc;
	}
	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertices::PosNormalTangentTex);
	std::vector<std::uint16_t> indices = SimpleGeoMesh.GetIndices16();
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);



	mSimpleMesh = std::make_unique<::MeshGeometry>();
	mSimpleMesh->Name = MeshName;

	//CPU Vertex Buffer
	ThrowIfFailed(D3DCreateBlob(vbByteSize, &mSimpleMesh->VertexBufferCPU));
	CopyMemory(mSimpleMesh->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);


	//CPU Index Buffer
	ThrowIfFailed(D3DCreateBlob(ibByteSize, &mSimpleMesh->IndexBufferCPU));
	CopyMemory(mSimpleMesh->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	//GPU Buffer
	mSimpleMesh->VertexBufferGPU = EngineUtility::CreateDefaultBuffer(pd3dDevice,
		pCommandList, vertices.data(), vbByteSize, mSimpleMesh->VertexBufferUploader);


	mSimpleMesh->IndexBufferGPU = EngineUtility::CreateDefaultBuffer(pd3dDevice,
		pCommandList, indices.data(), ibByteSize, mSimpleMesh->IndexBufferUploader);

	mSimpleMesh->VertexByteStride = sizeof(Vertices::PosNormalTangentTex);
	mSimpleMesh->VertexBufferByteSize = vbByteSize;
	mSimpleMesh->IndexFormat = DXGI_FORMAT_R16_UINT;
	mSimpleMesh->IndexBufferByteSize = ibByteSize;

	::SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;
	submesh.Bounds.Center = XMFLOAT3(0.0f, 0.0f, 0.0f);
	submesh.Bounds.Extents = BoundsExtents;
	mSimpleMesh->DrawArgs[MeshName] = submesh;//now only have one submesh we use mesh name
}



std::unique_ptr<::MeshGeometry>& SimpleGeoInstance::GetSimpleGeoMesh(SimpleGeometry GeoType)

{
	BuildSimpleGeometryMesh(GeoType);
	return mSimpleMesh;

}

void SimpleGeoInstance::BuildMaterials()
{
	XMFLOAT4X4  MatTransform = MathHelper::Identity4x4();

	Materials::GetMaterialInstance()->BuildMaterials(
		L"PBRwhite_spruce_tree_bark",
		0.09f,
		XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
		XMFLOAT3(0.1f, 0.1f, 0.1f),
		MatTransform,
		MaterialType::NormalPBRMat,
		pTextureManage->GetNumFrameResources());
	mPBRTextureName.push_back(L"PBRwhite_spruce_tree_bark");

	//PBRharshbricks Material
	Materials::GetMaterialInstance()->BuildMaterials(
		L"PBRharshbricks",
		0.05f,
		XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
		XMFLOAT3(0.1f, 0.1f, 0.1f),
		MatTransform,
		MaterialType::NormalPBRMat,
		pTextureManage->GetNumFrameResources());
	mPBRTextureName.push_back(L"PBRharshbricks");

	//PBRrocky_shoreline1 Material
	Materials::GetMaterialInstance()->BuildMaterials(
		L"PBRrocky_shoreline1",
		0.05f,
		XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
		XMFLOAT3(0.1f, 0.1f, 0.1f),
		MatTransform,
		MaterialType::NormalPBRMat,
		pTextureManage->GetNumFrameResources());
	mPBRTextureName.push_back(L"PBRrocky_shoreline1");

	//PBRstylized_grass1 Material
	Materials::GetMaterialInstance()->BuildMaterials(
		L"PBRstylized_grass1",
		0.09f,
		XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
		XMFLOAT3(0.1f, 0.1f, 0.1f),
		MatTransform,
		MaterialType::NormalPBRMat,
		pTextureManage->GetNumFrameResources());
	mPBRTextureName.push_back(L"PBRstylized_grass1");

	//PBRIndustrial_narrow_brick Material
	Materials::GetMaterialInstance()->BuildMaterials(
		L"PBRIndustrial_narrow_brick",
		0.05f,
		XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
		XMFLOAT3(0.1f, 0.1f, 0.1f),
		MatTransform,
		MaterialType::NormalPBRMat,
		pTextureManage->GetNumFrameResources());
	mPBRTextureName.push_back(L"PBRIndustrial_narrow_brick");

	//PBRBrass Material
	XMStoreFloat4x4(&MatTransform, XMMatrixScaling(0.1f, 0.1f, 0.1f));
	Materials::GetMaterialInstance()->BuildMaterials(
		L"PBRBrass",
		0.01f,
		XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
		XMFLOAT3(0.1f, 0.1f, 0.1f),
		MatTransform,
		MaterialType::NormalPBRMat,
		pTextureManage->GetNumFrameResources());
	mPBRTextureName.push_back(L"PBRBrass");
#pragma endregion

}

void SimpleGeoInstance::FillSRVDescriptorHeap(int* SRVIndex, CD3DX12_CPU_DESCRIPTOR_HANDLE* hDescriptor)
{
	std::vector<ID3D12Resource*> PBRTexResource;
	PBRTexResource.resize(PBRTextureType::Count);
	for (auto& e : mPBRTextureName)
	{
		PBRTexResource[PBRTextureType::DiffuseSrvHeapIndex] = pTextureManage->GetTexture(e + L"\\" + e + L"_albedo")->Resource.Get();
		PBRTexResource[PBRTextureType::NormalSrvHeapIndex] = pTextureManage->GetTexture(e + L"\\" + e + L"_normal")->Resource.Get();
		PBRTexResource[PBRTextureType::AoSrvHeapIndex] = pTextureManage->GetTexture(e + L"\\" + e + L"_ao")->Resource.Get();
		PBRTexResource[PBRTextureType::MetallicSrvHeapIndex] = pTextureManage->GetTexture(e + L"\\" + e + L"_metallic")->Resource.Get();
		PBRTexResource[PBRTextureType::RoughnessSrvHeapIndex] = pTextureManage->GetTexture(e + L"\\" + e + L"_roughness")->Resource.Get();
		PBRTexResource[PBRTextureType::HeightSrvHeapIndex] = pTextureManage->GetTexture(e + L"\\" + e + L"_height")->Resource.Get();

		pTextureManage->CreatePBRSRVinDescriptorHeap(PBRTexResource, SRVIndex, hDescriptor, e);
	}

}
