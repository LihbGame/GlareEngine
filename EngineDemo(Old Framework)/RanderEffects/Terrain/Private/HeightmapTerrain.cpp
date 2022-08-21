#include "HeightmapTerrain.h"
#include "GeometryGenerator.h"
#include "Vertex.h"
#include "Grass.h"
#include "Material.h"
#include "stb_image.h"

using namespace DirectX::PackedVector;

HeightmapTerrain::HeightmapTerrain(
	ID3D12Device* device, 
	ID3D12GraphicsCommandList* CommandList,
	TextureManage* TextureManage,
	InitInfo initInfo,
	ID3D12Resource* RandomTexSRV)
	:mDevice(device),
	mCommandList(CommandList),
	mInfo(initInfo),
	pTextureManage(TextureManage)
{
	XMStoreFloat4x4(&mWorld, XMMatrixIdentity());

	// Divide height map into patches such that each patch has CellsPerPatch.
	mNumPatchVertRows = ((mInfo.HeightmapHeight - 1) / CellsPerPatch) + 1;
	mNumPatchVertCols = ((mInfo.HeightmapWidth - 1) / CellsPerPatch) + 1;

	mNumPatchVertices = mNumPatchVertRows * mNumPatchVertCols;
	mNumPatchQuadFaces = (mNumPatchVertRows - 1) * (mNumPatchVertCols - 1);

	//Load textures
	LoadHeightmapAsset();
	//Smooth Height map
	Smooth();
	//bounds box
	CalcAllPatchBoundsY();
	//Geometry VB & IB
	BuildQuadPatchGeometry();

	//init Grass
	mGrass = std::make_unique<Grass>(device, CommandList, TextureManage, 
		this->GetWidth(), this->GetDepth(), 
		2500, 2500);

}

HeightmapTerrain::~HeightmapTerrain()
{
}

float HeightmapTerrain::GetWidth() const
{
	// Total terrain width.
	return (mInfo.HeightmapWidth - 1) * mInfo.CellSpacing;
}

float HeightmapTerrain::GetDepth() const
{
	// Total terrain depth.
	return (mInfo.HeightmapHeight - 1) * mInfo.CellSpacing;
}

float HeightmapTerrain::GetHeight(float x, float z) const
{
	// Transform from terrain local space to "cell" space.
	float c = (x + 0.5f * GetWidth()) / mInfo.CellSpacing;
	float d = (z - 0.5f * GetDepth()) / -mInfo.CellSpacing;

	// Get the row and column we are in.
	int row = (int)floorf(d);
	int col = (int)floorf(c);

	// Grab the heights of the cell we are in.
	// A*--*B
	//  | /|
	//  |/ |
	// C*--*D
	float A = mHeightmap[row * mInfo.HeightmapWidth + col];
	float B = mHeightmap[row * mInfo.HeightmapWidth + col + 1];
	float C = mHeightmap[(row + 1) * mInfo.HeightmapWidth + col];
	float D = mHeightmap[(row + 1) * mInfo.HeightmapWidth + col + 1];

	// Where we are relative to the cell.
	float s = c - (float)col;
	float t = d - (float)row;

	// If upper triangle ABC.
	if (s + t <= 1.0f)
	{
		float uy = B - A;
		float vy = C - A;
		return A + s * uy + t * vy;
	}
	else // lower triangle DCB.
	{
		float uy = C - D;
		float vy = B - D;
		return D + (1.0f - s) * uy + (1.0f - t) * vy;
	}
}

::MeshGeometry* HeightmapTerrain::GetMeshGeometry() const
{
	return mGeometries.get();
}

XMMATRIX HeightmapTerrain::GetWorld() const
{
	return XMLoadFloat4x4(&mWorld);
}

void HeightmapTerrain::SetWorld(CXMMATRIX M)
{
	XMStoreFloat4x4(&mWorld, M);
}


void HeightmapTerrain::Update(float dt)
{


}

void HeightmapTerrain::BuildMaterials()
{
	XMFLOAT4X4  MatTransform = MathHelper::Identity4x4();
	for (auto e : TerrainTextures)
	{
		Materials::GetMaterialInstance()->BuildMaterials(
			std::wstring(e.first.begin(), e.first.end()),
			0.08f,
			XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
			XMFLOAT3(0.1f, 0.1f, 0.1f),
			MatTransform,
			MaterialType::HeightMapTerrainPBRMat,
			pTextureManage->GetNumFrameResources());
	}

	mGrass->BuildMaterials();
}

