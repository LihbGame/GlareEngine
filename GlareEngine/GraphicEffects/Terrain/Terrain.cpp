#include "Terrain.h"
#include "Engine/GeometryGenerator.h"
#include "Engine/Vertex.h"
#include "Engine/EngineGlobal.h"
#include "Engine/Scene.h"
#include "EngineGUI.h"
#include "stb_image/stb_image.h"


using namespace DirectX::PackedVector;

GraphicsPSO Terrain::mPSO;

const char* TerrainLayerPBRMaterials[]
{
	"grass",
	"darkdirt",
	"stone",
	"lightdirt",
	"snow"
};



Terrain::Terrain(ID3D12GraphicsCommandList* CommandList,TerrainInitInfo TerrainInfo)
	:mTerrainInfo(TerrainInfo),
	m_pCommandList(CommandList)
{
	mObjectType = ObjectType::Terrain;

	mTileNum = mTerrainInfo.TerrainTileSize * mTerrainInfo.TerrainTileSize;
	mWorldTransforms.reserve(mTileNum);

	for (UINT TileIndex=0; TileIndex < mTileNum;++TileIndex)
	{
		XMStoreFloat4x4(&mWorldTransforms[TileIndex], XMMatrixIdentity());
	}

	mTerrainTileWidth = mTerrainInfo.TerrainCellNumInTile * mTerrainInfo.CellSize;
	mTerrainSize = mTerrainInfo.TerrainTileSize * mTerrainTileWidth;

	mNumPatchVertRows = mTerrainInfo.TerrainCellNumInTile + 1;
	mNumPatchVertCols = mNumPatchVertRows;

	mNumPatchVertices = mNumPatchVertRows * mNumPatchVertCols;
	mNumPatchQuadFaces = (mNumPatchVertRows - 1) * (mNumPatchVertCols - 1);

	//Load Asset
	LoadHeightMapAsset();
	//Smooth Height map
	Smooth();
	//Bounds Box
	CalcAllPatchBoundsY();
	//Geometry VB & IB
	BuildQuadPatchGeometry(m_pCommandList);
	//build terrain transform
	BuildTerrainTransform();
}


Terrain::~Terrain()
{
}

void Terrain::Update(float dt)
{
	UpdateTerrainConstantBuffer();

}

