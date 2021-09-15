#pragma once
#include "RanderObject.h"
#include "PipelineState.h"
#include "MeshStruct.h"
using namespace GlareEngine;

class CSky :
    public RanderObject
{
public:
	CSky(ID3D12GraphicsCommandList* CommandList,
		float radius, int sliceCount, int stackCount);

    ~CSky();

    void BuildSkyMesh(ID3D12GraphicsCommandList* CommandList,
        float radius, int sliceCount, int stackCount);

    void BuildSkySRV(ID3D12GraphicsCommandList* CommandList);

    virtual void Draw(GraphicsContext& Context);
    virtual void BuildPSO(const RootSignature& rootSignature);

private:
	//World mat
	XMFLOAT4X4 mWorld = MathHelper::Identity4x4();
    //Sky Cube Mesh
    std::unique_ptr<MeshGeometry> mSkyMesh;
    //Sky SRV Descriptor
    CD3DX12_CPU_DESCRIPTOR_HANDLE m_Descriptor;
    //Sky PSO
    GraphicsPSO mSkyPSO;

};

