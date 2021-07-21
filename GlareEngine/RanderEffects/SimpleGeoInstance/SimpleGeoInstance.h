#pragma once

#include "EngineUtility.h"
#include "GeometryGenerator.h"
#include "Vertex.h"

class TextureManage;
enum class  SimpleGeometry : int
{
    Box = 0,
    Sphere,
    Cylinder,
    Grid,
    Quad //A quad aligned with the screen.  This is useful for post processing and screen effects.
};

class SimpleGeoInstance
{
public:
    SimpleGeoInstance(ID3D12GraphicsCommandList* pCommandList, ID3D12Device* pd3dDevice, TextureManage* TextureManage);

    ~SimpleGeoInstance();

    std::unique_ptr<MeshGeometry>& GetSimpleGeoMesh(SimpleGeometry GeoType);

    void BuildMaterials();

	void FillSRVDescriptorHeap(int* SRVIndex,
		CD3DX12_CPU_DESCRIPTOR_HANDLE* hDescriptor);

    const std::vector<wstring>& GetTextureNames() { return mPBRTextureName; }
private:

    void BuildSimpleGeometryMesh(SimpleGeometry GeoType);

    ID3D12GraphicsCommandList* pCommandList;
    ID3D12Device* pd3dDevice;
    std::unique_ptr<MeshGeometry> mSimpleMesh;
    std::vector<wstring> mPBRTextureName;
    TextureManage* pTextureManage = nullptr;
};