void Terrain::DrawUI()
{
	if (ImGui::CollapsingHeader("Terrain", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::SliderFloat("Tess Scale", &mTessellationScale, 0.0f, 2.0f);
	}
}

void Terrain::Draw(GraphicsContext& Context, GraphicsPSO* SpecificPSO)
{
}

float Terrain::GetWidth() const
{
	return mTerrainSize;

}

float Terrain::GetDepth() const
{
	return mTerrainSize;
}

float Terrain::GetHeight(float x, float z) const
{
	return 0.0f;
}

void Terrain::BuildPSO(const PSOCommonProperty CommonProperty)
{



}

void Terrain::CreateMaterials()
{
	int MaterialNum = sizeof(mTerrainInfo.LayerMapFilename) / sizeof(string);
	for (int i = 0; i < MaterialNum; ++i)
	{
		vector<Texture*> ModelTextures;
		string Filename = EngineGlobal::TerrainAssetPath;
		Filename += mTerrainInfo.LayerMapFilename[i];
		TextureManager::GetInstance(m_pCommandList)->CreatePBRTextures(Filename, ModelTextures);
		MaterialManager::GetMaterialInstance()->BuildMaterials(
			StringToWString(mTerrainInfo.LayerMapFilename[i]),
			ModelTextures,
			0.04f,
			XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
			XMFLOAT3(0.1f, 0.1f, 0.1f),
			MathHelper::Identity4x4());
	}
}

void Terrain::LoadHeightMapAsset()
{

	/// <summary>
	/// load height maps
	/// </summary>

	//check file type
	string FileType = mTerrainInfo.HeightMapFilename.substr(mTerrainInfo.HeightMapFilename.find_last_of('.') + 1);
	transform(FileType.begin(), FileType.end(), FileType.begin(), ::toupper);

	if (FileType == "RAW")
	{
		UINT MapSize = mTerrainInfo.HeightmapHeight * mTerrainInfo.HeightmapWidth;
		mHeightmap.resize(MapSize, 0);
		std::vector<unsigned char> MapData(MapSize);
		// Open the file.
		std::ifstream inFile;
		inFile.open(mTerrainInfo.HeightMapFilename.c_str(), std::ios_base::binary);
		if (inFile)
		{
			// Read the RAW bytes.
			inFile.read((char*)&MapData[0], (std::streamsize)MapData.size());
			// Done with file.
			inFile.close();

			for (UINT i = 0; i < MapSize; ++i)
			{
				mHeightmap[i] = MapData[i] / 255.0f * mTerrainInfo.HeightScale;
			}
		}
	}
	else
	{
		int width, height, numComponents;
		float* MapData = stbi_loadf(mTerrainInfo.HeightMapFilename.c_str(), &width, &height, &numComponents, 1);
		mTerrainInfo.HeightmapHeight = height;
		mTerrainInfo.HeightmapWidth = width;
		UINT MapSize = mTerrainInfo.HeightmapHeight * mTerrainInfo.HeightmapWidth;
		mHeightmap.resize(MapSize, 0);

		for (UINT i = 0; i < MapSize; ++i)
		{
			mHeightmap[i] = MapData[i] * mTerrainInfo.HeightScale;
		}
		STBI_FREE(MapData);
	}

	/// <summary>
	/// Build Terrain SRV
	/// </summary>
	BuildTerrainSRV();
	

	/// <summary>
	/// create terrain layer Materials
	/// </summary>
	CreateMaterials();
}


void Terrain::Smooth()
{
	std::vector<float> Dest(mHeightmap.size());
	int Index = 0;
	for (UINT i = 0; i < mTerrainInfo.HeightmapHeight; ++i)
	{
		for (UINT j = 0; j < mTerrainInfo.HeightmapWidth; ++j)
		{
			Index = i * mTerrainInfo.HeightmapWidth + j;
			Dest[Index] = Average(i, j);
		}
	}

	// Replace the old height map with the filtered one.
	mHeightmap = Dest;
}

bool Terrain::InBounds(int i, int j)
{
	// True if ij are valid indices; false otherwise.
	return
		i >= 0 && i < (int)mTerrainInfo.HeightmapHeight&&
		j >= 0 && j < (int)mTerrainInfo.HeightmapWidth;
}

float Terrain::Average(int i, int j)
{
	// Function computes the average height of the i j element.
	// It averages itself with its eight neighbor pixels.  Note
	// that if a pixel is missing neighbor, we just don't include it
	// in the average--that is, edge pixels don't have a neighbor pixel.
	//
	// ----------
	// | 1| 2| 3|
	// ----------
	// |4 |IJ| 6|
	// ----------
	// | 7| 8| 9|
	// ----------

	float avg = 0.0f;
	float num = 0.0f;

	// Use int to allow negatives.
	for (int m = i - 1; m <= i + 1; ++m)
	{
		for (int n = j - 1; n <= j + 1; ++n)
		{
			if (InBounds(m, n))
			{
				avg += mHeightmap[static_cast<size_t>(m) * mTerrainInfo.HeightmapWidth + n];
				num += 1.0f;
			}
		}
	}

	return avg / num;
}

void Terrain::CalcAllPatchBoundsY()
{
	mTileBoundsY.resize(mTileNum);
	mAllPatchBoundsY.resize(mTileNum);

	for (UINT TileID = 0; TileID < mTileNum; ++TileID)
	{
		mAllPatchBoundsY[TileID].resize(mNumPatchQuadFaces);
		// For each patch
		float minY = +MathHelper::Infinity;
		float maxY = -MathHelper::Infinity;
		for (UINT i = 0; i < mNumPatchVertRows - 1; ++i)
		{
			for (UINT j = 0; j < mNumPatchVertCols - 1; ++j)
			{
				XMFLOAT2  PatchBoundsY = CalcPatchBoundsY(i, j, TileID);
				minY = MathHelper::Min(minY, PatchBoundsY.x);
				maxY = MathHelper::Max(maxY, PatchBoundsY.y);
			}
		}
		mTileBoundsY[TileID] = XMFLOAT2(minY, maxY);
	}
}

XMFLOAT2 Terrain::CalcPatchBoundsY(UINT i, UINT j, UINT TileID)
{
	// Scan the height maps values this patch covers and compute the min/max height.
	UINT CellsPerTile = mTerrainInfo.HeightmapHeight / mTerrainInfo.TerrainTileSize;
	UINT CellsPerPatch = CellsPerTile / (mNumPatchVertRows - 1);

	UINT x0 = j * CellsPerPatch;
	UINT x1 = (j + 1) * CellsPerPatch;

	UINT y0 = i * CellsPerPatch;
	UINT y1 = (i + 1) * CellsPerPatch;

	float minY = +MathHelper::Infinity;
	float maxY = -MathHelper::Infinity;

	UINT TileRowOffset = TileID / mTerrainInfo.TerrainTileSize * CellsPerTile;
	UINT TileColOffset = TileID % mTerrainInfo.TerrainTileSize * CellsPerTile;

	for (UINT y = y0; y <= y1; ++y)
	{
		for (UINT x = x0; x <= x1; ++x)
		{
			UINT k = (y + TileRowOffset) * mTerrainInfo.HeightmapWidth + (x + TileColOffset);
			minY = MathHelper::Min(minY, mHeightmap[k]);
			maxY = MathHelper::Max(maxY, mHeightmap[k]);
		}
	}

	UINT patchID = i * (mNumPatchVertCols - 1) + j;
	mAllPatchBoundsY[TileID][patchID] = XMFLOAT2(minY, maxY);
	return XMFLOAT2(minY, maxY);
}

void Terrain::BuildQuadPatchGeometry(ID3D12GraphicsCommandList* CommandList)
{

	auto geo = std::make_unique<::MeshGeometry>();
	geo->Name = "TerrainGeometry";

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
			indices[static_cast<size_t>(k) + 1] = i * mNumPatchVertCols + j + 1;

			// Bottom row of 2x2 quad patch
			indices[static_cast<size_t>(k) + 2] = (i + 1) * mNumPatchVertCols + j;
			indices[static_cast<size_t>(k) + 3] = (i + 1) * mNumPatchVertCols + j + 1;

			k += 4; // next quad
		}
	}
	UINT ibByteSize = (UINT)indices.size() * sizeof(USHORT);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->IndexBufferGPU = EngineUtility::CreateDefaultBuffer(g_Device,
		CommandList, indices.data(), ibByteSize, geo->IndexBufferUploader);
