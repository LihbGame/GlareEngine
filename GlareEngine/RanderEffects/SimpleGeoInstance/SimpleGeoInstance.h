#pragma once

#include "L3DUtil.h"
#include "L3DGeometryGenerator.h"
#include "L3DVertex.h"


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
    SimpleGeoInstance(ID3D12GraphicsCommandList* pCommandList, ID3D12Device* pd3dDevice);

    ~SimpleGeoInstance();

    std::unique_ptr<MeshGeometry>& GetSimpleGeoMesh(SimpleGeometry GeoType);

    void BuildMaterials();

    const std::vector<wstring>& GetTextureNames() { return mPBRTextureName; }
private:

    void BuildSimpleGeometryMesh(SimpleGeometry GeoType);

    ID3D12GraphicsCommandList* pCommandList;
    ID3D12Device* pd3dDevice;
    std::unique_ptr<MeshGeometry> mSimpleMesh;
    std::vector<wstring> mPBRTextureName;
};
