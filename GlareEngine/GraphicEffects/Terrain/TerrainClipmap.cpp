#include "TerrainClipmap.h"
#include "Engine/EngineUtility.h"
#include "Engine/Vertex.h"

using namespace DirectX;

TerrainClipmap::TerrainClipmap(
    UINT ClipmapLevels,
    UINT TileSize,
    float CellSizeBase,
    ID3D12Device* Device)
    : mClipmapLevels(ClipmapLevels)
    , mTileSize(TileSize)
    , mCellSizeBase(CellSizeBase)
    , mDevice(Device)
{
    // Create descriptor heap for per-tile resources
    // Each tile needs 3 SRVs + 3 UAVs = 6 descriptors
    // Total tiles: up to 25 per level (5x5) * levels
    UINT maxTiles = 25 * mClipmapLevels;
    mDescriptorHeapSize = maxTiles * 6;

    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.NumDescriptors = mDescriptorHeapSize;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    ThrowIfFailed(mDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&mTileDescriptorHeap)));

    // Initialize clipmap levels
    mLevels.resize(mClipmapLevels);
    for (UINT level = 0; level < mClipmapLevels; level++)
    {
        ClipmapLevel& lvl = mLevels[level];
        // 5x5 grid of tiles per level
        const int tilesPerSide = 5;
        lvl.Tiles.resize(tilesPerSide * tilesPerSide);
        for (int i = 0; i < tilesPerSide * tilesPerSide; i++)
        {
            lvl.Tiles[i] = make_unique<ClipmapTile>();
            lvl.Tiles[i]->LODLevel = level;
        }
    }
}

TerrainClipmap::~TerrainClipmap() = default;

CD3DX12_CPU_DESCRIPTOR_HANDLE TerrainClipmap::AllocateTileDescriptor()
{
    CD3DX12_CPU_DESCRIPTOR_HANDLE handle(
        mTileDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
    handle.Offset(mNextDescriptorIndex,
        mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
    mNextDescriptorIndex++;
    return handle;
}

void TerrainClipmap::CreateTileGPUResources(ClipmapTile* Tile, ID3D12GraphicsCommandList*)
{
    UINT texSize = mTileSize;

    // Height map: R16_FLOAT
    D3D12_RESOURCE_DESC heightDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        DXGI_FORMAT_R16_FLOAT, texSize, texSize, 1, 1);
    heightDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    CD3DX12_HEAP_PROPERTIES defaultHeap(D3D12_HEAP_TYPE_DEFAULT);
    ThrowIfFailed(mDevice->CreateCommittedResource(
        &defaultHeap, D3D12_HEAP_FLAG_NONE, &heightDesc,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        nullptr, IID_PPV_ARGS(&Tile->HeightMap)));

    // Normal map: RG16_FLOAT
    D3D12_RESOURCE_DESC normalDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        DXGI_FORMAT_R16G16_FLOAT, texSize, texSize, 1, 1);
    normalDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    ThrowIfFailed(mDevice->CreateCommittedResource(
        &defaultHeap, D3D12_HEAP_FLAG_NONE, &normalDesc,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        nullptr, IID_PPV_ARGS(&Tile->NormalMap)));

    // Material weight map: RGBA8_UNORM
    D3D12_RESOURCE_DESC weightDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        DXGI_FORMAT_R8G8B8A8_UNORM, texSize, texSize, 1, 1);
    weightDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    ThrowIfFailed(mDevice->CreateCommittedResource(
        &defaultHeap, D3D12_HEAP_FLAG_NONE, &weightDesc,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        nullptr, IID_PPV_ARGS(&Tile->MaterialWeightMap)));

    // Create SRV descriptors
    Tile->HeightSRV = AllocateTileDescriptor();
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Format = DXGI_FORMAT_R16_FLOAT;
    mDevice->CreateShaderResourceView(Tile->HeightMap.Get(), &srvDesc, Tile->HeightSRV);

    Tile->NormalSRV = AllocateTileDescriptor();
    srvDesc.Format = DXGI_FORMAT_R16G16_FLOAT;
    mDevice->CreateShaderResourceView(Tile->NormalMap.Get(), &srvDesc, Tile->NormalSRV);

    Tile->WeightSRV = AllocateTileDescriptor();
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    mDevice->CreateShaderResourceView(Tile->MaterialWeightMap.Get(), &srvDesc, Tile->WeightSRV);

    // Create UAV descriptors
    Tile->HeightUAV = AllocateTileDescriptor();
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    uavDesc.Format = DXGI_FORMAT_R16_FLOAT;
    mDevice->CreateUnorderedAccessView(Tile->HeightMap.Get(), nullptr, &uavDesc, Tile->HeightUAV);

    Tile->NormalUAV = AllocateTileDescriptor();
    uavDesc.Format = DXGI_FORMAT_R16G16_FLOAT;
    mDevice->CreateUnorderedAccessView(Tile->NormalMap.Get(), nullptr, &uavDesc, Tile->NormalUAV);

    Tile->WeightUAV = AllocateTileDescriptor();
    uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    mDevice->CreateUnorderedAccessView(Tile->MaterialWeightMap.Get(), nullptr, &uavDesc, Tile->WeightUAV);

    // Register in global SRV table
    Tile->HeightSRVIndex = AddToGlobalTextureSRVDescriptor(Tile->HeightSRV);
    Tile->NormalSRVIndex = AddToGlobalTextureSRVDescriptor(Tile->NormalSRV);
    Tile->WeightSRVIndex = AddToGlobalTextureSRVDescriptor(Tile->WeightSRV);

    Tile->IsDirty = true;
}