#pragma endregion

	//Vertices
#pragma region Vertices data
	std::vector<Vertices::Terrain> patchVertices(mTileNum * mNumPatchVertices);

	float halfWidth = 0.5f * GetWidth();
	float halfDepth = 0.5f * GetDepth();

	float patchWidth = GetWidth() / (mNumPatchVertCols - 1);
	float patchDepth = GetDepth() / (mNumPatchVertRows - 1);


    float TileUV= 1.0f / mTerrainInfo.TerrainTileSize;
	float du = TileUV / (mNumPatchVertCols - 1);
	float dv = TileUV / (mNumPatchVertRows - 1);

	for (UINT TileID = 0; TileID < mTileNum; ++TileID)
	{
		UINT offset = TileID * mNumPatchVertices;
		float offsetV = TileID / mTerrainInfo.TerrainTileSize * TileUV;
		float offsetU = TileID % mTerrainInfo.TerrainTileSize * TileUV;
		for (UINT i = 0; i < mNumPatchVertRows; ++i)
		{
			float z = halfDepth - i * patchDepth;
			for (UINT j = 0; j < mNumPatchVertCols; ++j)
			{
				float x = -halfWidth + j * patchWidth;

				patchVertices[offset + static_cast<size_t>(i) * mNumPatchVertCols + j].Position = XMFLOAT3(x, 0.0f, z);

				// Stretch texture over grid.
				patchVertices[offset + static_cast<size_t>(i) * mNumPatchVertCols + j].Tex.x = j * du + offsetU;
				patchVertices[offset + static_cast<size_t>(i) * mNumPatchVertCols + j].Tex.y = i * dv + offsetV;
			}
		}

		// Store axis-aligned bounding box y-bounds in upper-left patch corner.
		for (UINT i = 0; i < mNumPatchVertRows - 1; ++i)
		{
			for (UINT j = 0; j < mNumPatchVertCols - 1; ++j)
			{
				UINT patchID = i * (mNumPatchVertCols - 1) + j;
				patchVertices[offset + static_cast<size_t>(i) * mNumPatchVertCols + j].BoundsY = mAllPatchBoundsY[TileID][patchID];
			}
		}

		SubmeshGeometry submesh;
		submesh.IndexCount = (UINT)indices.size();
		submesh.VertexCount = mNumPatchVertices;
		submesh.StartIndexLocation = 0;
		submesh.BaseVertexLocation = offset;

		string TileName = "Terrain ";
		TileName += "Row:" + to_string(TileID / mTerrainInfo.TerrainTileSize);
		TileName += " Col:" + to_string(TileID % mTerrainInfo.TerrainTileSize);
		geo->DrawArgs[TileName] = submesh;
	}

	const UINT vbByteSize = (UINT)patchVertices.size() * sizeof(Vertices::Terrain);

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), patchVertices.data(), vbByteSize);

	geo->VertexBufferGPU = EngineUtility::CreateDefaultBuffer(g_Device,
		CommandList, patchVertices.data(), vbByteSize, geo->VertexBufferUploader);
