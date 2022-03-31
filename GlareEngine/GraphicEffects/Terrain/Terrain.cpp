#include "Terrain.h"


GraphicsPSO Terrain::mPSO;

Terrain::Terrain()
{

}


Terrain::~Terrain()
{
}

void Terrain::Update(float dt)
{
}

float Terrain::GetWidth() const
{
	return 0.0f;
}

float Terrain::GetDepth() const
{
	return 0.0f;
}

float Terrain::GetHeight(float x, float z) const
{
	return 0.0f;
}

void Terrain::CreateMaterials()
{
}

void Terrain::LoadHeightMapFromFile(string filename)
{
}

void Terrain::Smooth()
{
}

bool Terrain::InBounds(int i, int j)
{
	return false;
}

float Terrain::Average(int i, int j)
{
	return 0.0f;
}

void Terrain::CalcAllPatchBoundsY()
{
}

void Terrain::CalcPatchBoundsY(UINT i, UINT j)
{
}

void Terrain::BuildQuadPatchGeometry()
{
}
