#pragma once
#include "RenderObject.h"
#include "PipelineState.h"
#include "MeshStruct.h"
using namespace GlareEngine;

class CSky :
    public RenderObject
{
public:
	CSky(ID3D12GraphicsCommandList* CommandList,
		float radius, int sliceCount, int stackCount);

    ~CSky();

    void BuildSkyMesh(ID3D12GraphicsCommandList* CommandList,
        float radius, int sliceCount, int stackCount);

    void BuildSkySRV(ID3D12GraphicsCommandList* CommandList);

    virtual void Draw(GraphicsContext& Context, GraphicsPSO* SpecificPSO = nullptr);

    static void BuildPSO(const PSOCommonProperty CommonProperty);

    CD3DX12_CPU_DESCRIPTOR_HANDLE GetSkyCubeDescriptor() { return m_Descriptor; }
private:
	//PSO
	static GraphicsPSO mPSO;
	//World mat
	XMFLOAT4X4 mWorld = MathHelper::Identity4x4();
    //Sky Cube Mesh
    std::unique_ptr<MeshGeometry> mSkyMesh;
    //Sky SRV Descriptor
    CD3DX12_CPU_DESCRIPTOR_HANDLE m_Descriptor;

};