#pragma endregion
	
	geo->VertexByteStride = sizeof(Vertices::Terrain);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	mGeometries = std::move(geo);
}

void Terrain::BuildTerrainTransform()
{
	XMFLOAT4X4 TerrainTransform;
	int offset = mTerrainInfo.TerrainTileSize / 2;
	for (UINT i = 0; i < mTerrainInfo.TerrainTileSize; ++i)
	{
		for (UINT j = 0; j < mTerrainInfo.TerrainTileSize; ++j)
		{
			XMStoreFloat4x4(&TerrainTransform, XMMatrixTranslation((j - offset) * mTerrainTileWidth, 0.0f, (i - offset) * mTerrainTileWidth));
			mWorldTransforms.push_back(TerrainTransform);
		}
	}

}

void Terrain::UpdateTerrainConstantBuffer()
{
	assert(EngineGlobal::gCurrentScene);

	XMMATRIX viewProj = EngineGlobal::gCurrentScene->GetCamera()->GetView() * EngineGlobal::gCurrentScene->GetCamera()->GetProj();

	XMFLOAT4 worldPlanes[6];
	ExtractFrustumPlanes(worldPlanes, viewProj);
	int CopySize = sizeof(worldPlanes) * sizeof(XMFLOAT4);
	memcpy(mTerrainConstant.gWorldFrustumPlanes, worldPlanes, CopySize);

	mTerrainConstant.gBlendMapIndex = mBlendMapIndex;
	mTerrainConstant.gHeightMapIndex = mHeightMapIndex;

	mTerrainConstant.gTessellationScale = mTessellationScale;
	mTerrainConstant.gTexelCellSpaceU = 1.0f / mTerrainInfo.HeightmapWidth;
	mTerrainConstant.gTexelCellSpaceV = 1.0f / mTerrainInfo.HeightmapHeight;
	mTerrainConstant.gWorldCellSpace = mTerrainInfo.CellSize;
}

void Terrain::BuildTerrainSRV()
{
	//blend map
	auto BlendMapTex = TextureManager::GetInstance(m_pCommandList)->GetTexture(wstring(mTerrainInfo.BlendMapFilename.begin(), mTerrainInfo.BlendMapFilename.end()))->Resource;
	D3D12_SHADER_RESOURCE_VIEW_DESC BlendMapDesc = {};
	BlendMapDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	BlendMapDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	BlendMapDesc.Texture2D.MostDetailedMip = 0;
	BlendMapDesc.Texture2D.MipLevels = BlendMapTex->GetDesc().MipLevels;
	BlendMapDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	BlendMapDesc.Format = BlendMapTex->GetDesc().Format;
	m_BlendMapDescriptor = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	g_Device->CreateShaderResourceView(BlendMapTex.Get(), &BlendMapDesc, m_BlendMapDescriptor);
	mBlendMapIndex= AddToGlobalTextureSRVDescriptor(m_BlendMapDescriptor);

	//height map
	vector<HALF> HalfHeightMapData;
	HalfHeightMapData.resize(mHeightmap.size());
	std::transform(mHeightmap.begin(), mHeightmap.end(), HalfHeightMapData.begin(), XMConvertFloatToHalf);
	mHeightMapSRV = EngineUtility::CreateDefault2DTexture(g_Device,
		m_pCommandList, HalfHeightMapData.data(), sizeof(HALF) * mHeightmap.size(), DXGI_FORMAT_R16_FLOAT, mTerrainInfo.HeightmapWidth, mTerrainInfo.HeightmapHeight, mHeightMapUploader);

	D3D12_SHADER_RESOURCE_VIEW_DESC HeightMapDesc = BlendMapDesc;
	HeightMapDesc.Texture2D.MipLevels = -1;
	HeightMapDesc.Texture2D.MostDetailedMip = 0;
	HeightMapDesc.Format = DXGI_FORMAT_R16_FLOAT;
	m_HeightMapDescriptor = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	g_Device->CreateShaderResourceView(mHeightMapSRV.Get(), &HeightMapDesc, m_HeightMapDescriptor);
	mHeightMapIndex = AddToGlobalTextureSRVDescriptor(m_HeightMapDescriptor);
}
