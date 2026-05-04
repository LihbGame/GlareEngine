#pragma once
#include "Engine/EngineUtility.h"
#include "Graphics/GraphicsCore.h"
#include "Graphics/CommandContext.h"
#include "Graphics/GPUBuffer.h"

// A single tile in the clipmap ring
struct ClipmapTile
{
    XMINT2    GridCoord       = { 0, 0 };     // grid coordinates in this LOD level
    int     LODLevel        = 0;
    bool    IsDirty         = true;          // needs GPU regeneration
    bool    IsActive        = false;
    bool    NeedsSRVTransition = true;       // true after compute generation, cleared after draw transition

    // GPU resources per tile
    ComPtr<ID3D12Resource>  HeightMap;
    ComPtr<ID3D12Resource>  NormalMap;
    ComPtr<ID3D12Resource>  MaterialWeightMap;

    // SRV descriptors
    CD3DX12_CPU_DESCRIPTOR_HANDLE HeightSRV;
    CD3DX12_CPU_DESCRIPTOR_HANDLE NormalSRV;
    CD3DX12_CPU_DESCRIPTOR_HANDLE WeightSRV;

    // UAV descriptors for compute write
    CD3DX12_CPU_DESCRIPTOR_HANDLE HeightUAV;
    CD3DX12_CPU_DESCRIPTOR_HANDLE NormalUAV;
    CD3DX12_CPU_DESCRIPTOR_HANDLE WeightUAV;

    // Global SRV indices for shader access
    int     HeightSRVIndex  = 0;
    int     NormalSRVIndex  = 0;
    int     WeightSRVIndex  = 0;
};

class TerrainClipmap
{
public:
    TerrainClipmap(
        UINT ClipmapLevels,
        UINT TileSize,
        UINT HeightmapSize,
        float CellSizeBase,
        ID3D12Device* Device);

    ~TerrainClipmap();

    void Update(const XMFLOAT3& CameraPos);
    void CreateTileResources(ID3D12GraphicsCommandList* CmdList);

    const vector<ClipmapTile*>& GetDirtyTiles() const { return mDirtyTiles; }
    const vector<ClipmapTile*>& GetActiveTiles() const { return mActiveTiles; }

    void MarkTileClean(ClipmapTile* Tile);
    void ActivateTilesForLevel(UINT Level, const XMINT2& NewOrigin);

    // Returns true if this tile's world-space bounds are entirely
    // inside the next finer level's active tile coverage (should be skipped).
    bool IsCoveredByFinerLevel(const ClipmapTile* Tile) const;

    // Per-frame access
    UINT GetClipmapLevels() const { return mClipmapLevels; }
    UINT GetTileSize() const { return mTileSize; }
    float GetCellSize(UINT Level) const { return mCellSizeBase * (1u << Level); }
    float GetCellSizeBase() const { return mCellSizeBase; }
    XMINT2 GetLevelOrigin(UINT Level) const;
    bool IsLevelInitialized(UINT Level) const;

    // Get tile for a given level and grid coord
    ClipmapTile* GetTile(UINT Level, XMINT2 GridCoord);

private:
    XMINT2 WorldToGrid(const XMFLOAT3& WorldPos, UINT Level) const;
    XMINT2 SnapToGrid(const XMINT2& GridPos) const;
    void UpdateActiveTiles(UINT Level, const XMINT2& NewOrigin);

    UINT mClipmapLevels;
    UINT mTileSize;
    UINT mHeightmapSize;
    float mCellSizeBase;

    ID3D12Device* mDevice = nullptr;

    // Ring buffer of tiles per level
    // Level 0 (finest) to Level N (coarsest)
    struct ClipmapLevel
    {
        XMINT2 CurrentOrigin = { 0, 0 }; // grid origin of this level's center tile
        bool Initialized = false;
        vector<unique_ptr<ClipmapTile>> Tiles; // 5x5 tiles = 25 per level
    };
    vector<ClipmapLevel> mLevels;

    // Tracking
    vector<ClipmapTile*> mDirtyTiles;
    vector<ClipmapTile*> mActiveTiles;

    // Descriptor heap for tile SRV/UAV
    ComPtr<ID3D12DescriptorHeap> mTileDescriptorHeap;
    UINT mDescriptorHeapSize = 0;

    void CreateTileGPUResources(ClipmapTile* Tile, ID3D12GraphicsCommandList* CmdList);
    CD3DX12_CPU_DESCRIPTOR_HANDLE AllocateTileDescriptor();
    UINT mNextDescriptorIndex = 0;
};
