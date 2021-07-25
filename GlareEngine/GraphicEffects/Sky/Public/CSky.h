#pragma once
#include "RanderObject.h"
#include "PipelineState.h"

class CSky :
    public RanderObject
{
public:
	CSky(ID3D12GraphicsCommandList* CommandList,
		float radius, int sliceCount, int stackCount);

    ~CSky();

    void BuildSkyMesh(ID3D12GraphicsCommandList* CommandList,
        float radius, int sliceCount, int stackCount);

    void BuildSkySRV();


    virtual void Draw();
    virtual void BuildPSO(const RootSignature& rootSignature);

private:
    //Sky Cube Mesh
    std::unique_ptr<MeshGeometry> mSkyMesh;
    //Sky SRV Descriptor
    CD3DX12_CPU_DESCRIPTOR_HANDLE* m_pDescriptor=nullptr;
    //Sky PSO
    GraphicsPSO mSkyPSO;
};

