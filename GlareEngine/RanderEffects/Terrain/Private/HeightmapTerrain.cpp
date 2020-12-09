#include "HeightmapTerrain.h"
#include "L3DGeometryGenerator.h"
#include "L3DVertex.h"

using namespace DirectX::PackedVector;

HeightmapTerrain::HeightmapTerrain(
	ID3D12Device* device, 
	ID3D12GraphicsCommandList* CommandList,
	L3DTextureManage* TextureManage,
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
	return 0.0f;
}

MeshGeometry* HeightmapTerrain::GetMeshGeometry() const
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

void HeightmapTerrain::LoadHeightmapAsset()
{
	vector<string> AllTerrainTextureNames;
	for (int i = 0; i < 5; ++i)
	{
		AllTerrainTextureNames.push_back(mInfo.LayerMapFilename[i] + "_albedo");
		AllTerrainTextureNames.push_back(mInfo.LayerMapFilename[i] + "_ao");
		AllTerrainTextureNames.push_back(mInfo.LayerMapFilename[i] + "_metallic");
		AllTerrainTextureNames.push_back(mInfo.LayerMapFilename[i] + "_normal");
		AllTerrainTextureNames.push_back(mInfo.LayerMapFilename[i] + "_roughness");
		AllTerrainTextureNames.push_back(mInfo.LayerMapFilename[i] + "_height");
	}

	for (int i=0;i<AllTerrainTextureNames.size();++i)
	{
		TerrainTextures[mInfo.LayerMapFilename[i/6]].push_back(pTextureManage->GetTexture(wstring(AllTerrainTextureNames[i].begin(), AllTerrainTextureNames[i].end())).get());
	}


	// A height for each vertex
	std::vector<unsigned char> in(mInfo.HeightmapWidth * mInfo.HeightmapHeight);

	// Open the file.
	std::ifstream inFile;
	inFile.open(mInfo.HeightMapFilename.c_str(), std::ios_base::binary);

	if (inFile)
	{
		// Read the RAW bytes.
		inFile.read((char*)&in[0], (std::streamsize)in.size());
		// Done with file.
		inFile.close();
	}

	// Copy the array data into a float array and scale it.
	mHeightmap.resize(mInfo.HeightmapHeight * mInfo.HeightmapWidth, 0);
	for (UINT i = 0; i < mInfo.HeightmapHeight * mInfo.HeightmapWidth; ++i)
	{
		mHeightmap[i] = (in[i] / 255.0f) * mInfo.HeightScale;
	}

}

void HeightmapTerrain::Smooth()
{
	std::vector<float> dest(mHeightmap.size());

	for (UINT i = 0; i < mInfo.HeightmapHeight; ++i)
	{
		for (UINT j = 0; j < mInfo.HeightmapWidth; ++j)
		{
			dest[i * mInfo.HeightmapWidth + j] = Average(i, j);
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
	std::vector<L3DVertice::Terrain> patchVertices(mNumPatchVertRows * mNumPatchVertCols);

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

			patchVertices[i * mNumPatchVertCols + j].Pos = XMFLOAT3(x, 0.0f, z);

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

	std::vector<L3DVertice::Terrain> vertices(patchVertices.size());
	for (size_t i = 0; i < patchVertices.size(); ++i)
	{
		vertices[i].Pos = patchVertices[i].Pos;
		vertices[i].Tex = patchVertices[i].Tex;
	}

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(L3DVertice::Terrain);
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


	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "TerrainGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = L3DUtil::CreateDefaultBuffer(mDevice,
		mCommandList, vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = L3DUtil::CreateDefaultBuffer(mDevice,
		mCommandList, indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(L3DVertice::Terrain);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["HeightMapTerrain"] = submesh;

	mGeometries= std::move(geo);
}

void HeightmapTerrain::BuildHeightmapSRV(CD3DX12_CPU_DESCRIPTOR_HANDLE BlendMapDescriptor, CD3DX12_CPU_DESCRIPTOR_HANDLE HeightMapDescriptor)
{
	//blend map
	auto BlendMapTex = pTextureManage->GetTexture(wstring(mInfo.BlendMapFilename.begin(), mInfo.BlendMapFilename.end()))->Resource;
	D3D12_SHADER_RESOURCE_VIEW_DESC BlendMapDesc = {};
	BlendMapDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	BlendMapDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	BlendMapDesc.Texture2D.MostDetailedMip = 0;
	BlendMapDesc.Texture2D.MipLevels = BlendMapTex->GetDesc().MipLevels;
	BlendMapDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	BlendMapDesc.Format = BlendMapTex->GetDesc().Format;
	mDevice->CreateShaderResourceView(BlendMapTex.Get(), &BlendMapDesc, BlendMapDescriptor);

	//height map
	vector<HALF> HalfHeightMapData;
	HalfHeightMapData.resize(mHeightmap.size());
	std::transform(mHeightmap.begin(), mHeightmap.end(), HalfHeightMapData.begin(), XMConvertFloatToHalf);
	mHeightMapSRV = L3DUtil::CreateDefault2DTexture(mDevice,
		mCommandList, HalfHeightMapData.data(), sizeof(HALF) * mHeightmap.size(), DXGI_FORMAT_R16_FLOAT, mInfo.HeightmapWidth, mInfo.HeightmapHeight, mHeightMapUploader);
	
	D3D12_SHADER_RESOURCE_VIEW_DESC HeightMapDesc = BlendMapDesc;
	HeightMapDesc.Texture2D.MipLevels = 1;
	HeightMapDesc.Format = DXGI_FORMAT_R16_FLOAT;
	mDevice->CreateShaderResourceView(mHeightMapSRV.Get(), &HeightMapDesc, HeightMapDescriptor);
}

void HeightmapTerrain::GetTerrainConstant(TerrainConstants& TerrainConstant)
{
	TerrainConstant.gMinDist = 200.0f;
	TerrainConstant.gMaxDist = 1000.0f;
	TerrainConstant.gMinTess = 0.0f;
	TerrainConstant.gMaxTess = 6.0f;

	TerrainConstant.gTexelCellSpaceU = 1.0f / mInfo.HeightmapWidth;
	TerrainConstant.gTexelCellSpaceV = 1.0f / mInfo.HeightmapHeight;
	TerrainConstant.gWorldCellSpace = mInfo.CellSpacing;
}
