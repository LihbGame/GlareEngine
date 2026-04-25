#pragma once
#include "Engine/EngineUtility.h"
#include "Graphics/GraphicsCore.h"
#include "Graphics/CommandContext.h"
#include "Graphics/PipelineState.h"
#include "Graphics/RootSignature.h"
#include "Misc/ConstantBuffer.h"

// Forward declaration
struct ClipmapTile;

class TerrainNoiseGenerator
{
public:
    TerrainNoiseGenerator(
        ID3D12Device* Device,
        ID3D12GraphicsCommandList* CmdList,
        const ProceduralTerrainInitInfo& Info);

    ~TerrainNoiseGenerator();

    // Generate height + normal + material weights for a list of dirty tiles
    void GenerateTiles(
        ComputeContext& Context,
        const vector<ClipmapTile*>& DirtyTiles,
        const XMFLOAT3& CameraPos,
        float BaseCellSize);

    void SetSeed(UINT Seed) { mSeed = Seed; }
    void SetHeightScale(float Scale) { mHeightScale = Scale; }
    void SetNoiseScale(float Scale) { mNoiseScale = Scale; }

    // Get root signature for PSO creation
    RootSignature& GetRootSignature() { return mRootSig; }

private:
    void BuildRootSignature(ID3D12Device* Device);
    void BuildPipelineState(ID3D12Device* Device, ID3D12GraphicsCommandList* CmdList);
    void CreateConstantBuffer();

    ID3D12Device* mDevice = nullptr;

    // Pipeline
    RootSignature    mRootSig;
    ComputePSO       mComputePSO;

    // Constant buffer (uploaded per tile via ring buffer)
    ComPtr<ID3D12Resource>          mConstantBuffer;
    ProceduralTerrainNoiseCB        mNoiseCBData;
    D3D12_GPU_VIRTUAL_ADDRESS       mConstantBufferGPU = 0;

    // Parameters
    UINT    mSeed           = 42;
    float   mHeightScale    = 200.0f;
    float   mNoiseScale     = 0.05f;
    UINT    mOctaves        = 7;
    float   mLacunarity     = 2.0f;
    float   mPersistence    = 0.5f;
    float   mWarpStrength   = 30.0f;
    float   mWarpScale      = 0.02f;
    float   mSnowHeight     = 150.0f;
    float   mSnowTransition = 20.0f;
    float   mStoneSlope     = 0.6f;
    float   mStoneTransition = 0.15f;
    UINT    mTileSize       = 64;
};
