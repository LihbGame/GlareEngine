#include "HeightmapTerrain.h"

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
	CalcAllPatchBoundsY();


	BuildQuadPatchVB(device);
	BuildQuadPatchIB(device);
	BuildHeightmapSRV(device);


}

HeightmapTerrain::~HeightmapTerrain()
{
}

float HeightmapTerrain::GetWidth() const
{
	return 0.0f;
}

float HeightmapTerrain::GetDepth() const
{
	return 0.0f;
}

float HeightmapTerrain::GetHeight(float x, float z) const
{
	return 0.0f;
}

XMMATRIX HeightmapTerrain::GetWorld() const
{
	return XMMATRIX();
}

void HeightmapTerrain::SetWorld(CXMMATRIX M)
{
}

void HeightmapTerrain::Init()
{
}

void HeightmapTerrain::Draw(ID3D11DeviceContext* dc, const Camera& cam, bool isReflection)
{
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

	for (auto e:AllTerrainTextureNames)
	{
		TerrainTextures[e].push_back(pTextureManage->GetTexture(wstring(e.begin(), e.end())).get());
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
}

void HeightmapTerrain::CalcPatchBoundsY(UINT i, UINT j)
{
}

void HeightmapTerrain::BuildQuadPatchVB(ID3D12Device* device)
{
}

void HeightmapTerrain::BuildQuadPatchIB(ID3D12Device* device)
{
}

void HeightmapTerrain::BuildHeightmapSRV(ID3D12Device* device)
{
}
