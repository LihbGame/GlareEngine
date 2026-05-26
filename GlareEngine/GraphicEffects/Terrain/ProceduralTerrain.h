#pragma once
#include <unordered_set>
#include "Misc/RenderObject.h"
#include "Misc/ConstantBuffer.h"
#include "Engine/EngineUtility.h"
#include "Engine/Camera.h"
#include "Graphics/TextureManager.h"
#include "Graphics/RootSignature.h"
#include "Graphics/PipelineState.h"
#include "Graphics/Render.h"
#include "Engine/SceneQuadtreeCulling.h"
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
    void UpdateCulling(GraphicsContext* Context);
    void QueryVisibleTiles(const XMFLOAT4 FrustumPlanes[6], vector<ClipmapTile*>& VisibleTiles);
    const vector<ClipmapTile*>& GetMainRenderTiles() const;
    void UpdateCullingDebugTexture(GraphicsContext* Context);
    void EnsureCullingDebugTextureResources();
    D3D12_CPU_DESCRIPTOR_HANDLE GetCullingDebugSRV() const { return mCullingDebugSRV; }
    bool HasCullingDebugTexture() const { return mCullingDebugTextureValid && mCullingDebugTextureResource != nullptr && mCullingDebugSRV.ptr != D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN; }
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
    float mTessScaleUI = 1.0f;

    // Detail texture parameters
    float mDetailScaleUI = 20.0f;
    float mDetailFadeDistanceUI = 500.0f;
    bool  mUseHeightBlendUI = true;
    bool  mUseParallaxUI = true;
    float mParallaxHeightScaleUI = 0.012f;
    float mParallaxFadeStartUI = 80.0f;
    float mParallaxFadeEndUI = 250.0f;

    // Debug: render only a specific LOD level (-1 = all)
    int mDebugLODLevel = -1;
    bool mEnableQuadtreeCullingUI = true;
    bool mShowCullingDebugTextureUI = true;
    int mCullingMaxDepthUI = 6;
    int mCullingMaxItemsPerLeafUI = 4;
    float mCullingMinHeightUI = -300.0f;
    float mCullingMaxHeightUI = 600.0f;
    float mCullingHeightPaddingUI = 0.0f;

    // Scene-independent quadtree culling data.
    struct CullingTileState
    {
        const ClipmapTile* Tile = nullptr;
        XMINT2 GridCoord = { 0, 0 };
        int LODLevel = 0;
        bool HasHeightMap = false;
    };

    SceneQuadtreeCulling mCullingTree;
    vector<SceneCullingItem> mCullingItems;
    vector<ClipmapTile*> mVisibleTiles;
    vector<const SceneCullingItem*> mVisibleCullingItems;
    unordered_set<const ClipmapTile*> mVisibleTileSet;
    vector<uint32_t> mCullingDebugPixels;
    SceneCullingBounds mCullingWorldBounds;
    ComPtr<ID3D12Resource> mCullingDebugTextureResource;
    D3D12_CPU_DESCRIPTOR_HANDLE mCullingDebugSRV = { D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN };
    bool mCullingDebugTextureValid = false;
    vector<CullingTileState> mPrevCullingTileStates;
    XMFLOAT4 mPrevCullingFrustumPlanes[6] = {};
    bool mHasPrevCullingFrustum = false;
    bool mPrevEnableQuadtreeCullingUI = true;
    bool mPrevShowCullingDebugTextureUI = true;
    int mPrevDebugLODLevel = -1;
    int mPrevCullingMaxDepthUI = 6;
    int mPrevCullingMaxItemsPerLeafUI = 4;
    float mPrevCullingHeightScaleUI = 2000.0f;
    float mPrevCullingMinHeightUI = -300.0f;
    float mPrevCullingMaxHeightUI = 600.0f;
    float mPrevCullingHeightPaddingUI = 0.0f;

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
