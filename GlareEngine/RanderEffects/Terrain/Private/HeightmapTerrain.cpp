#include "HeightmapTerrain.h"

HeightmapTerrain::HeightmapTerrain(
	ID3D12Device* device, 
	ID3D12GraphicsCommandList* dc, 
	InitInfo& initInfo,
	ID3D12Resource* RandomTexSRV)
{
	XMStoreFloat4x4(&mWorld, XMMatrixIdentity());

	mInfo = initInfo;

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
		AllTerrainTextureNames.push_back(mInfo.LayerMapFilename[i] + "Deffuse");
		AllTerrainTextureNames.push_back(mInfo.LayerMapFilename[i] + "AO");
		AllTerrainTextureNames.push_back(mInfo.LayerMapFilename[i] + "Metallic");
		AllTerrainTextureNames.push_back(mInfo.LayerMapFilename[i] + "Normal");
		AllTerrainTextureNames.push_back(mInfo.LayerMapFilename[i] + "Roughness");
		AllTerrainTextureNames.push_back(mInfo.LayerMapFilename[i] + "Height");
	}

	for (auto e:AllTerrainTextureNames)
	{
		TerrainTextures[e].push_back(pTextureManage->GetTexture(wstring(e.begin(), e.end())).get());
	}
}

void HeightmapTerrain::Smooth()
{
}

bool HeightmapTerrain::InBounds(int i, int j)
{
	return false;
}

float HeightmapTerrain::Average(int i, int j)
{
	return 0.0f;
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
