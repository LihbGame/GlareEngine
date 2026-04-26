#pragma once
#include "Misc/RenderObject.h"
#include "Misc/ConstantBuffer.h"
#include "Engine/EngineUtility.h"
#include "Engine/Camera.h"
#include "Graphics/TextureManager.h"
#include "Graphics/RootSignature.h"
#include "Graphics/PipelineState.h"
#include "Graphics/Render.h"
#include "TerrainClipmap.h"
#include "TerrainNoiseGenerator.h"

class ProceduralTerrain :
    public RenderObject
{
public:
    ProceduralTerrain(
        ID3D12GraphicsCommandList* CmdList,
        const ProceduralTerrainInitInfo& Info);
    ~ProceduralTerrain();

    void Update(float dt, GraphicsContext* Context = nullptr) override;
    void InitMaterial() override;
    void Draw(GraphicsContext& Context, GraphicsPSO* SpecificPSO = nullptr) override;
    void DrawShadow(GraphicsContext& Context, GraphicsPSO* SpecificShadowPSO = nullptr) override;
    void DrawDepthOnly(GraphicsContext& Context);
    void DrawUI() override;

    float GetHeight(float x, float z) const;

private:
    void BuildRootSignature();
    void BuildPipelineState(ID3D12GraphicsCommandList* CmdList);
    void LoadMaterialTextures(ID3D12GraphicsCommandList* CmdList);
    void UpdateConstantBuffer();

    // Subsystems
    unique_ptr<TerrainClipmap>          mClipmap;
    unique_ptr<TerrainNoiseGenerator>   mNoiseGen;

    // Render pipeline
    RootSignature   mTerrainRootSig;
    GraphicsPSO     mTerrainPSO;
    GraphicsPSO     mTerrainDeferredPSO;
    GraphicsPSO     mTerrainShadowPSO;

    // Constant buffer
    ProceduralTerrainConstants mConstants;
    ComPtr<ID3D12Resource>     mConstantBuffer;
    D3D12_GPU_VIRTUAL_ADDRESS  mCBGPU = 0;

    // Material layer SRV indices: [layer][albedo, normal, roughness, metallic, AO]
    int mLayerSRVIndices[5][5] = {};
    int mLayerMaterialIndices[5] = {};

    // Init info
    ProceduralTerrainInitInfo mInitInfo;

    // Shared geometry
    ComPtr<ID3D12Resource> mVertexBuffer;
    ComPtr<ID3D12Resource> mIndexBuffer;
    ComPtr<ID3D12Resource> mVBUploader;
    ComPtr<ID3D12Resource> mIBUploader;
    UINT mIndexCount = 0;
    UINT mVertexStride = 0;

    // ImGui parameters
    float mNoiseScaleUI = 0.05f;
    float mHeightScaleUI = 200.0f;
    float mWarpStrengthUI = 30.0f;
    float mSnowHeightUI = 150.0f;
    float mStoneSlopeUI = 0.6f;

    // Cached main pass CB data (set by Scene before Draw)
    const void* mMainCBData = nullptr;
    uint32_t mMainCBSize = 0;
public:
    void CacheMainCBData(const void* data, uint32_t size)
    {
        mMainCBData = data;
        mMainCBSize = size;
    }
};
