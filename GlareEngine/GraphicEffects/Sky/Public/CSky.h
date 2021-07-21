#pragma once
#include "RanderObject.h"

class CSky :
    public RanderObject
{
public:
	CSky(ID3D12GraphicsCommandList* CommandList,
		float radius, int sliceCount, int stackCount);

    ~CSky();

    void BuildSkyMesh(ID3D12GraphicsCommandList* CommandList,
        float radius, int sliceCount, int stackCount);

    virtual void Draw();
    virtual void SetPSO();

private:
    //Ìì¿ÕºÐµÄMesh
    std::unique_ptr<MeshGeometry> mSkyMesh;
};