void HeightmapTerrain::FillSRVDescriptorHeap(int* SRVIndex, CD3DX12_CPU_DESCRIPTOR_HANDLE* hDescriptor)
{
	std::vector<ID3D12Resource*> PBRTexResource;
	PBRTexResource.resize(PBRTextureType::Count);
	for (auto& e : TerrainTextures)
	{
		PBRTexResource[PBRTextureType::DiffuseSrvHeapIndex] = e.second[0]->Resource.Get();
		PBRTexResource[PBRTextureType::NormalSrvHeapIndex] = e.second[1]->Resource.Get();
		PBRTexResource[PBRTextureType::AoSrvHeapIndex] = e.second[2]->Resource.Get();
		PBRTexResource[PBRTextureType::MetallicSrvHeapIndex] = e.second[3]->Resource.Get();
		PBRTexResource[PBRTextureType::RoughnessSrvHeapIndex] = e.second[4]->Resource.Get();
		PBRTexResource[PBRTextureType::HeightSrvHeapIndex] = e.second[5]->Resource.Get();

		pTextureManage->CreatePBRSRVinDescriptorHeap(PBRTexResource, SRVIndex, hDescriptor, std::wstring(e.first.begin(), e.first.end()));
	}


	CD3DX12_CPU_DESCRIPTOR_HANDLE BlendMapDescriptor = *hDescriptor;
	(*hDescriptor).Offset(1, pTextureManage->GetCbvSrvDescriptorSize());
	mBlendMapIndex = (*SRVIndex)++;

	CD3DX12_CPU_DESCRIPTOR_HANDLE HeightMapDescriptor = *hDescriptor;
	(*hDescriptor).Offset(1, pTextureManage->GetCbvSrvDescriptorSize());
	mHeightMapIndex = (*SRVIndex)++;

	BuildHeightmapSRV(BlendMapDescriptor, HeightMapDescriptor);
}

void HeightmapTerrain::LoadHeightMapFromFile(std::string filename)
{
	//check file type
	std::string FileType = filename.substr(filename.find_last_of('.') + 1);
	transform(FileType.begin(), FileType.end(), FileType.begin(), ::toupper);

	if (FileType == "RAW")
	{
		UINT MapSize = mInfo.HeightmapHeight * mInfo.HeightmapWidth;
		mHeightmap.resize(MapSize, 0);
		std::vector<unsigned char> MapData(MapSize);
		// Open the file.
		std::ifstream inFile;
		inFile.open(mInfo.HeightMapFilename.c_str(), std::ios_base::binary);
		if (inFile)
		{
			// Read the RAW bytes.
			inFile.read((char*)&MapData[0], (std::streamsize)MapData.size());
			// Done with file.
			inFile.close();

			for (UINT i = 0; i < MapSize; ++i)
			{
				mHeightmap[i] = MapData[i] / 255.0f * mInfo.HeightScale;
			}
		}
	}
	else
	{
		int width, height, numComponents;
		float* MapData = stbi_loadf(mInfo.HeightMapFilename.c_str(), &width, &height, &numComponents, 1);
		mInfo.HeightmapHeight = height;
		mInfo.HeightmapWidth = width;
		UINT MapSize = mInfo.HeightmapHeight * mInfo.HeightmapWidth;
		mHeightmap.resize(MapSize, 0);

		for (UINT i = 0; i < MapSize; ++i)
		{
			mHeightmap[i] = MapData[i] * mInfo.HeightScale;
		}
		STBI_FREE(MapData);
	}
}

void HeightmapTerrain::LoadHeightmapAsset()
{
	std::vector<std::string> AllTerrainTextureNames;
	for (int i = 0; i < 5; ++i)
	{
		AllTerrainTextureNames.push_back(mInfo.LayerMapFilename[i] + "_albedo");
		AllTerrainTextureNames.push_back(mInfo.LayerMapFilename[i] + "_normal");
		AllTerrainTextureNames.push_back(mInfo.LayerMapFilename[i] + "_ao");
		AllTerrainTextureNames.push_back(mInfo.LayerMapFilename[i] + "_metallic");
		AllTerrainTextureNames.push_back(mInfo.LayerMapFilename[i] + "_roughness");
		AllTerrainTextureNames.push_back(mInfo.LayerMapFilename[i] + "_height");
	}

	for (int i=0;i<AllTerrainTextureNames.size();++i)
	{
		TerrainTextures[mInfo.LayerMapFilename[i/6]].push_back(pTextureManage->GetTexture(std::wstring(AllTerrainTextureNames[i].begin(), AllTerrainTextureNames[i].end())).get());
	}

	LoadHeightMapFromFile(mInfo.HeightMapFilename.c_str());
}

