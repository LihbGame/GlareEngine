#include "Grass.h"
#include "L3DVertex.h"
Grass::Grass(ID3D12Device* device, float GrassWidth, float GrassDepth, int VertRows, int VertCols, std::vector<XMFLOAT2>& PatchBoundsY)
:mDevice(device),
mNumVertRows(VertRows),
mNumVertCols(VertCols),
mGrassWidth(GrassWidth),
mGrassDepth(GrassDepth),
mPatchBoundsY(PatchBoundsY)
{

}

Grass::~Grass()
{
}

void Grass::BuildGrassVB()
{
	std::vector<L3DVertice::Grass> patchVertices(mNumVertRows * mNumVertCols);

	float halfWidth = 0.5f * mGrassWidth;
	float halfDepth = 0.5f * mGrassDepth;

	float patchWidth = mGrassWidth / (mNumVertCols - 1);
	float patchDepth = mGrassDepth / (mNumVertRows - 1);
	float du = 1.0f / (mNumVertCols - 1);
	float dv = 1.0f / (mNumVertRows - 1);

	for (UINT i = 0; i < mNumVertRows; ++i)
	{
		float z = halfDepth - i * patchDepth;
		for (UINT j = 0; j < mNumVertCols; ++j)
		{
			float x = -halfWidth + j * patchWidth;
			patchVertices[i * mNumVertCols + j].Pos = XMFLOAT3(x, 0.0f, z);
		}
	}

	// Store axis-aligned bounding box y-bounds in upper-left patch corner.
	for (UINT i = 0; i < mNumVertRows - 1; ++i)
	{
		for (UINT j = 0; j < mNumVertCols - 1; ++j)
		{
			UINT patchID = i * (mNumVertCols - 1) + j;
			patchVertices[i * mNumVertCols + j].BoundsY = mPatchBoundsY[patchID];
		}
	}

	std::vector<L3DVertice::Grass> vertices(patchVertices.size());
	for (size_t i = 0; i < patchVertices.size(); ++i)
	{
		vertices[i].Pos = patchVertices[i].Pos;
		vertices[i].Tex = patchVertices[i].Tex;
	}

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(L3DVertice::Terrain);
}