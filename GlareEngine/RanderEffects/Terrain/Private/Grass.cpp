#include "Grass.h"
#include "L3DVertex.h"
#include "L3DTextureManage.h"

Grass::Grass(ID3D12Device* device, 
	ID3D12GraphicsCommandList* CommandList,
	L3DTextureManage* TextureManage,
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
	std::vector<L3DVertice::Grass> vertices(mNumVertRows * mNumVertCols);
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
			vertices[i * mNumVertCols + j].Pos = XMFLOAT3(x, 0.0f, z);
			// Stretch texture over grid.
			vertices[i * mNumVertCols + j].Tex.x = j * du;
			vertices[i * mNumVertCols + j].Tex.y = i * dv;
		}
	}

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(L3DVertice::Grass);


	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "GrassGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);


	geo->VertexBufferGPU = L3DUtil::CreateDefaultBuffer(mDevice,
		mCommandList, vertices.data(), vbByteSize, geo->VertexBufferUploader);

	
	geo->VertexByteStride = sizeof(L3DVertice::Grass);
	geo->VertexBufferByteSize = vbByteSize;


	SubmeshGeometry submesh;
	submesh.VertexCount = (UINT)vertices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["Grass"] = submesh;

	mGeometries = std::move(geo);
}