void HeightmapTerrain::Smooth()
{
	std::vector<float> dest(mHeightmap.size());
	int Index = 0;
	for (UINT i = 0; i < mInfo.HeightmapHeight; ++i)
	{
		for (UINT j = 0; j < mInfo.HeightmapWidth; ++j)
		{
			Index = i * mInfo.HeightmapWidth + j;
			dest[Index] = Average(i, j);
		}
	}

	// Replace the old height map with the filtered one.
	mHeightmap = dest;
}

bool HeightmapTerrain::InBounds(int i, int j)
{
	// True if ij are valid indices; false otherwise.
	return
		i >= 0 && i < (int)mInfo.HeightmapHeight&&
		j >= 0 && j < (int)mInfo.HeightmapWidth;
}

float HeightmapTerrain::Average(int i, int j)
{
	// Function computes the average height of the ij element.
	// It averages itself with its eight neighbor pixels.  Note
	// that if a pixel is missing neighbor, we just don't include it
	// in the average--that is, edge pixels don't have a neighbor pixel.
	//
	// ----------
	// | 1| 2| 3|
	// ----------
	// |4 |ij| 6|
	// ----------
	// | 7| 8| 9|
	// ----------

	float avg = 0.0f;
	float num = 0.0f;

	// Use int to allow negatives.  If we use UINT, @ i=0, m=i-1=UINT_MAX
	// and no iterations of the outer for loop occur.
	for (int m = i - 1; m <= i + 1; ++m)
	{
		for (int n = j - 1; n <= j + 1; ++n)
		{
			if (InBounds(m, n))
			{
				avg += mHeightmap[m * mInfo.HeightmapWidth + n];
				num += 1.0f;
			}
		}
	}

	return avg / num;
}

void HeightmapTerrain::CalcAllPatchBoundsY()
{
	mPatchBoundsY.resize(mNumPatchQuadFaces);

	// For each patch
	for (UINT i = 0; i < mNumPatchVertRows - 1; ++i)
	{
		for (UINT j = 0; j < mNumPatchVertCols - 1; ++j)
		{
			CalcPatchBoundsY(i, j);
		}
	}
}

void HeightmapTerrain::CalcPatchBoundsY(UINT i, UINT j)
{
	// Scan the height map values this patch covers and compute the min/max height.

	UINT x0 = j * CellsPerPatch;
	UINT x1 = (j + 1) * CellsPerPatch;

	UINT y0 = i * CellsPerPatch;
	UINT y1 = (i + 1) * CellsPerPatch;

	float minY = +MathHelper::Infinity;
	float maxY = -MathHelper::Infinity;
	for (UINT y = y0; y <= y1; ++y)
	{
		for (UINT x = x0; x <= x1; ++x)
		{
			UINT k = y * mInfo.HeightmapWidth + x;
			minY = MathHelper::Min(minY, mHeightmap[k]);
			maxY = MathHelper::Max(maxY, mHeightmap[k]);
		}
	}

	UINT patchID = i * (mNumPatchVertCols - 1) + j;
	mPatchBoundsY[patchID] = XMFLOAT2(minY, maxY);
}

void HeightmapTerrain::BuildQuadPatchGeometry()
{
//Vertices
#pragma region Vertices data
	std::vector<Vertices::Terrain> patchVertices(mNumPatchVertRows * mNumPatchVertCols);

	float halfWidth = 0.5f * GetWidth();
	float halfDepth = 0.5f * GetDepth();

	float patchWidth = GetWidth() / (mNumPatchVertCols - 1);
	float patchDepth = GetDepth() / (mNumPatchVertRows - 1);
	float du = 1.0f / (mNumPatchVertCols - 1);
	float dv = 1.0f / (mNumPatchVertRows - 1);

	for (UINT i = 0; i < mNumPatchVertRows; ++i)
	{
		float z = halfDepth - i * patchDepth;
		for (UINT j = 0; j < mNumPatchVertCols; ++j)
		{
			float x = -halfWidth + j * patchWidth;

			patchVertices[i * mNumPatchVertCols + j].Position = XMFLOAT3(x, 0.0f, z);

			// Stretch texture over grid.
			patchVertices[i * mNumPatchVertCols + j].Tex.x = j * du;
			patchVertices[i * mNumPatchVertCols + j].Tex.y = i * dv;
		}
	}

	// Store axis-aligned bounding box y-bounds in upper-left patch corner.
	for (UINT i = 0; i < mNumPatchVertRows - 1; ++i)
	{
		for (UINT j = 0; j < mNumPatchVertCols - 1; ++j)
		{
			UINT patchID = i * (mNumPatchVertCols - 1) + j;
			patchVertices[i * mNumPatchVertCols + j].BoundsY = mPatchBoundsY[patchID];
		}
	}

	std::vector<Vertices::Terrain> vertices(patchVertices.size());
	for (size_t i = 0; i < patchVertices.size(); ++i)
	{
		vertices[i].Position = patchVertices[i].Position;
		vertices[i].Tex = patchVertices[i].Tex;
		vertices[i].BoundsY = patchVertices[i].BoundsY;
	}

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertices::Terrain);
#pragma endregion

