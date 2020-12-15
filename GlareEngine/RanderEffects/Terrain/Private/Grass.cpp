#include "Grass.h"
#include "L3DVertex.h"
#include "L3DTextureManage.h"

Grass::Grass(ID3D12Device* device, 
	ID3D12GraphicsCommandList* CommandList,
	L3DTextureManage* TextureManage,
	float GrassWidth, float GrassDepth, 
	int VertRows, int VertCols, 
	std::vector<XMFLOAT2>& PatchBoundsY)
:mDevice(device),
mNumVertRows(VertRows),
mNumVertCols(VertCols),
mGrassWidth(GrassWidth),
mGrassDepth(GrassDepth),
mPatchBoundsY(PatchBoundsY),
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
	int NumPacthRows = mNumVertRows - 1;
	int NumPacthCols = mNumVertCols - 1;

	mOffset.resize(NumPacthRows * NumPacthCols);
	mBoundsY.resize(NumPacthRows * NumPacthCols);
	float halfWidth = 0.5f * mGrassWidth;
	float halfDepth = 0.5f * mGrassDepth;

	float patchWidth = mGrassWidth / NumPacthCols;
	float patchDepth = mGrassDepth / NumPacthRows;
	
	for (UINT i = 0; i < NumPacthRows; ++i)
	{
		float z = halfDepth - i * patchDepth;
		for (UINT j = 0; j < NumPacthCols; ++j)
		{
			float x = -halfWidth + j * patchWidth;
			mOffset[i * NumPacthCols + j] = XMFLOAT4(x, 0.0f, z,0.0f);
			mBoundsY[i * NumPacthCols + j]= XMFLOAT4(mPatchBoundsY[i * NumPacthCols + j].x, mPatchBoundsY[i * NumPacthCols + j].y, 0.0f, 0.0f);
		}
	}

	float Cell = mGrassWidth / NumPacthRows;
	static const XMFLOAT3 gPacthVertice[4] =
	{
		XMFLOAT3(0.0f,0.0f,0.0f),
		XMFLOAT3(Cell,0.0f,0.0f),
		XMFLOAT3(0.0f,0.0f,-Cell),
		XMFLOAT3(Cell,0.0f,-Cell)
	};

	std::vector<L3DVertice::Pos> vertices(4);
	for (size_t i = 0; i < 4; ++i)
	{
		vertices[i].Pos = gPacthVertice[i];
	}

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(L3DVertice::Pos);

	std::vector<USHORT> indices(4); // 4 indices per quad face
	
	indices[0] = 0;
	indices[1] = 1;
	indices[2] = 2;
	indices[3] = 3;

	UINT ibByteSize = (UINT)indices.size() * sizeof(USHORT);


	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "GrassGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = L3DUtil::CreateDefaultBuffer(mDevice,
		mCommandList, vertices.data(), vbByteSize, geo->VertexBufferUploader);
	
	geo->IndexBufferGPU = L3DUtil::CreateDefaultBuffer(mDevice,
		mCommandList, indices.data(), ibByteSize, geo->IndexBufferUploader);
	
	
	geo->VertexByteStride = sizeof(L3DVertice::Pos);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;


	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["Grass"] = submesh;

	mGeometries = std::move(geo);
}