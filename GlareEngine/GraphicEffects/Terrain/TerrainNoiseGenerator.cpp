#include "TerrainNoiseGenerator.h"
#include "TerrainClipmap.h"
#include "Engine/EngineUtility.h"
#include "Engine/EngineGlobal.h"

#include "CompiledShaders/TerrainNoiseCS.h"

using namespace DirectX;

TerrainNoiseGenerator::TerrainNoiseGenerator(
    ID3D12Device* Device,
    ID3D12GraphicsCommandList* CmdList,
    const ProceduralTerrainInitInfo& Info)
    : mDevice(Device)
    , mSeed(Info.Seed)
    , mHeightScale(Info.HeightScale)
    , mNoiseScale(Info.NoiseScale)
    , mTileSize(Info.TileSize)
{
    BuildRootSignature(Device);
    BuildPipelineState(Device, CmdList);
    CreateConstantBuffer();
}

TerrainNoiseGenerator::~TerrainNoiseGenerator() = default;

void TerrainNoiseGenerator::BuildRootSignature(ID3D12Device* Device)
{
    mRootSig.Reset(2, 0);
    // [0] CBV - TerrainNoiseCB
    mRootSig[0].InitAsConstantBuffer(0);
    // [1] UAV table - HeightMap, NormalMap, MaterialWeights
    mRootSig[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 3);
    mRootSig.Finalize(L"TerrainNoiseGen", D3D12_ROOT_SIGNATURE_FLAG_NONE);
}

void TerrainNoiseGenerator::BuildPipelineState(ID3D12Device* Device, ID3D12GraphicsCommandList* CmdList)
{
    mComputePSO.SetRootSignature(mRootSig);
    mComputePSO.SetComputeShader(g_pTerrainNoiseCS, sizeof(g_pTerrainNoiseCS));
    mComputePSO.Finalize();
}

void TerrainNoiseGenerator::CreateConstantBuffer()
{
    const UINT cRingBufferSize = 256;
    UINT cbSize = Math::AlignUp(sizeof(ProceduralTerrainNoiseCB), 256u) * cRingBufferSize;

    CD3DX12_HEAP_PROPERTIES uploadHeap(D3D12_HEAP_TYPE_UPLOAD);
    auto desc = CD3DX12_RESOURCE_DESC::Buffer(cbSize);
    ThrowIfFailed(mDevice->CreateCommittedResource(
        &uploadHeap, D3D12_HEAP_FLAG_NONE, &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr, IID_PPV_ARGS(&mConstantBuffer)));

    mConstantBufferGPU = mConstantBuffer->GetGPUVirtualAddress();
}

void TerrainNoiseGenerator::GenerateTiles(
    ComputeContext& Context,
    const vector<ClipmapTile*>& DirtyTiles,
    const XMFLOAT3& CameraPos,
    float BaseCellSize)
{
    if (DirtyTiles.empty()) return;

    Context.SetRootSignature(mRootSig);
    Context.SetPipelineState(mComputePSO);

    const UINT cbAlignedSize = Math::AlignUp(sizeof(ProceduralTerrainNoiseCB), 256u);
    const UINT cRingBufferSize = 256;
    UINT ringIndex = 0;

    for (ClipmapTile* tile : DirtyTiles)
    {
        if (!tile || !tile->HeightMap) continue;

        // Compute actual cell size for this tile's LOD level
        float cellSize = BaseCellSize * (1u << tile->LODLevel);

        // Fill constant buffer
        mNoiseCBData.CameraPosition = CameraPos;
        mNoiseCBData.CellSize = cellSize;
        mNoiseCBData.TileOffset = { tile->GridCoord.x * (int)mTileSize,
                                     tile->GridCoord.y * (int)mTileSize };
        mNoiseCBData.TileSize = mTileSize;
        mNoiseCBData.HeightScale = mHeightScale;
        mNoiseCBData.NoiseScale = mNoiseScale;
        mNoiseCBData.Seed = mSeed;
        mNoiseCBData.Octaves = mOctaves;
        mNoiseCBData.Lacunarity = mLacunarity;
        mNoiseCBData.Persistence = mPersistence;
        mNoiseCBData.WarpStrength = mWarpStrength;
        mNoiseCBData.WarpScale = mWarpScale;
        mNoiseCBData.SnowHeight = mSnowHeight;
        mNoiseCBData.SnowTransition = mSnowTransition;
        mNoiseCBData.StoneSlope = mStoneSlope;
        mNoiseCBData.StoneTransition = mStoneTransition;
        mNoiseCBData.LODLevel = tile->LODLevel;

        // Upload CB to ring buffer slot
        UINT8* pData = nullptr;
        D3D12_RANGE readRange = { 0, 0 };
        D3D12_RANGE writeRange = { ringIndex * cbAlignedSize, (ringIndex + 1) * cbAlignedSize };
        mConstantBuffer->Map(0, &readRange, (void**)&pData);
        memcpy(pData + ringIndex * cbAlignedSize, &mNoiseCBData, sizeof(ProceduralTerrainNoiseCB));
        mConstantBuffer->Unmap(0, &writeRange);

        D3D12_GPU_VIRTUAL_ADDRESS cbAddr = mConstantBufferGPU + ringIndex * cbAlignedSize;
        ringIndex = (ringIndex + 1) % cRingBufferSize;

        // Set root parameters
        Context.SetConstantBuffer(0, cbAddr);

        // Transition resources to UAV (only needed if not already in UAV state)
        if (!tile->NeedsSRVTransition)
        {
            CD3DX12_RESOURCE_BARRIER barriers[3] = {
                CD3DX12_RESOURCE_BARRIER::Transition(
                    tile->HeightMap.Get(),
                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                    D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
                CD3DX12_RESOURCE_BARRIER::Transition(
                    tile->NormalMap.Get(),
                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                    D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
                CD3DX12_RESOURCE_BARRIER::Transition(
                    tile->MaterialWeightMap.Get(),
                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                    D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
            };
            Context.GetCommandList()->ResourceBarrier(3, barriers);
        }
        // Mark that this tile now needs SRV transition when drawn
        tile->NeedsSRVTransition = true;

        // Set UAVs
        Context.SetDynamicDescriptor(1, 0, tile->HeightUAV);
        Context.SetDynamicDescriptor(1, 1, tile->NormalUAV);
        Context.SetDynamicDescriptor(1, 2, tile->WeightUAV);

        // Dispatch: 2D, one thread per texel, numthreads(8,8,1)
        Context.Dispatch2D(mTileSize, mTileSize, 8, 8);
    }

    // UAV barrier to ensure all compute writes complete
    CD3DX12_RESOURCE_BARRIER uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(nullptr);
    Context.GetCommandList()->ResourceBarrier(1, &uavBarrier);

    Context.FlushResourceBarriers();
}