void TerrainClipmap::CreateTileResources(ID3D12GraphicsCommandList* CmdList)
{
    for (auto& level : mLevels)
    {
        for (auto& tile : level.Tiles)
        {
            if (!tile->HeightMap)
            {
                CreateTileGPUResources(tile.get(), CmdList);
            }
        }
    }
}

XMINT2 TerrainClipmap::WorldToGrid(const XMFLOAT3& WorldPos, UINT Level) const
{
    float cellSize = mCellSizeBase * (1u << Level) * mTileSize;
    return XMINT2(
        (int)floorf(WorldPos.x / cellSize),
        (int)floorf(WorldPos.z / cellSize)
    );
}

XMINT2 TerrainClipmap::SnapToGrid(const XMINT2& GridPos) const
{
    return XMINT2(
        GridPos.x >= 0 ? GridPos.x : GridPos.x - 4,
        GridPos.y >= 0 ? GridPos.y : GridPos.y - 4
    );
}

void TerrainClipmap::Update(const XMFLOAT3& CameraPos)
{
    mDirtyTiles.clear();
    mActiveTiles.clear();

    for (UINT level = 0; level < mClipmapLevels; level++)
    {
        XMINT2 newOrigin = WorldToGrid(CameraPos, level);
        ClipmapLevel& lvl = mLevels[level];

        if (newOrigin.x != lvl.CurrentOrigin.x || newOrigin.y != lvl.CurrentOrigin.y || !lvl.Initialized)
        {
            lvl.CurrentOrigin = newOrigin;
            lvl.Initialized = true;
            UpdateActiveTiles(level, newOrigin);
        }

        // Collect active and dirty tiles
        for (auto& tile : lvl.Tiles)
        {
            if (tile->IsActive)
            {
                mActiveTiles.push_back(tile.get());
                if (tile->IsDirty)
                {
                    mDirtyTiles.push_back(tile.get());
                }
            }
        }
    }
}

void TerrainClipmap::UpdateActiveTiles(UINT Level, const XMINT2& NewOrigin)
{
    ClipmapLevel& lvl = mLevels[Level];
    const int tilesPerSide = 5;
    int halfTiles = tilesPerSide / 2;

    for (int dy = -halfTiles; dy <= halfTiles; dy++)
    {
        for (int dx = -halfTiles; dx <= halfTiles; dx++)
        {
            int idx = (dy + halfTiles) * tilesPerSide + (dx + halfTiles);
            auto& tile = lvl.Tiles[idx];

            XMINT2 newCoord = { NewOrigin.x + dx, NewOrigin.y + dy };

            if (tile->GridCoord.x != newCoord.x || tile->GridCoord.y != newCoord.y)
            {
                tile->GridCoord = newCoord;
                tile->IsDirty = true;
            }
            tile->IsActive = true;
        }
    }
}

ClipmapTile* TerrainClipmap::GetTile(UINT Level, XMINT2 GridCoord)
{
    if (Level >= mClipmapLevels) return nullptr;
    for (auto& tile : mLevels[Level].Tiles)
    {
        if (tile->GridCoord.x == GridCoord.x && tile->GridCoord.y == GridCoord.y)
            return tile.get();
    }
    return nullptr;
}

void TerrainClipmap::MarkTileClean(ClipmapTile* Tile)
{
    if (Tile) Tile->IsDirty = false;
}

bool TerrainClipmap::IsCoveredByFinerLevel(const ClipmapTile* Tile) const
{
    if (!Tile || Tile->LODLevel == 0) return false;

    UINT finerLevel = Tile->LODLevel - 1;
    if (finerLevel >= mClipmapLevels) return false;

    const ClipmapLevel& finer = mLevels[finerLevel];
    if (!finer.Initialized) return false;

    // This tile's world-space bounds
    float cellSize = mCellSizeBase * (1u << Tile->LODLevel);
    float tileWorldSize = (float)mTileSize * cellSize;
    float tileMinX = (float)Tile->GridCoord.x * tileWorldSize;
    float tileMaxX = tileMinX + tileWorldSize;
    float tileMinZ = (float)Tile->GridCoord.y * tileWorldSize;
    float tileMaxZ = tileMinZ + tileWorldSize;

    // Finer level's total world-space coverage
    float finerCellSize = mCellSizeBase * (1u << finerLevel);
    float finerTileWorldSize = (float)mTileSize * finerCellSize;
    int halfTiles = 2; // 5/2 = 2 (tiles go from origin-2 to origin+2)
    float finerMinX = ((float)finer.CurrentOrigin.x - (float)halfTiles) * finerTileWorldSize;
    float finerMaxX = ((float)finer.CurrentOrigin.x + (float)(halfTiles + 1)) * finerTileWorldSize;
    float finerMinZ = ((float)finer.CurrentOrigin.y - (float)halfTiles) * finerTileWorldSize;
    float finerMaxZ = ((float)finer.CurrentOrigin.y + (float)(halfTiles + 1)) * finerTileWorldSize;

    // Tile is covered if entirely within finer level's bounds
    return tileMinX >= finerMinX && tileMaxX <= finerMaxX &&
           tileMinZ >= finerMinZ && tileMaxZ <= finerMaxZ;
}

XMINT2 TerrainClipmap::GetLevelOrigin(UINT Level) const
{
    if (Level >= mClipmapLevels) return { 0, 0 };
    return mLevels[Level].CurrentOrigin;
}

bool TerrainClipmap::IsLevelInitialized(UINT Level) const
{
    if (Level >= mClipmapLevels) return false;
    return mLevels[Level].Initialized;
}

void TerrainClipmap::ActivateTilesForLevel(UINT Level, const XMINT2& NewOrigin)
{
    if (Level >= mClipmapLevels) return;
    mLevels[Level].CurrentOrigin = NewOrigin;
    UpdateActiveTiles(Level, NewOrigin);
}