//Indices
#pragma region Indices data
	std::vector<USHORT> indices(mNumPatchQuadFaces * 4); // 4 indices per quad face
	// Iterate over each quad and compute indices.
	int k = 0;
	for (UINT i = 0; i < mNumPatchVertRows - 1; ++i)
	{
		for (UINT j = 0; j < mNumPatchVertCols - 1; ++j)
		{
			// Top row of 2x2 quad patch
			indices[k] = i * mNumPatchVertCols + j;
			indices[k + 1] = i * mNumPatchVertCols + j + 1;

			// Bottom row of 2x2 quad patch
			indices[k + 2] = (i + 1) * mNumPatchVertCols + j;
			indices[k + 3] = (i + 1) * mNumPatchVertCols + j + 1;

			k += 4; // next quad
		}
	}
	UINT ibByteSize = (UINT)indices.size() * sizeof(USHORT);
#pragma endregion


	auto geo = std::make_unique<::MeshGeometry>();
	geo->Name = L"TerrainGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = EngineUtility::CreateDefaultBuffer(mDevice,
		mCommandList, vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = EngineUtility::CreateDefaultBuffer(mDevice,
		mCommandList, indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertices::Terrain);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	::SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs[L"HeightMapTerrain"] = submesh;

	mGeometries= std::move(geo);
}

void HeightmapTerrain::BuildHeightmapSRV(CD3DX12_CPU_DESCRIPTOR_HANDLE BlendMapDescriptor, CD3DX12_CPU_DESCRIPTOR_HANDLE HeightMapDescriptor)
{
	//blend map
	auto BlendMapTex = pTextureManage->GetTexture(std::wstring(mInfo.BlendMapFilename.begin(), mInfo.BlendMapFilename.end()))->Resource;
	D3D12_SHADER_RESOURCE_VIEW_DESC BlendMapDesc = {};
	BlendMapDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	BlendMapDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	BlendMapDesc.Texture2D.MostDetailedMip = 0;
	BlendMapDesc.Texture2D.MipLevels = BlendMapTex->GetDesc().MipLevels;
	BlendMapDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	BlendMapDesc.Format = BlendMapTex->GetDesc().Format;
	mDevice->CreateShaderResourceView(BlendMapTex.Get(), &BlendMapDesc, BlendMapDescriptor);

	//height map
	std::vector<HALF> HalfHeightMapData;
	HalfHeightMapData.resize(mHeightmap.size());
	std::transform(mHeightmap.begin(), mHeightmap.end(), HalfHeightMapData.begin(), XMConvertFloatToHalf);
	mHeightMapSRV = EngineUtility::CreateDefault2DTexture(mDevice,
		mCommandList, HalfHeightMapData.data(), sizeof(HALF) * mHeightmap.size(), DXGI_FORMAT_R16_FLOAT, mInfo.HeightmapWidth, mInfo.HeightmapHeight, mHeightMapUploader);
	
	D3D12_SHADER_RESOURCE_VIEW_DESC HeightMapDesc = BlendMapDesc;
	HeightMapDesc.Texture2D.MipLevels = -1;
	HeightMapDesc.Texture2D.MostDetailedMip = 0;
	HeightMapDesc.Format = DXGI_FORMAT_R16_FLOAT;
	mDevice->CreateShaderResourceView(mHeightMapSRV.Get(), &HeightMapDesc, HeightMapDescriptor);
}

void HeightmapTerrain::GetTerrainConstant(TerrainConstants& TerrainConstant)
{
	TerrainConstant.gMinDist = 200.0f;
	TerrainConstant.gMaxDist = mInfo.HeightmapWidth * mInfo.CellSpacing/2;
	TerrainConstant.gMinTess = 1.0f;
	TerrainConstant.gMaxTess = 5.0f;

	TerrainConstant.gTexelCellSpaceU = 1.0f / mInfo.HeightmapWidth;
	TerrainConstant.gTexelCellSpaceV = 1.0f / mInfo.HeightmapHeight;
	TerrainConstant.gWorldCellSpace = mInfo.CellSpacing;
}
