#pragma once
#include "Misc/RenderObject.h"
#include "Graphics/PipelineState.h"
#include "Animation/MeshStruct.h"
#include "Engine/RenderMaterial.h"

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

    virtual void DrawShadow(GraphicsContext& Context, GraphicsPSO* SpecificShadowPSO = nullptr) {}
    
    virtual void DrawUI();

    virtual void InitMaterial();

    CD3DX12_CPU_DESCRIPTOR_HANDLE GetSkyCubeDescriptor() { return m_Descriptor; }

    void Update(float dt, GraphicsContext* Context = nullptr);
private:
    //sky material
    RenderMaterial* mSkyMaterial = nullptr;
	//World mat
	XMFLOAT4X4 mWorld = MathHelper::Identity4x4();
    //Sky Cube Mesh
    std::unique_ptr<MeshGeometry> mSkyMesh;
    //Sky SRV Descriptor
    CD3DX12_CPU_DESCRIPTOR_HANDLE m_Descriptor;

};

