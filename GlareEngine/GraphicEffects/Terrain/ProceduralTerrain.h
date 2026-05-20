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
    void ComputeSkirtFlags(const ClipmapTile* tile);
    void ComputeSkirtFlagsFor(const ClipmapTile* tile, ProceduralTerrainConstants& outConstants);

    // Subsystems
    unique_ptr<TerrainClipmap>          mClipmap;
    unique_ptr<TerrainNoiseGenerator>   mNoiseGen;

    // Render pipeline
    RootSignature   mTerrainRootSig;
    GraphicsPSO     mTerrainPSO;
    GraphicsPSO     mTerrainDeferredPSO;
    GraphicsPSO     mTerrainShadowPSO;          // Z-prepass (reversed depth, scene depth buffer)
    GraphicsPSO     mTerrainCastShadowPSO;      // Shadow map pass (standard depth, D32 shadow buffer)
    GraphicsPSO     mTerrainPSO_Wireframe;
    GraphicsPSO     mTerrainDeferredPSO_Wireframe;
    GraphicsPSO     mTerrainShadowPSO_Wireframe;

    // Constant buffer (main pass ring buffer)
    ProceduralTerrainConstants mConstants;
    ComPtr<ID3D12Resource>     mConstantBuffer;
    D3D12_GPU_VIRTUAL_ADDRESS  mCBGPU = 0;
    uint8_t*                   mMappedCB = nullptr;

    // Shadow pass constant buffer (separate to avoid ring buffer overwrite by main pass)
    ProceduralTerrainConstants mShadowConstants;
    ComPtr<ID3D12Resource>     mShadowConstantBuffer;
    D3D12_GPU_VIRTUAL_ADDRESS  mShadowCBGPU = 0;
    uint8_t*                   mMappedShadowCB = nullptr;

    // Material layer SRV indices: [layer][albedo, normal, roughness, metallic, AO]
    int mLayerSRVIndices[5][5] = {};
    int mLayerMaterialIndices[5] = {};
    // Per-layer height map SRV indices for height-based blending
    int mLayerHeightSRVIndices[5] = { -1, -1, -1, -1, -1 };

    // Init info
    ProceduralTerrainInitInfo mInitInfo;

    // Cached shadow VP matrix (set by Scene before shadow pass)
    XMFLOAT4X4      mCachedShadowVP = MathHelper::Identity4x4();

    // Shared geometry
    ComPtr<ID3D12Resource> mVertexBuffer;
    ComPtr<ID3D12Resource> mIndexBuffer;
    ComPtr<ID3D12Resource> mVBUploader;
    ComPtr<ID3D12Resource> mIBUploader;
    UINT mIndexCount = 0;
    UINT mVertexStride = 0;

    // ImGui parameters
    float mNoiseScaleUI = 0.003f;
    float mHeightScaleUI = 2000.0f;
    int mHighFreqLayersUI = 2;
    float mSnowHeightUI = 150.0f;
    float mStoneSlopeUI = 0.6f;
    float mTexScaleUI = 200.0f;
    float mRoughnessScaleUI = 9.0f;
    float mStochasticStrengthUI = 0.35f;
    float mMaxTessFactorUI = 4.0f;
    float mMaxTessDistUI = 500.0f;

    // Detail texture parameters
    float mDetailScaleUI = 20.0f;
    float mDetailFadeDistanceUI = 500.0f;
    bool  mUseHeightBlendUI = true;

    // Debug: render only a specific LOD level (-1 = all)
    int mDebugLODLevel = -1;

    // Tracked previous-frame noise params to detect changes and trigger regeneration
    float mPrevNoiseScale = 0.003f;
    float mPrevHeightScale = 2000.0f;
    float mPrevHighFreqLayers = 2.0f;


    // Cached main pass CB data (set by Scene before Draw)
    const void* mMainCBData = nullptr;
    uint32_t mMainCBSize = 0;
public:
    void CacheMainCBData(const void* data, uint32_t size)
    {
        mMainCBData = data;
        mMainCBSize = size;
    }
    void CacheShadowVP(const XMFLOAT4X4& shadowVP) { mCachedShadowVP = shadowVP; }
};
